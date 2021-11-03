#!/usr/bin/env python3
#
# send-a-pair.py - Send two consequtive TCP segments for GRO to merge.
#

from scapy.all import *

SEG_LEN = 1500 - 20 - 4 - 20 - 20 # MTU - IPv4 - GRE - IPv4 - TCP

OUTER_SRC='10.1.1.1'
OUTER_DST='10.2.2.2'

INNER_SRC='192.168.1.1'
INNER_DST='192.168.2.2'

p1 = IP(dst=OUTER_DST,
        src=OUTER_SRC,
        id=1)/GRE()/IP(dst=INNER_DST,
                       src=INNER_SRC,
                       id=1)/TCP(sport=12345,
                                 dport=443,
                                 flags='A',
                                 seq=(0 * SEG_LEN))/('A' * SEG_LEN)
p2 = IP(dst=OUTER_DST,
        src=OUTER_SRC,
        id=2)/GRE()/IP(src=INNER_SRC,
                       dst=INNER_DST,
                       id=2)/TCP(sport=12345,
                                 dport=443,
                                 flags='A',
                                 seq=(1 * SEG_LEN))/('B' * SEG_LEN)
send([p1, p2], verbose=False)
