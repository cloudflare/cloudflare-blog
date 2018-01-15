SystemTap script helping with debugging SYN and Accept queue overflows
-------------------

`resq.stp` prints the true SYN Queue length. This gives similar to
`ss -n state syn-recv`, but is more versatile. Example usage:


```
$ sudo stap -g resq.stp `pidof -s nginx` 5
PID=31905 FD=5
TASK=0xffff881f3a521d40
sk=0xffff882079b34380
reqsk_queue_len=6
reqsk_queue_len=9
...
```


`acceptq.stp` prints the size of both SYN as well as Accept queues
when the Listen Drop occurs. This is very useful for debugging spiky
applications that get stuck only for a short amount of time, but
enough to cause ListenDrop counters to raise. Example usage showing
struggling socket:

```
$ sudo stap -v acceptq.stp
time (us)        acceptq qmax  local addr    remote_addr
1495634198449075  1025   1024  0.0.0.0:6443  10.0.1.92:28585
1495634198449253  1025   1024  0.0.0.0:6443  10.0.1.92:50500
1495634198450062  1025   1024  0.0.0.0:6443  10.0.1.92:65434
```
