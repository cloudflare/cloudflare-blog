import os
import socket

if True:
    sd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sd.bind(('127.0.0.1', 1024))
    sd.listen(10)
    for i in range(3):
        if os.fork () == 0:
            while True:
                cd, _ = sd.accept()
                cd.close()
                print i
    os.wait()
