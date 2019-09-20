import io
import os
import select
import socket
import time
import utils

utils.new_ns()

port = 1


s = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
# s.setsockopt(socket.IPPROTO_TCP, socket.TCP_DEFER_ACCEPT, 1)

if True:
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_USER_TIMEOUT, 5*1000)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, 1) # two probes
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, 1) # seconds
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, 1) # seconds

s.bind(('127.0.0.1', port))
s.listen(16)

tcpdump = utils.tcpdump_start(port)
c = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)

utils.drop_start(sport=port)

## speedup
# os.system('sysctl net.ipv4.tcp_synack_retries=2')

c.setblocking(False)
try:
    c.connect(('127.0.0.1', port))
except io.BlockingIOError:
    pass
c.setblocking(True)
c.close()

while True:
    utils.ss(port)
    time.sleep(2)
