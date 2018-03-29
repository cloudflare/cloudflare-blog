
Using eBPF on SOCK_STREAM connections
-------------------------------------

This code does two things. First, it uses
`github.com/nathanjsweet/ebpf` golang module to set up an eBPF filter
and map on a SOCK\_STREAM (aka: TCP) socket.

Then it hacks together a custom `net.DialTCP` implementation, which
sets up the eBPF filter in the right place - to allow it capture
SYN+ACK packet.

The result is that we can extract IP TTL values from the return
SYN+ACK packet.

Example run:

    go build -o ttl-ebpf .
    ./ttl-ebpf tcp4://google.com:80 tcp6://google.com:80
    [+] All good. Measured TTL distance to tcp4://google.com:80 216.58.217.206:80 is 6
    [+] All good. Measured TTL distance to tcp6://google.com:80 [2607:f8b0:4007:808::200e]:80 is 5

