#!/bin/env python3
import errno
import os
import socket
import struct

INET_DIAG_NOCOOKIE = b'\xff' * 8
NETLINK_SOCK_DIAG = 4
NLMSG_DONE = 3
NLMSG_ERROR = 2
NLM_F_DUMP = 0x300
NLM_F_REQUEST = 1
SOCK_DIAG_BY_FAMILY = 20
TCP_ESTABLISHED = 1
TCP_TIME_WAIT = 6


def _netlink_tw_lookup(family):
    # NLMsgHdr "length type flags seq pid"
    nl_msg = struct.pack(
        "=LHHLL",
        72,  # length
        SOCK_DIAG_BY_FAMILY,
        NLM_F_REQUEST | NLM_F_DUMP,
        0,
        0)

    # InetDiagReqV2 "family protocol ext states >>id<<"
    req_v2 = struct.pack("=BBBxI", family, socket.IPPROTO_TCP, 0,
                         1 << TCP_TIME_WAIT)

    # InetDiagSockId "sport dport src dst iface cookie"
    # citing kernel: /* src and dst are swapped for historical reasons */
    sock_id = struct.pack("!HH16s16sI8s", 0, 0, b'\x00', b'\x00',
                          socket.htonl(0), INET_DIAG_NOCOOKIE)
    nl = socket.socket(socket.AF_NETLINK, socket.SOCK_RAW, NETLINK_SOCK_DIAG)
    nl.connect((0, 0))

    nl.send(nl_msg + req_v2 + sock_id)
    b = b''
    while True:
        t = 0
        b = b + nl.recv(4096)
        while len(b) > 16:
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
                yield addr
            if t == NLMSG_ERROR:
                (l, t, f, s, p, e) = struct.unpack_from("=LHHLLI", b, 0)
                errno = 0xffffffff + 1 - e
                raise OSError(errno)
            b = b[l:]
            if t == NLMSG_DONE:
                break
        if t == NLMSG_DONE:
            break


TCP_REPAIR = 19
TCP_REPAIR_ON = 1
TCP_REPAIR_OFF = 0
IP_FREEBIND = 15

os.system("ip link add vrf-for-tw-kill type vrf table 100;"
          "ip link set up dev vrf-for-tw-kill;")

for family in (socket.AF_INET, socket.AF_INET6):
    for addr in _netlink_tw_lookup(family):
        print(addr)
        laddr, lport = addr[0][0], addr[0][1]
        raddr, rport = addr[1][0], addr[1][1]
        os.system("ip addr add dev vrf-for-tw-kill %s/32" % (raddr, ))

        sd = socket.socket(family, socket.SOCK_STREAM)
        sd.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                      struct.pack("ii", 1, 0))
        sd.setsockopt(socket.SOL_IP, IP_FREEBIND, 1)
        sd.setsockopt(socket.SOL_TCP, TCP_REPAIR, TCP_REPAIR_ON)

        sd.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, b"vrf-for-tw-kill\x00")
        sd.bind((raddr, rport))
        sd.setsockopt(socket.SOL_TCP, TCP_REPAIR, TCP_REPAIR_OFF)
        fail = False
        sd.setblocking(False)
        try:
            sd.connect((laddr, lport))
        except Exception as e:
            if e.errno != errno.EINPROGRESS:
                print(e)
                fail = True
        sd.close()
        if fail:
            print('fail')
        os.system("ip addr del dev vrf-for-tw-kill %s/32" % (raddr, ))

os.system("ip link del vrf-for-tw-kill")
