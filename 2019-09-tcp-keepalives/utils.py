import atexit
import ctypes
import io
import os
import shlex
import signal
import socket
import subprocess
import sys
import tcp_info
import time


LIBC = ctypes.CDLL("libc.so.6")

CLONE_NEWNET = 0x40000000
original_net_ns = open("/proc/self/ns/net", 'rb')
if True:
    r = LIBC.unshare(CLONE_NEWNET)
    if r != 0:
        print("[!] Are you root? Need unshare() syscall.")
        sys.exit(-1)
    LIBC.setns(original_net_ns.fileno(), CLONE_NEWNET)

def new_ns():
    r = LIBC.unshare(CLONE_NEWNET)
    if r != 0:
        print("[!] Are you root? Need unshare() syscall.")
        sys.exit(-1)
    os.system("ip link set lo up")

def restore_ns():
    LIBC.setns(original_net_ns.fileno(), CLONE_NEWNET)


def do_iptables(action, sport, dport, extra):
    if sport:
        sport = '--sport %d' % (sport,)
        dport = ''
    else:
        sport = ''
        dport = '--dport %d' % (dport,)
    os.system("iptables -%s INPUT -i lo -p tcp %s %s %s -j DROP" % (action, sport, dport, extra))

def drop_start(sport=None, dport=None, extra=''):
    do_iptables('I', sport, dport, extra)

def drop_stop(sport=None, dport=None, extra=''):
    do_iptables('D', sport, dport, extra)

tcpdump_bin = os.popen('which tcpdump').read().strip()
ss_bin = os.popen('which ss').read().strip()

def tcpdump_start(port):
    p = subprocess.Popen(shlex.split('%s -B 16384 --packet-buffered -n -ttttt -i lo port %s' % (tcpdump_bin, port)))
    time.sleep(1)
    def close():
        p.send_signal(signal.SIGINT)
        p.wait()
    p.close = close
    atexit.register(close)
    return p

def ss(port):
    print(os.popen('%s -t -n -o -a dport = :%s or sport = :%s' % (ss_bin, port, port)).read())

def check_buffer(c):
    ti = tcp_info.from_socket(c)
    print("delivered, acked", ti.tcpi_bytes_acked-1)
    print("in-flight:", ti.tcpi_bytes_sent - ti.tcpi_bytes_retrans- ti.tcpi_bytes_acked+1)
    print("in queue, not in flight:", ti.tcpi_notsent_bytes)

def socket_info(c):
    ti = tcp_info.from_socket(c)
    acked = ti.tcpi_bytes_acked-1
    in_flight = ti.tcpi_bytes_sent - ti.tcpi_bytes_retrans- ti.tcpi_bytes_acked+1
    notsent = ti.tcpi_notsent_bytes
    return acked, in_flight, notsent
