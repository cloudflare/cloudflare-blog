Accept vs Epoll de-queueing order
---------------------------------

The experiment showing the FIFO vs LIFO-like behavior of two ways of
waiting for new connections on the shared accept queue. The balanced
behavior of blocking-accept:

    $ python blocking-accept.py &
    $ for i in `seq 6`; do nc localhost 1024; done
    2
    1
    0
    2
    1
    0

The unbalanced LIFO-like behavior of blocking-EPOLLEXCLUSIVE-epoll
and nonblocking-accept setup:

    $ python epoll-and-accept.py &
    $ for i in `seq 6`; do nc localhost 1024; done
    0
    0
    0
    0
    0
    0


Latency of requests in shared-queue vs reuseport nginx setup
------------------------------------------------------------

In this experiment we try to show that even though the total work is
the same, the latency distribution will suffer in high-load
SO_REUSEPORT setup.

In this setup we need to pretend that there is some CPU-intensive work
done by nginx. We do that by running a stupid loop inside request
handler:

    i = 0
    for names = 1, 1000000 do
      i = i + 1
    end
    ngx.say("<p>hello, world</p>")


Note that we also run 12 nginx workers, and assume 24 logical cpus.
To reproduce the shared-queue latency graph, run nginx.

    $ nginx -c nginx-shared-queue.conf -p $PWD

We can confirm the CPU pinning and that indeed we have one shared
accept queue:

    $ for pid in $(pgrep nginx); do taskset -cp $pid; done
    pid 9366's current affinity list: 0-23
    pid 9367's current affinity list: 12
    pid 9369's current affinity list: 13
    pid 9370's current affinity list: 14
    pid 9371's current affinity list: 15
    pid 9372's current affinity list: 16
    pid 9373's current affinity list: 17
    pid 9374's current affinity list: 18
    pid 9375's current affinity list: 19
    pid 9376's current affinity list: 20
    pid 9377's current affinity list: 21
    pid 9378's current affinity list: 22
    pid 9379's current affinity list: 23

    $ ss -4nl -t 'sport = :8181' | cat
    State      Recv-Q Send-Q        Local Address:Port          Peer Address:Port
    LISTEN     0      511                       *:8181                     *:*

Now run the benchmark program from another server:

    $ go build benchhttp.go
    $ ./benchhttp -n 100000 -c 200 -r target:8181 http://a.a/ | cut -d " " -f 1 | ./mmhistogram -t "Duration in ms (shared queue)"
    Duration in ms (shared queue) min:3.61 avg:30.39 med=30.28 max:72.65 dev:1.58 count:100000
    Duration in ms (shared queue):
     value |-------------------------------------------------- count
         0 |                                                   0
         1 |                                                   0
         2 |                                                   1
         4 |                                                   16
         8 |                                                   67
        16 |************************************************** 91760
        32 |                                              **** 8155
        64 |                                                   1

The second experiment, to check the SO_REUSEPORT multi accept
queue latency distribution:

    $ nginx -c nginx-shared-queue.conf -p $PWD

    $ ss -4nl -t 'sport = :8181' | cat
    State      Recv-Q Send-Q        Local Address:Port          Peer Address:Port
    LISTEN     0      511                       *:8181                     *:*
    LISTEN     0      511                       *:8181                     *:*
    LISTEN     0      511                       *:8181                     *:*
    LISTEN     0      511                       *:8181                     *:*
    LISTEN     0      511                       *:8181                     *:*
    LISTEN     0      511                       *:8181                     *:*
    LISTEN     0      511                       *:8181                     *:*
    LISTEN     0      511                       *:8181                     *:*
    LISTEN     0      511                       *:8181                     *:*
    LISTEN     0      511                       *:8181                     *:*
    LISTEN     0      511                       *:8181                     *:*
    LISTEN     0      511                       *:8181                     *:*

The benchmark reuseport multiple queues benchmark:

    $ ./benchhttp -n 100000 -c 200 -r target:8181 http://a.a/ | cut -d " " -f 1 | ./mmhistogram -t "Duration in ms (multiple queues)"
    Duration in ms (multiple queues) min:1.49 avg:31.37 med=24.67 max:144.55 dev:25.27 count:100000
    Duration in ms (multiple queues):
     value |-------------------------------------------------- count
         0 |                                                   0
         1 |                                                 * 1023
         2 |                                         ********* 5321
         4 |                                 ***************** 9986
         8 |                  ******************************** 18443
        16 |    ********************************************** 25852
        32 |************************************************** 27949
        64 |                              ******************** 11368
       128 |                                                   58
