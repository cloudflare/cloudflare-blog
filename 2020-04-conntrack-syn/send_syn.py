import logging
logging.getLogger("scapy.runtime").setLevel(logging.ERROR)

from scapy.all import *

tun = TunTapInterface("tun0", mode_tun=True)
tun.open()

for i in range(10000,10000+10):
    ip=IP(src="198.18.0.2", dst="192.0.2.1")
    tcp=TCP(sport=i, dport=80, flags="S")
    send(ip/tcp, verbose=False, inter=0.01, socket=tun)
