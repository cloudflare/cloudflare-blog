import socket
import os

PORT=1235
BACKLOG=2

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind(('0.0.0.0', PORT))
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
# s.setsockopt(socket.IPPROTO_TCP, socket.TCP_DEFER_ACCEPT, 120)
s.listen(BACKLOG)

# Filling Accept Queue with unhandled connection attempts, to imitate
# slow application.

C = []
for i in range($QSIZE+2):
    if i == $QSIZE+1:
        os.system("nstat -r > /dev/null")
    c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    c.setblocking(False)
    try:
        c.connect(('127.0.0.1', PORT))
    except socket.error:
        pass
    C.append( c )
    if i == $QSIZE+1:
        os.system("nstat")

