Conntrack SYN
-------------

To reproduce the first test on Linux:

    virtualenv -p `which python3` venv
    ./venv/bin/pip install scapy
   unshare -Ur -n bash test-1.bash


You should see ten packets delivered to the namespace, ten entries in
conntrack and both iptables counters showing 10 packets. The next step
is to reproduce the situation with full conntrack table. This requires
changing the toggle on the host:

    echo 7 | sudo tee /proc/sys/net/netfilter/nf_conntrack_max
    unshare -Ur -n bash test-1.bash
    echo 262144 | sudo tee /proc/sys/net/netfilter/nf_conntrack_max

Now you should notice 10 packets delivered to the namespace, 7
conntrack entries created, 10 packets delivered to "raw" table, but
only 7 packets delivered to "mangle" iptables table. In "dmesg" you
should see lines like this:

    "nf_conntrack: nf_conntrack: table full, dropping packet"

We showed that conntrack indeed drops packets when creating new entry
is not possible.


Second test shows a slightly more realistic scenario where a SYN flood
hits a legitimate listening socket. We can notice SYN+ACK responses,
potentially created using a fast SYN Cookie mechanism. Again, filled
conntrack table will prevent the SYN packets to reach the listening
socket, and prevent SYN Cookies from engaging.

    echo 7 | sudo tee /proc/sys/net/netfilter/nf_conntrack_max
    unshare -Ur -n bash test-2.bash
    echo 262144 | sudo tee /proc/sys/net/netfilter/nf_conntrack_max
