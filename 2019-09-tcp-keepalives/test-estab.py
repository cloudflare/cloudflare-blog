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

# user timeout on one socket for 3s
c.setsockopt(socket.IPPROTO_TCP, socket.TCP_USER_TIMEOUT, 3*1000)

# drop packets
utils.drop_start(dport=port)
utils.drop_start(sport=port)

utils.ss(port)
time.sleep(6)
utils.ss(port)
# the point: user-timeout doesn't kick in

c.send(b'hello world')

time.sleep(1)
utils.ss(port)

# utils.drop_stop(dport=port)
# utils.drop_stop(sport=port)
# time.sleep(1)
# utils.ss(port)

poll = select.poll()
poll.register(c, select.POLLIN)
poll.poll()

utils.ss(port)

e = c.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR)
print("[ ] SO_ERROR = %s" % (e,))

t1 = time.time()
print("[ ] took: %f seconds" % (t1-t0,))
