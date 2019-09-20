import io
import os
import select
import socket
import time
import utils

utils.new_ns()

port = 1


s = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
s.setsockopt(socket.IPPROTO_TCP, socket.TCP_DEFER_ACCEPT, 100)

if False:
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_USER_TIMEOUT, 5*1000)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, 1)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, 1)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, 1)

s.bind(('127.0.0.1', port))
s.listen(16)

tcpdump = utils.tcpdump_start(port)
c = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)


t0 = time.time()

# For testing lost ACK
#    utils.drop_start(dport=port, extra='--tcp-flags SYN,ACK,FIN,RST,PSH ACK')

c.setblocking(False)
try:
    c.connect(('127.0.0.1', port))
except io.BlockingIOError:
    pass
c.setblocking(True)

utils.ss(port)
time.sleep(2)
utils.ss(port)
time.sleep(2)
utils.ss(port)
time.sleep(2)
utils.ss(port)
time.sleep(2)
utils.ss(port)
print("[x] send")
c.send(b"hello world")
utils.ss(port)
time.sleep(2)
utils.ss(port)
