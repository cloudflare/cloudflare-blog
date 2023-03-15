#!/usr/bin/env drgn

"""
dump_bhash.py - List all TCP bind buckets in the current netns.

Script is not aware of VRF.
"""

import os

from drgn.helpers.linux.list import (
    hlist_for_each,
    hlist_for_each_entry,
)
from drgn.helpers.linux.net import get_net_ns_by_fd
from drgn.helpers.linux.pid import find_task


def dump_bind_bucket(head, net):
    for tb in hlist_for_each_entry("struct inet_bind_bucket", head, "node"):
        # Skip buckets not from this netns
        if tb.ib_net.net != net:
            continue

        port = tb.port.value_()
        fastreuse = tb.fastreuse.value_()
        owners_len = len(list(hlist_for_each(tb.owners)))

        print(
            "{:8d}  {:{sign}9d}  {:7d}".format(
                port,
                fastreuse,
                owners_len,
                sign="+" if fastreuse != 0 else " ",
            )
        )


def get_netns():
    pid = os.getpid()
    task = find_task(prog, pid)
    with open(f"/proc/{pid}/ns/net") as f:
        return get_net_ns_by_fd(task, f.fileno())


def main():
    print("{:8}  {:9}  {:7}".format("TCP-PORT", "FASTREUSE", "#OWNERS"))

    tcp_hashinfo = prog.object("tcp_hashinfo")
    net = get_netns()

    # Iterate over all bhash slots
    for i in range(0, tcp_hashinfo.bhash_size):
        head = tcp_hashinfo.bhash[i].chain
        # Iterate over bind buckets in the slot
        dump_bind_bucket(head, net)


main()
