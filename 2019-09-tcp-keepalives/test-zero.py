import io
import os
import select
import socket
import time
import utils
import ctypes

utils.new_ns()

port = 1

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
s.bind(('127.0.0.1', port))
s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)

s.listen(16)

tcpdump = utils.tcpdump_start(port)
c = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
c.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
c.connect(('127.0.0.1', port))

x, _ = s.accept()

if False:
    c.setsockopt(socket.IPPROTO_TCP, socket.TCP_USER_TIMEOUT, 30*1000)
    c.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    c.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, 3)
    c.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, 1)
    c.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, 11)

time.sleep(0.2)
print("[ ] c.send()")
import fcntl
TIOCOUTQ=0x5411
c.setblocking(False)
while True:
    bytes_avail = ctypes.c_int()
    fcntl.ioctl(c.fileno(), TIOCOUTQ, bytes_avail)
    if bytes_avail.value > 64*1024:
        break
    try:
        c.send(b"A" * 16384 * 4)
    except io.BlockingIOError:
        break
c.setblocking(True)
time.sleep(0.2)
utils.ss(port)
utils.check_buffer(c)

t0 = time.time()

utils.drop_start(dport=port)
utils.drop_start(sport=port)
time.sleep(2)
utils.ss(port)

time.sleep(10)
utils.check_buffer(c)
utils.ss(port)

poll = select.poll()
poll.register(c, select.POLLIN)
poll.poll()

utils.ss(port)


e = c.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
print("[ ] SO_ERROR = %s" % (e,))

t1 = time.time()
print("[ ] took: %f seconds" % (t1-t0,))
