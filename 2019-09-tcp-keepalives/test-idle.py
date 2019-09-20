import io
import os
import select
import socket
import time
import utils

utils.new_ns()

port = 1


s = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)

s.bind(('127.0.0.1', port))
s.listen(16)

tcpdump = utils.tcpdump_start(port)
c = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
c.connect(('127.0.0.1', port))

if True:
    c.setsockopt(socket.IPPROTO_TCP, socket.TCP_USER_TIMEOUT, 33500)
    c.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    c.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, 1) # seconds
    c.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, 11) # seconds
    c.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, 111)

t0 = time.time()

utils.drop_start(dport=port)
utils.drop_start(sport=port)

time.sleep(2)
utils.ss(port)

poll = select.poll()
poll.register(c, select.POLLIN)
poll.poll()

utils.ss(port)

e = c.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
print("[ ] SO_ERROR = %s" % (e,))

t1 = time.time()
print("[ ] took: %f seconds" % (t1-t0,))
