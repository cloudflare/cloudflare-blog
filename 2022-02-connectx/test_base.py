from connectx import changed_namespace as cx_changed_namespace
import ctypes
import functools
import os
import random
import select
import socket
import sys
import threading

LIBC = ctypes.CDLL("libc.so.6")
CLONE_NEWNET = 0x40000000

IPV6_FREEBIND = 78

LL = "fe80::602a:900e:beef:dead"


class Server:

    def __init__(self, port):
        strea = socket.socket(socket.AF_INET6, socket.SOCK_STREAM, 0)
        dgram = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, 0)
        strea.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
        dgram.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
        dgram.setsockopt(socket.IPPROTO_IPV6, IPV6_FREEBIND, 1)
        strea.bind(('::', port))
        strea.listen(32)
        dgram.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_RECVPKTINFO, 1)
        dgram.bind(('::', port))
        pr, pw = os.pipe()
        self.pr = os.fdopen(pr)
        self.pw = os.fdopen(pw, 'wb')
        self.dgram = dgram
        self.strea = strea
        self.thread1 = threading.Thread(target=self.run_dgram, args=(dgram, ))
        self.thread1.start()
        self.thread2 = threading.Thread(target=self.run_strea, args=(strea, ))
        self.thread2.start()

    def run_dgram(self, sd):
        while True:
            a, c, b = select.select([sd, self.pr], [], [], None)
            if self.pr in a:
                break
            buf, cmsg, _flags, remote_addr = sd.recvmsg(4096, 1024)
            sd.sendmsg([buf], cmsg, 0, remote_addr)

    def run_strea(self, sd):
        gc = []
        while True:
            a, c, b = select.select([sd, self.pr], [], [], None)
            if self.pr in a:
                break
            cd, _ = sd.accept()
            b = cd.recv(1024)
            cd.send(b)
            gc.append(cd)
        while gc:
            gc.pop().close()

    def stop(self):
        self.pw.write(b'1\r\n')
        self.pw.close()
        self.thread1.join()
        self.thread2.join()
        self.pr.close()
        self.dgram.close()
        self.strea.close()


class TestBase:
    # port range of size 3, like 1-3, is not workig well
    local_port_range = (1, 4)

    def socket(self):
        s = socket.socket(self.socketFamily, self.socketType, 0)
        if self.socketFamily == socket.AF_INET6:
            # freebind needed for anyip on v6. We want that for
            # symmetry with v4 127.0.0.0/8 subnet.
            s.setsockopt(socket.IPPROTO_IPV6, IPV6_FREEBIND, 1)
        self.gc.append(s)
        return s

    def rand_port(self):
        return self.ports.pop()

    def ephemeral_ports(self):
        return list(
            range(self.local_port_range[0], self.local_port_range[1] + 1))

    def setUp(self):
        self.gc = []
        self.prev_net_fd = open("/proc/self/ns/net", 'rb')
        r = LIBC.unshare(CLONE_NEWNET)
        if r != 0:
            print(
                '[!] Are you running within "unshare -Ur" ? Need unshare() syscall.'
            )
            sys.exit(-1)

        # mode tun, since we don't actually plan on anyone reading the other side.
        os.system("ip link set lo up;"
                  "ip tuntap add mode tun name eth0;"
                  "ip link set eth0 mtu 65521;"
                  "ip link set eth0 up;"
                  "ip addr add 192.168.1.100/24 dev eth0;"
                  "ip addr add 3ffe::100/16 dev eth0 nodad;"
                  "ip route add 0.0.0.0/0 via 192.168.1.1 dev eth0;"
                  "ip route add ::/0 via 3ffe::1 dev eth0;"
                  "ip route add local 2001:db8::/64 src ::1 dev lo;"
                  "ip addr add " + LL + "/64 dev eth0;"
                  "sysctl -qw net.ipv4.ip_unprivileged_port_start=0;"
                  "sysctl -qw net.ipv4.ip_local_port_range='%d %d';" %
                  (self.local_port_range[0], self.local_port_range[1]))

        ports = list(
            range(self.local_port_range[0], self.local_port_range[1] + 1))
        ports.reverse()
        self.ports = ports
        self.s = Server(port=5353)
        cx_changed_namespace()

    def tearDown(self):
        while self.gc:
            self.gc.pop().close()
        LIBC.setns(self.prev_net_fd.fileno(), CLONE_NEWNET)
        self.prev_net_fd.close()
        self.s.stop()
        cx_changed_namespace()

    def assertEcho(self, sd):
        p = bytes(str(random.random()), 'utf')
        sd.send(p)
        c = sd.recv(128)
        self.assertEqual(p, c)


def withLocalPortRange(lo, hi, skip=''):

    def decorate(fn):
        fn_name = fn.__name__

        @functools.wraps(fn)
        def maybe(self, *args, **kw):
            os.system("sysctl -qw net.ipv4.ip_local_reserved_ports='%s';"
                      "sysctl -qw net.ipv4.ip_local_port_range='%d %d';" %
                      (skip, lo, hi))
            lpr = self.local_port_range
            self.local_port_range = (lo, hi)
            cx_changed_namespace()
            try:
                ret = fn(self, *args, **kw)
            finally:
                self.local_port_range = lpr
            return ret

        return maybe

    return decorate
