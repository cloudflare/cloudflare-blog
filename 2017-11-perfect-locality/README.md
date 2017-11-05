SystemTap scripts debugging REUSEPORT and packet locality
---------------------------------------------------------


## setcbpf.stp

Sets basic a SO_ATTACH_REUSEPORT_CBPF BPF program to load balance
incoming connections across REUSEPORT group.  Sockets received on N'th
CPU will be passed to N'th member of the REUSEPORT group.

Usage:

    $ sudo stap -g setcbpf.stp <pid> <fd number> <group size>

Where pid and fd number point to specific file descriptor on a
specific process which must be a REUSEPORT bound socket. And group
size specifies the number of sockets in REUSEPORT group.

Example:

    $ sudo stap -vg setcbpf.stp `pidof nginx -s` 3 12
    [+] Pid=29333 fd=3 group_size=12 setsockopt(SO_ATTACH_REUSEPORT_CBPF)=0


## accept.stp

Prints current CPU and the sk_incoming_cpu value from the socket of
the sockets received in accept(). If both match, you have a good
locality of received new connections.

Usage:

    $ sudo stap -g accept.stp <executable name>

Example:

    $ sudo stap -g accept.stp nginx
    cpu=#12 pid=29333 accept(3) -> fd=30 rxcpu=#12
    cpu=#12 pid=29333 accept(3) -> fd=31 rxcpu=#12

This means that an nginx worker pinned to CPU #12 received two
connections, which were delivered to CPU #12 as well.


## locality.stp

Each second prints number of received and transmitted packets (skb's)
among with a percentage of "local" packets. The locality is described
as - skb allocation and free was done on the same CPU.

Usage:

    $ sudo stap -g locality.stp <number of CPU cores>

Where number of CPU cores is a number of non-HT CPU cores. This is
usually the number of RX queues available in the system.

Example:

    $ sudo stap -g locality.stp 12
    rx= 99%  118kpps tx= 99% 115kpps
    rx= 99%  132kpps tx= 99% 129kpps
    rx= 99%  138kpps tx= 99% 136kpps

This indicates about 130k packets per second both received and
transmitted. Both RX and TX locality is high at 99%.
