Testing TCP Keepalives on Linux
-------------

Various scripts for testing behaviour of SO_KEEPALIVE, TCP_KEEPIDLE,
TCP_KEEPINTVL, TCP_KEEPCNT and TCP_USER_TIMEOUT linux socket toggles.

The scripts are invasive - for example deploy custom `iptables -j
DROP` rules - but run in a dedicated network namespace, so shouldn't
have any persistent side effects.

`tcp-dead.py`

Shows packets retransmissions when user sends data over dead,
previously idle connection.

`tcp-idle.py`

Proves that TCP_USER_TIMEOUT doesn't affect idle socket, until
retransmissions (or keepalives) keep in. Idle socket will remain idle
even with aggressive user-timeout.

`tcp-estab.py`

Proves that TCP_USER_TIMEOUT doesn't affect idle socket, even if first
transmitted packet is lost. In other words, in the case of idle socket
TCP_USER_TIMEOUT measures delay not from last-good packet, but from
the moments the retransmissions begun.

`test-pacing.py`

Using TCP_INFO to recover notsent, in-flight and delivered byte counts.

`test-syn-ack.py`

Testing socket behaviour on third ACK dropped. The caveat is TCP_DEFER_ACCEPT.

`test-syn-recv.py`

Testing socket behaviour on third SYN+ACK dropped.

`test-syn-sent.py`

Testing connect() behaviour on SYN packets dropped.
