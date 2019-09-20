import io
import os
import select
import socket
import time
import utils
import ctypes
import signal
import atexit

utils.new_ns()

port = 1

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
s.bind(('127.0.0.1', port))
s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)

s.listen(16)

# tcpdump = utils.tcpdump_start(port)

c = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
c.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
c.connect(('127.0.0.1', port))

if False:
    RECV_BYTES_PER_SEC = 200
    x, _ = s.accept()
    child_pid = os.fork()
    x.set_inheritable(True)
    if child_pid == 0:
        c.close()
        recv_bytes = 0
        t0 = time.time()
        while True:
            t1 = time.time()
            bytes_per_sec = recv_bytes / (t1-t0)
            if bytes_per_sec < RECV_BYTES_PER_SEC:
                buf = x.recv(128)
                recv_bytes += len(buf)
            else:
                time.sleep(0.1)
        os.exit(0)

    def kill_child():
        os.kill(child_pid, signal.SIGINT)
        os.waitpid(child_pid, 0)

    atexit.register(kill_child)
else:
    x, _ = s.accept()
    x.shutdown(socket.SHUT_WR)
TCP_NOTSENT_LOWAT = 25

poll_period = 15
while True:
    c.setblocking(False)
    while True:
        try:
            c.send(b"A" * 16384 * 4)
        except io.BlockingIOError:
            break
    c.setblocking(True)
    break

    _, _, notsent1 = utils.socket_info(c)
    print("[s] notsent before=%d" % (notsent1,))
    notsent1_t = time.time()

    poll = select.poll()
    poll.register(c, select.POLLOUT)
    events = poll.poll(poll_period * 1000) # in ms

    _, _, notsent2 = utils.socket_info(c)
    print("[s] notsent after=%d" % (notsent2,))
    notsent2_t = time.time()
    pace = (notsent1 - notsent2) / (notsent2_t - notsent1_t)
    if pace > 90:
        print("[s] pace=%f bytes/sec. OK!" % (pace,))
    else:
        print("[s] pace=%f bytes/sec. connection too slow!" % (pace,))
        break

utils.check_buffer(c)
c.close()
while True:
    utils.ss(port)
    time.sleep(5)

os.kill(child_pid, signal.SIGINT)
