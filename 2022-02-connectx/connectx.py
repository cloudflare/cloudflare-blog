import ctypes
import errno
import os
import random
import socket
import struct

LIBC = ctypes.CDLL("libc.so.6")

IP_BIND_ADDRESS_NO_PORT = 24

SOCK_DIAG_BY_FAMILY = 20
NLM_F_REQUEST = 1
TCP_ESTABLISHED = 1
NETLINK_SOCK_DIAG = 4
NLMSG_ERROR = 2
INET_DIAG_NOCOOKIE = b'\xff' * 8

nl = None
ephemeral_lo = ephemeral_hi = ephemeral_skip = None


def _netlink_udp_lookup(family, local_addr, remote_addr):
    global nl
    # Everyone does NLM_F_REQUEST | NLM_F_DUMP. This triggers socket
    # traversal, but sadly ignores ip addresses in the lookup. The IP
    # stuff must then be expressed with bytecode. To avoid that let's
    # run without F_DUMP which will cause going into udp_dump_one
    # code, which does inspect ip, port, and cookie. Without F_DUMP we
    # also get only a single response.

    # NLMsgHdr "length type flags seq pid"
    nl_msg = struct.pack(
        "=LHHLL",
        72,  # length
        SOCK_DIAG_BY_FAMILY,
        NLM_F_REQUEST,  # notice: no NLM_F_DUMP
        0,
        0)

    # InetDiagReqV2 "family protocol ext states >>id<<"
    req_v2 = struct.pack("=BBBxI", family, socket.IPPROTO_UDP, 0,
                         1 << TCP_ESTABLISHED)

    iface = 0
    if family == socket.AF_INET6 and len(remote_addr) > 3:
        iface = remote_addr[3]

    # InetDiagSockId "sport dport src dst iface cookie"
    # citing kernel: /* src and dst are swapped for historical reasons */
    sock_id = struct.pack("!HH16s16sI8s", remote_addr[1], local_addr[1],
                          socket.inet_pton(family, remote_addr[0]),
                          socket.inet_pton(family, local_addr[0]),
                          socket.htonl(iface), INET_DIAG_NOCOOKIE)
    if nl == None:
        nl = socket.socket(socket.AF_NETLINK, socket.SOCK_RAW,
                           NETLINK_SOCK_DIAG)
        nl.connect((0, 0))

    nl.send(nl_msg + req_v2 + sock_id)
    cookie, addr = None, None
    b = nl.recv(4096)
    (l, t) = struct.unpack_from("=LH", b, 0)
    if t == SOCK_DIAG_BY_FAMILY:
        # `struct nlmsghdr` folllwed by `struct inet_diag_msg`
        (sport, dport, src, dst, iface,
         xcookie) = struct.unpack_from("!HH16s16sI8s", b, 16 + 4)
        if family == socket.AF_INET:
            addr = ((socket.inet_ntop(family, src[:4]), sport),
                    (socket.inet_ntop(family, dst[:4]), dport))
        else:
            addr = ((socket.inet_ntop(family, src), sport),
                    (socket.inet_ntop(family, dst), dport, 0, iface))
        cookie = xcookie
    if t == NLMSG_ERROR:
        (l, t, f, s, p, e) = struct.unpack_from("=LHHLLI", b, 0)
        errno = 0xffffffff + 1 - e
    return cookie, addr


def connectx(sd, local_addr, remote_addr, flags=0):
    family = sd.getsockopt(socket.SOL_SOCKET, socket.SO_DOMAIN)
    sotype = sd.getsockopt(socket.SOL_SOCKET, socket.SO_TYPE)
    if sotype == socket.SOCK_STREAM:
        _connectx_tcp(family, sd, local_addr, remote_addr, flags)
    elif sotype == socket.SOCK_DGRAM:
        _connectx_udp(family, sd, local_addr, remote_addr, flags)


WILDCARD = ('0.0.0.0', '::')


