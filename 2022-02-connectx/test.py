from connectx import connectx
from test_base import LL, TestBase, withLocalPortRange
import errno
import socket
import unittest


class xTests(TestBase):

    def test_share_4_tuple(self):
        '''Two sockets can't share 4-tuple. Two sockets can share local addr
        and local port if destination is differnet.

        For this we need SO_REUSEADDR for TCP.

        (lhost, lport, remote)  --> # 1
        (lhost, lport, remote)  --> # ERR
        (lhost, lport, remote2) --> # 1
        '''
        s1 = self.socket()
        s2 = self.socket()
        s3 = self.socket()
        lport = self.rand_port()
        connectx(s1, (self.localhost, lport), self.remoteA)
        self.assertEcho(s1)
        with self.assertRaises(OSError):
            connectx(s2, (self.localhost, lport), self.remoteA)

        connectx(s3, (self.localhost, lport), self.remoteB)
        self.assertEcho(s3)

    def test_share_4_tuple_wildcard(self):
        ''' Local addr of 0.0.0.0 with port is supported.

        (*, lport, remote)      --> # 1
        (lhost2, lport, remote) --> # 1
        '''
        lport = self.rand_port()
        s1 = self.socket()
        connectx(s1, (self.inaddrany, lport), self.remoteA)
        self.assertEcho(s1)

        s2 = self.socket()
        with self.assertRaises(OSError):
            connectx(s2, (self.inaddrany, lport), self.remoteA)

        # but from another ip it's ok
        s3 = self.socket()
        connectx(s3, (self.another_local, lport), self.remoteA)
        self.assertEcho(s3)

    def test_any_local_port_range(self):
        '''We can establish ephemeral concurrent cons to one dst. Even when
        port is already used by connectx to go somewhere else.

        This should work out of the box since it doesn't need bind-before-connect.

        (*, *, remote)   --> # ephemeral
        (*, *, remote2)  --> # ephemeral
        '''
        S = []
        for i in self.ephemeral_ports():
            sd = self.socket()
            connectx(sd, (self.inaddrany, 0), self.remoteB)
            self.assertEcho(sd)
            S.append(sd)
        self.assertEqual(len(S), len(self.ephemeral_ports()))

        X = []
        for i in self.ephemeral_ports():
            sd = self.socket()
            connectx(sd, (self.inaddrany, 0), self.remoteA)
            self.assertEcho(sd)
            X.append(sd)
        self.assertEqual(len(X), len(self.ephemeral_ports()))

        # but you can't have another socket
        s = self.socket()
        with self.assertRaises(OSError):
            connectx(s, (self.inaddrany, 0), self.remoteA)

        s = self.socket()
        with self.assertRaises(OSError):
            connectx(s, (self.inaddrany, 0), self.remoteB)

    def test_some_local_port_range(self):
        '''We can establish ephemeral concurrent cons to one dst. Even when
        port is already used by connectx to go somewhere else.

        (lhost, *, remote)   --> # ephemeral
        (lhost, *, remote2)  --> # ephemeral

        To get this working we need IP_BIND_ADDRESS_NO_PORT for Tcp.
        '''
        S = []
        for i in self.ephemeral_ports():
            sd = self.socket()
            connectx(sd, (self.localhost, 0), self.remoteB)
            self.assertEcho(sd)
            S.append(sd)
        self.assertEqual(len(S), len(self.ephemeral_ports()))

        X = []
        for i in self.ephemeral_ports():
            sd = self.socket()
            connectx(sd, (self.localhost, 0), self.remoteA)
            self.assertEcho(sd)
            X.append(sd)
        self.assertEqual(len(X), len(self.ephemeral_ports()))

        # but you can't have another socket
        s = self.socket()
        with self.assertRaises(OSError):
            connectx(s, (self.localhost, 0), self.remoteA)

        s = self.socket()
        with self.assertRaises(OSError):
            connectx(s, (self.localhost, 0), self.remoteB)

    @withLocalPortRange(10, 21, "11,14-18,19")
    def test_with_skip_ports(self):
        '''Are the ip_local_reserved_ports adhered to.  TCP behaves funny
        when there are only 3 available ports.

        '''
        S = []
        for i in self.ephemeral_ports():
            sd = self.socket()
            try:
                connectx(sd, (self.localhost, 0), self.remoteB)
                self.assertEcho(sd)
                S.append(sd)
            except OSError:
                pass
        # 10,12,13,20,21
        self.assertEqual(len(S), 5)

    def test_skip_unconnected(self):
        ''' Never connect over unconnected socket. '''
        s = self.socket()
        s.bind((self.inaddrany, 0))
        # listen() for TCP is not be needed.

        S = []
        for i in self.ephemeral_ports():
            sd = self.socket()
            try:
                connectx(sd, (self.inaddrany, 0), self.remoteB)
                self.assertEcho(sd)
                S.append(sd)
            except OSError:
                pass
        self.assertEqual(len(S), len(self.ephemeral_ports()) - 1)

    def test_skip_unconnected_port(self):
        ''' Never connect over unconnected inaddr any socket. '''
        s = self.socket()
        lport = self.rand_port()
        s.bind((self.inaddrany, lport))

        sd = self.socket()
        with self.assertRaises(OSError):
            connectx(sd, (self.localhost, lport), self.remoteA)

        sd = self.socket()
        with self.assertRaises(OSError):
            connectx(sd, (self.inaddrany, lport), self.remoteA)

    def test_skip_unconnected_port_sometimes(self):
        ''' It's fine to connect over unconnected specificic socket. '''
        s = self.socket()
        lport = self.rand_port()
        s.bind((self.localhost, lport))

        sd = self.socket()
        with self.assertRaises(OSError):
            connectx(sd, (self.localhost, lport), self.remoteA)

        # connecting from non-lo should work
        sd = self.socket()
        connectx(sd, (self.another_local, lport), self.remoteA)
        self.assertEcho(sd)

        # connecting from non-lo should work
        sd = self.socket()
        with self.assertRaises(OSError):
            connectx(sd, (self.inaddrany, lport), self.remoteA)

    def test_concurrent_count_tcp_vanilla(self):
        ''' Run connect() against two targets. '''
        if self.socketType != socket.SOCK_STREAM:
            return
        X = []
        for i in self.ephemeral_ports():
            sd = self.socket()
            sd.connect(self.remoteA)
            self.assertEcho(sd)
            X.append(sd)
        self.assertEqual(len(X), len(self.ephemeral_ports()))

        S = []
        for i in self.ephemeral_ports():
            sd = self.socket()
            sd.connect(self.remoteB)
            self.assertEcho(sd)
            S.append(sd)
        self.assertEqual(len(S), len(self.ephemeral_ports()))

        sd = self.socket()
        with self.assertRaises(OSError) as x:
            sd.connect(self.remoteB)
        self.assertEqual(x.exception.errno, errno.EADDRNOTAVAIL)

    def test_concurrent_count_tcp_bind(self):
        ''' Run bind()+connect() against one target. '''
        if self.socketType != socket.SOCK_STREAM:
            return
        X = []
        for i in self.ephemeral_ports():
            sd = self.socket()
            sd.bind((self.inaddrany, 0))
            sd.connect(self.remoteA)
            self.assertEcho(sd)
            X.append(sd)
        self.assertEqual(len(X), len(self.ephemeral_ports()))

        sd = self.socket()
        with self.assertRaises(OSError) as x:
            sd.bind((self.inaddrany, 0))
            sd.connect(self.remoteB)
        self.assertEqual(x.exception.errno, errno.EADDRINUSE)

        sd = self.socket()
        with self.assertRaises(OSError) as x:
            sd.bind((self.localhost, 0))
            sd.connect(self.remoteB)
        self.assertEqual(x.exception.errno, errno.EADDRINUSE)

        # When using another src, it works fine.
        sd = self.socket()
        sd.bind((self.another_local, 0))
        sd.connect(self.remoteB)
        self.assertEcho(sd)

    def test_concurrent_count_tcp_no_port(self):
        ''' . '''
        IP_BIND_ADDRESS_NO_PORT = 24

        if self.socketType != socket.SOCK_STREAM:
            return
        X = []
        for i in self.ephemeral_ports():
            sd = self.socket()
            sd.setsockopt(socket.IPPROTO_IP, IP_BIND_ADDRESS_NO_PORT, 1)
            sd.bind((self.inaddrany, 0))
            sd.connect(self.remoteA)
            self.assertEcho(sd)
            X.append(sd)
        self.assertEqual(len(X), len(self.ephemeral_ports()))

        S = []
        for i in self.ephemeral_ports():
            sd = self.socket()
            sd.setsockopt(socket.IPPROTO_IP, IP_BIND_ADDRESS_NO_PORT, 1)
            sd.bind((self.inaddrany, 0))
            sd.connect(self.remoteB)
            self.assertEcho(sd)
            S.append(sd)
        self.assertEqual(len(S), len(self.ephemeral_ports()))

        sd = self.socket()
        sd.setsockopt(socket.IPPROTO_IP, IP_BIND_ADDRESS_NO_PORT, 1)
        with self.assertRaises(OSError) as x:
            sd.bind((self.inaddrany, 0))
            sd.connect(self.remoteB)
        self.assertEqual(x.exception.errno, errno.EADDRNOTAVAIL)

    def test_concurrent_count_tcp_yes_port(self):
        ''' test errno on REUSEADDR code '''
        if self.socketType != socket.SOCK_STREAM:
            return
        lport = self.rand_port()

        sd = self.socket()
        sd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sd.bind((self.inaddrany, lport))
        sd.connect(self.remoteA)
        self.assertEcho(sd)

        sd2 = self.socket()
        sd2.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sd2.bind((self.inaddrany, lport))
        with self.assertRaises(OSError) as x:
            sd2.connect(self.remoteA)
        self.assertEqual(x.exception.errno, errno.EADDRNOTAVAIL)

    def test_concurrent_count_udp_vanilla(self):
        ''' #ephemeral to first host, EAGAIN later '''
        if self.socketType != socket.SOCK_DGRAM:
            return
        X = []
        for i in self.ephemeral_ports():
            sd = self.socket()
            sd.connect(self.remoteA)
            self.assertEcho(sd)
            X.append(sd)
        self.assertEqual(len(X), len(self.ephemeral_ports()))

        sd = self.socket()
        with self.assertRaises(OSError) as x:
            sd.connect(self.remoteB)
        self.assertEqual(x.exception.errno, errno.EAGAIN)

    def test_concurrent_count_udp_connectx(self):
        ''' '''
        if self.socketType != socket.SOCK_DGRAM:
            return
        X = []
        for i in self.ephemeral_ports():
            sd = self.socket()
            connectx(sd, ('0.0.0.0', 0), self.remoteA)
            self.assertEcho(sd)
            X.append(sd)
        self.assertEqual(len(X), len(self.ephemeral_ports()))

        S = []
        for i in self.ephemeral_ports():
            sd = self.socket()
            connectx(sd, ('0.0.0.0', 0), self.remoteB)
            self.assertEcho(sd)
            S.append(sd)
        self.assertEqual(len(S), len(self.ephemeral_ports()))

        sd = self.socket()
        with self.assertRaises(OSError) as x:
            connectx(sd, ('0.0.0.0', 0), self.remoteB)
        self.assertEqual(x.exception.errno, errno.EAGAIN)


