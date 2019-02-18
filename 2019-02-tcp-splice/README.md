TCP echo servers testing TCP splicing
-------------------------------------

Here we show couple implementations of naive TCP echo servers,
intended to show benefits of various TCP splicing mechanisms.

echo-naive.c

Shows a naive read/write loop. As simple as it can get.

echo-iosubmit.c

Shows echo server using `io_submit()` on blocking network sockets to
perform the read+write. This allows for one syscall per loop, which
should speed up things slightly.

echo-splice.c

Uses splice(2) syscall. This totally avoids copying the data to
userspace, but still requires two syscalls per loop. It's not obious
what size the splice pipe should have.

echo-sockmap.c

Is using eBPF SOCKMAP infrastructure, and requires root to
work. Should be the fastest since the userspace is never worken up,
but well... it's not that easy.


All programs take second option, that enables SO_BUSYPOLL flag. This
requires root and can reduce the process wakeup time significantly.

Benchmarking
-----------

Finally there is `test-burst.c`. It's a simple program, which sends a
burst of data (like 128KiB, 2MiB, etc) over a TCP connection, waits
for the data to be reflected back and measures the latency of the TCP
echo server.


Example run:

    $ sudo taskset -c 1 ./echo-naive 0.0.0.0:1234
    $ sudo taskset -c 2 ./test-burst 192.168.1.22:1234 2 100

With this setup the test program will send 100 bursts of 2MiB to the
target echo server and report their latency.