def _connectx_tcp(family, sd, local_addr, remote_addr, flags=0):
    # We want to be able to use the same outbound local port for many
    # users on the system. We totally want to reuse the sport. We
    # don't really "listen". REUSEADDR is needed to allow subsequent
    # bind(*, Y) with our port Y to succeed. Without REUSEADDR the
    # bind (or auto-bind) will skip the port limiting total egress
    # conns. With TCP port-hijacking for REUSEADDR is not an issue.
    # Without REUSEADDR auto-bind over auto-bind would work. But
    # specific-bind over auto-bind would fail.
    sd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    # NO_PORT only makes sense for auto-bind
    if local_addr[1] == 0:
        # For both AF_INET and AF_INET6
        sd.setsockopt(socket.IPPROTO_IP, IP_BIND_ADDRESS_NO_PORT, 1)
    if not (local_addr[0] in WILDCARD and local_addr[1] == 0):
        sd.bind(local_addr)
    sd.connect(remote_addr)


SO_COOKIE = 57


def _connectx_udp(family, sd, local_addr, remote_addr, flags=0):
    if local_addr[0] in WILDCARD:
        # preserve iface for v6
        local_addr = list(local_addr)
        port = local_addr[1]
        local_addr = _get_src_route(family, remote_addr)
        local_addr = list(local_addr)
        local_addr[1] = port
        local_addr = tuple(local_addr)

    if local_addr[1] == 0:
        # Here's the deal. We can't do auto port assignment without
        # REUSEADDR, since we want to share ports with other sockets.
        # We cant do REUSEADDR=1 since it might give us a port number
        # already used for our 4-tuple.
        local_addr = list(local_addr)
        local_addr[1] = _get_udp_port(family, local_addr, remote_addr)
        local_addr = tuple(local_addr)

    cookie = sd.getsockopt(socket.SOL_SOCKET, SO_COOKIE, 8)

    # Before 2-tuple bind we totally must have SO_REUSEADDR
    sd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        sd.bind(local_addr)
    except OSError:
        # bind() might totally fail with EADDRINUSE if there is a
        # socket with SO_REUSEADDR=0, which means locked.
        raise

    # Here we create inconsistent socket state. Acquire lock
    # preventing anyone else from doing 2-tuple bind.
    sd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 0)

    c, _ = _netlink_udp_lookup(family, local_addr, remote_addr)
    if c != cookie:
        # Ideallly dissolve socket association. This is critical
        # section, so ensure the socket is actually cleaned.
        b = struct.pack("I32s", socket.AF_UNSPEC, b"")
        LIBC.connect(sd.fileno(), b, len(b))
        sd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        raise OSError(errno.EADDRINUSE, 'EADDRINUSE')

    # We can continue only if our socket cookie is on top of the
    # lookup. This connect should not fail.
    sd.connect(remote_addr)

    # Exit critical section
    sd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    return True


def changed_namespace():
    global nl
    global ephemeral_lo, ephemeral_hi, ephemeral_skip
    if nl:
        nl.close()
    ephemeral_lo = ephemeral_hi = ephemeral_skip = None
    nl = None


def _get_src_route(family, remote_addr):
    # can be done faster with rtnetlink
    s = socket.socket(family, socket.SOCK_DGRAM, 0)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.connect(remote_addr)
    local_addr = s.getsockname()
    s.close()
    return local_addr


def _read_ephemeral():
    global ephemeral_lo, ephemeral_hi, ephemeral_skip
    with open('/proc/sys/net/ipv4/ip_local_port_range') as f:
        lo, hi = map(int, f.read(512).strip().split())
    skip = set()
    with open('/proc/sys/net/ipv4/ip_local_reserved_ports') as f:
        for port in f.read(512).strip().split(','):
            if '-' in port:
                l, _, h = port.partition('-')
                for c in range(int(l), int(h) + 1):
                    skip.add(c)
            elif port:
                skip.add(int(port))
    ephemeral_lo, ephemeral_hi, ephemeral_skip = (lo, hi, skip)


def _get_udp_port(family, local_addr, remote_addr):
    if ephemeral_lo == None:
        _read_ephemeral()
    lo, hi = ephemeral_lo, ephemeral_hi
    start = random.randint(lo, hi)
    off = 0
    while off < hi + 1 - lo:
        port = start + off
        off += 1
        if port > hi:
            port = port - (hi + 1) + lo
        if port in ephemeral_skip:
            continue
        assert (port >= lo)
        assert (port <= hi)
        c, _ = _netlink_udp_lookup(family, (local_addr[0], port), remote_addr)
        if c is None:
            return port
    raise OSError(errno.EAGAIN, 'EAGAIN')