class TestConnectxIP4Base(xTests):
    inaddrany = '0.0.0.0'
    localhost = '127.0.0.1'
    another_local = '127.0.0.14'
    remoteA = ('127.0.0.2', 5353)
    remoteB = ('127.0.0.3', 5353)
    socketFamily = socket.AF_INET


class TestConnectxIP6Base(xTests):
    inaddrany = '::'
    localhost = '::1'
    another_local = '2001:db8::1'
    remoteA = ('2001:db8::2', 5353)
    remoteB = ('2001:db8::3', 5353)
    socketFamily = socket.AF_INET6

    def test_link_local(self):
        S = []
        for i in self.ephemeral_ports():
            sd = self.socket()
            # Src will be link-local
            connectx(sd, (self.inaddrany, 0), (LL, 5353, 0, 2))
            self.assertEcho(sd)
            S.append(sd)
        self.assertEqual(len(S), len(self.ephemeral_ports()))

        X = []
        for i in self.ephemeral_ports():
            sd = self.socket()
            connectx(sd, ('3ffe::100', 0), (LL, 5353, 0, 2))
            self.assertEcho(sd)
            X.append(sd)
        self.assertEqual(len(X), len(self.ephemeral_ports()))


class TestConnectxIP4UDP(TestConnectxIP4Base, unittest.TestCase):
    socketType = socket.SOCK_DGRAM


class TestConnectxIP4TCP(TestConnectxIP4Base, unittest.TestCase):
    socketType = socket.SOCK_STREAM


class TestConnectxIP6UDP(TestConnectxIP6Base, unittest.TestCase):
    socketType = socket.SOCK_DGRAM


class TestConnectxIP6TCP(TestConnectxIP6Base, unittest.TestCase):
    socketType = socket.SOCK_STREAM
