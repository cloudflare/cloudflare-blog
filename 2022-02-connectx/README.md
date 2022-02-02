Connectx
--------

A proposal for userspace connectx() syscall implementation. Works for
both v4 and v6, TCP and UDP. Doesn't support TCP Fast Open.

The idea is to support:

Vanilla connect():

    connectx(sd, ('0.0.0.0', 0), remote_addr )

Connect with source IP set:

    connectx(sd, ('192.168.1.100', 0), remote_addr )

Connect with source IP and port set:

    connectx(sd, ('192.168.1.100', 9999), remote_addr )


All this without arbitrary limits on total possible connection
count. We want exactly ephemeral-range concurrent connections for one
target. Naive solutions have a limitation of total concurrent count at
most ephemeral-range, since each connection locks a port. We want
better.



Kill Time-Wait
--------------

Sometimes there is a need to drop time-wait sockets. Don't do this,
since it's violating TCP protocol and generally is not
needed. However, sometimes it's useful. Take a look at the killtw.py
script.

    $ echo |nc -q1  1.1.1.1 443
    $ sudo ./killtw.py
    (('192.168.1.83', 49962), ('1.1.1.1', 443))
