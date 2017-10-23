import os
import select
import socket

# To avoid thundering-herd problem. This requires kernel 4.5+.
EPOLLEXCLUSIVE = 1<<28

if True:
    sd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sd.bind(('127.0.0.1', 1024))
    sd.listen(10)
    sd.setblocking(False)
    for i in range(3):
        if os.fork () == 0:
            ed = select.epoll()
            ed.register(sd, select.EPOLLIN | EPOLLEXCLUSIVE)
            while True:
                try:
                    ed.poll()
                except IOError:
                    continue
                try:
                    cd, _ = sd.accept()
                except socket.error:
                    continue
                cd.close()
                print i
    os.wait()
