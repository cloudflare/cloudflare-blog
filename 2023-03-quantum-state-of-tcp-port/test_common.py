from os import getpid, system
from socket import socket, SOMAXCONN

from drgn import program_from_kernel
from drgn.helpers.linux.list import (
    hlist_for_each,
    hlist_for_each_entry,
)
from drgn.helpers.linux.net import get_net_ns_by_fd
from drgn.helpers.linux.pid import find_task

IP_BIND_ADDRESS_NO_PORT = 24

DST1 = ("127.8.8.8", 1234)
DST2 = ("127.9.9.9", 1234)


def listener():
    ln = socket()
    ln.bind(("", 1234))
    ln.listen(SOMAXCONN)
    return ln


def bind_ex(s, addr):
    errno = 0
    try:
        s.bind(addr)
    except OSError as e:
        errno = e.errno
    return errno


def setup_netns():
    system("ip link set dev lo up")


def set_local_port_range(port):
    system(f"sysctl -w net.ipv4.ip_local_port_range='{port} {port}'")


prog = program_from_kernel()
tcp_hashinfo = prog.object("tcp_hashinfo")


def walk_bind_bucket_chain(head, net, port):
    for tb in hlist_for_each_entry("struct inet_bind_bucket", head, "node"):
        # Skip buckets not from this netns
        if tb.ib_net.net != net:
            continue
        # Skip buckets not for this port
        if tb.port.value_() != port:
            continue

        return tb


def current_netns():
    pid = getpid()
    task = find_task(prog, pid)
    with open(f"/proc/{pid}/ns/net") as f:
        return get_net_ns_by_fd(task, f.fileno())


def find_bind_bucket(port):
    net = current_netns()
    # Iterate over all bhash slots
    for i in range(0, tcp_hashinfo.bhash_size):
        head = tcp_hashinfo.bhash[i].chain
        bucket = walk_bind_bucket_chain(head, net, port)
        if bucket != None:
            return bucket


def dump_bind_bucket(port):
    tb = find_bind_bucket(port)
    if tb is None:
        print(f"bucket for port {port} not found")
        return

    port = tb.port.value_()
    fastreuse = tb.fastreuse.value_()
    num_owners = len(list(hlist_for_each(tb.owners)))

    print(f"port = {port} fastreuse = {fastreuse} #owners = {num_owners}")

    return fastreuse
