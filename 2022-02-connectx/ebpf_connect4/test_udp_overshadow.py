import socket
import os

a = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
a.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
a.bind(('127.0.0.1', 4444))
a.connect(('127.0.0.1', 1111))

b = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
b.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
b.bind(('127.0.0.1', 4444))
b.connect(('127.0.0.1', 1111))

os.system('ss -nau dport = :1111|cat')

