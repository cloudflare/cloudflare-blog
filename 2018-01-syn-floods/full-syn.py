# Very rough script to recreate conditions of a SYN Flod filling the
# SYN Queue. This is useful to understand which netstat counters are
# responsible for what.

import socket
import os

PORT=1235
BACKLOG=2
SYNCOUNT=100

os.system("sudo ip addr del 10.0.2.16/32 dev lo")
os.system("sudo ip addr add 10.0.2.16/32 dev lo")
os.system("sudo iptables -D INPUT -d 10.0.2.16/32 -j DROP")
os.system("sudo iptables -I INPUT -d 10.0.2.16/32 -j DROP")

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind(('0.0.0.0', PORT))
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

## s.setsockopt(socket.IPPROTO_TCP, socket.TCP_DEFER_ACCEPT, 120)
s.listen(BACKLOG)

# Fill the Accept Queue beforehand with legit handshakes?
if False:
    C = []
    for i in range(BACKLOG+1):
        c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        c.setblocking(False)
        try:
            c.connect(('127.0.0.1', PORT))
        except socket.error:
            pass
        C.append( c )

# SYN Flood
os.system("nstat -r > /dev/null")
os.system("sudo hping3 -q --spoof 10.0.2.16 -i u100 -c %d -S -p %d 127.0.0.1" % (SYNCOUNT, PORT,))
os.system("nstat")

# Check *valid* connection now, with cookies enabled.
if False:
    # TcpExtSyncookiesRecv
    c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    c.setblocking(False)
    try:
        c.connect(('127.0.0.1', PORT))
    except socket.error:
        pass
    os.system("nstat")

# Check *invalid* connection now, with cookies enabled.
if False:
    # TcpExtSyncookiesFailed
    os.system("sudo hping3 -q --spoof 10.0.2.16 -c 1 --ack -p %dT 127.0.0.1" % (PORT,))
    os.system("nstat")
