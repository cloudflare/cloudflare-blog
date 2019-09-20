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

if False:
    c.setsockopt(socket.IPPROTO_TCP, socket.TCP_USER_TIMEOUT, 41*1000)
    c.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    c.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, 3)
    c.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, 1)
    c.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, 11)

time.sleep(0.2)
t0 = time.time()

c.send(b"h"*200)
time.sleep(0.2)
utils.ss(port)

utils.drop_start(dport=port)
utils.drop_start(sport=port)

utils.ss(port)
c.send(b"h"*17)

utils.ss(port)
time.sleep(0.2)
utils.ss(port)
time.sleep(0.4)
utils.ss(port)
time.sleep(1)
utils.ss(port)
utils.check_buffer(c)

poll = select.poll()
poll.register(c, select.POLLIN)
poll.poll()

utils.ss(port)

e = c.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
print("[ ] SO_ERROR = %s" % (e,))

t1 = time.time()
print("[ ] took: %f seconds" % (t1-t0,))
