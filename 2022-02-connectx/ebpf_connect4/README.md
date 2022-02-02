BPF_CGROUP_INET4_CONNECT code preventing UDP ESTAB conflicts
------------------------------------------------------------

In UDP there is a problem, with SO_REUSEADDR (which is needed to share
local port numbers), it's possible to have two ESTABLISHED sockets
sharing the same 4-tuple.

With unicast IP's this is confusing, hard to debug, and not what user
typically wants. Only newest socket will receive traffic leaving the
older one rotting. No error is raised. Consider this:

```
a = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
a.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
a.bind(('127.0.0.1', 4444))
a.connect(('127.0.0.1', 1111))

b = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, 0)
b.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
b.bind(('127.0.0.1', 4444))
b.connect(('127.0.0.1', 1111))
```

With that running, `ss` will report:

    $ ss -nau dport = :1111|cat
    State   Recv-Q   Send-Q     Local Address:Port     Peer Address:Port
    ESTAB   0        0              127.0.0.1:4444        127.0.0.1:1111
    ESTAB   0        0              127.0.0.1:4444        127.0.0.1:1111

This is clearly weird.

To counter that you might want to load our BPF_CGROUP_INET4_CONNECT
program. While it's racy and only works when full local IP address and
local port are given to `bind()`, it should fix the problem:

    $ make attach
    bpftool prog load connect4_ebpf.o /sys/fs/bpf/connect4_prog  type cgroup/connect4
    bpftool cgroup attach /sys/fs/cgroup/unified/user.slice connect4 pinned /sys/fs/bpf/connect4_prog

Repeating the python code now gives EPERM error:

    $ python3 test_udp_overshadow.py
    Traceback (most recent call last):
      File "test_udp_overshadow.py", line 12, in <module>
        b.connect(('127.0.0.1', 1111))
    PermissionError: [Errno 1] Operation not permitted

For completeness, to unload the cgroup ebpf:

    $ make detach
    bpftool cgroup detach /sys/fs/cgroup/unified/user.slice connect4 pinned /sys/fs/bpf/connect4_prog
    rm /sys/fs/bpf/connect4_prog
