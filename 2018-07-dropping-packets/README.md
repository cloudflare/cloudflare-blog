How to drop 10 million packets
------------------------------

To recreate the test from the blog post. Build the programs:

    make

You will build programs that listen on UDP socket and drop packets in
various ways:

 * recv-loop: quick recv() loop just receiving and discarding packets
 * recmmsg-loop: recmmsg(16 packets) loop
 * trunc-loop: recvmmsg() but hacked with MSG_TRUNC to avoid copying data to userspace
 * bpf-drop: drop all packets in SO_ATTACH_FILTER
 * ebpf-drop: drop all packets in SO_ATTACH_BPF
 * busypoll-loop: recmmsg() loop but using SO_BUSY_POLL. Faster than pure recmms() version.

And an XDP program:

 * xdp-drop-ebpf.o: matches UDP, destination subnet and dst port and does XDP_DROP


Running these programs is straightforward, but setting infrastructure
may require some commentary.

Prerequisite:

    # verify lack of RAW socket
    sudo ss -A raw,packet_raw -l -p | cat

    # Push all data to RX queue #2
    ethtool -N ext0 flow-type udp4 dst-ip 198.18.0.12 dst-port 1234 action 2
    # Our NIC doens't have hardware flow steering for IPv6, so use mac matching
    ethtool -N ext0 flow-type ether src 24:8a:07:55:44:e8 action 2

    # verify
    ethtool -n ext0

    # disable Turbo Boost
    echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo

    # disable pause frames
    ethtool -A ext0 rx off tx off autoneg off

    # set up ips
    ip route add 198.18.0.0/24 dev vlan100
    ip route add fd00::/64 dev vlan100

    for i in `seq 0 64`; do
        ip addr add 198.18.0.$i dev vlan100
    done

    for i in `seq 0 64`; do
        ip addr add fd00::0.0.0.$i dev vlan100
    done

To clear ethtool flow steering rules:

    sudo ethtool -N ext0 delete `sudo ethtool -n ext0|grep Filter:|cut -d ":" -f 2`


Step 1
----

Setup:

    iptables -I PREROUTING -t mangle -d 198.18.0.0/24 -p udp --dport 1234 -j ACCEPT
    iptables -I PREROUTING -t raw -d 198.18.0.0/24 -p udp --dport 1234 -j ACCEPT
    iptables -I INPUT -t filter -d 198.18.0.0/24 -p udp --dport 1234 -j ACCEPT

    ip6tables -I PREROUTING -t mangle -d fd00::/64 -p udp --dport 1234 -j ACCEPT
    ip6tables -I PREROUTING -t raw -d fd00::/64 -p udp --dport 1234 -j ACCEPT
    ip6tables -I INPUT -t filter -d fd00::/64 -p udp --dport 1234 -j ACCEPT

Cleanup:

    iptables -D PREROUTING -t mangle -d 198.18.0.0/24 -p udp --dport 1234 -j ACCEPT
    iptables -D PREROUTING -t raw -d 198.18.0.0/24 -p udp --dport 1234 -j ACCEPT
    iptables -D INPUT -t filter -d 198.18.0.0/24 -p udp --dport 1234 -j ACCEPT

    ip6tables -D PREROUTING -t mangle -d fd00::/64 -p udp --dport 1234 -j ACCEPT
    ip6tables -D PREROUTING -t raw -d fd00::/64 -p udp --dport 1234 -j ACCEPT
    ip6tables -D INPUT -t filter -d fd00::/64 -p udp --dport 1234 -j ACCEPT

Verify:

    iptables-save | grep 198.18
    ip6tables-save | grep -i fd00

Step 2
----

Setup:

    iptables -I PREROUTING -t mangle -d 198.18.0.0/24 -p udp --dport 1234 -j ACCEPT
    iptables -I PREROUTING -t raw -d 198.18.0.0/24 -p udp --dport 1234 -j NOTRACK
    iptables -I INPUT -t filter -d 198.18.0.0/24 -p udp --dport 1234 -j ACCEPT

    ip6tables -I PREROUTING -t mangle -d fd00::/64 -p udp --dport 1234 -j ACCEPT
    ip6tables -I PREROUTING -t raw -d fd00::/64 -p udp --dport 1234 -j NOTRACK
    ip6tables -I INPUT -t filter -d fd00::/64 -p udp --dport 1234 -j ACCEPT

Cleanup:

    iptables -D PREROUTING -t mangle -d 198.18.0.0/24 -p udp --dport 1234 -j ACCEPT
    iptables -D PREROUTING -t raw -d 198.18.0.0/24 -p udp --dport 1234 -j NOTRACK
    iptables -D INPUT -t filter -d 198.18.0.0/24 -p udp --dport 1234 -j ACCEPT

    ip6tables -D PREROUTING -t mangle -d fd00::/64 -p udp --dport 1234 -j ACCEPT
    ip6tables -D PREROUTING -t raw -d fd00::/64 -p udp --dport 1234 -j NOTRACK
    ip6tables -D INPUT -t filter -d fd00::/64 -p udp --dport 1234 -j ACCEPT

Step 3
----

No additional setup

Step 4
----

Setup:

    iptables -I PREROUTING -t mangle -d 198.18.0.0/24 -p udp --dport 1234 -j ACCEPT
    iptables -I PREROUTING -t raw -d 198.18.0.0/24 -p udp --dport 1234 -j NOTRACK
    iptables -I INPUT -t filter -d 198.18.0.0/24 -p udp --dport 1234 -j DROP

    ip6tables -I PREROUTING -t mangle -d fd00::/64 -p udp --dport 1234 -j ACCEPT
    ip6tables -I PREROUTING -t raw -d fd00::/64 -p udp --dport 1234 -j NOTRACK
    ip6tables -I INPUT -t filter -d fd00::/64 -p udp --dport 1234 -j DROP

Cleanup:

    iptables -D PREROUTING -t mangle -d 198.18.0.0/24 -p udp --dport 1234 -j ACCEPT
    iptables -D PREROUTING -t raw -d 198.18.0.0/24 -p udp --dport 1234 -j NOTRACK
    iptables -D INPUT -t filter -d 198.18.0.0/24 -p udp --dport 1234 -j DROP

    ip6tables -D PREROUTING -t mangle -d fd00::/64 -p udp --dport 1234 -j ACCEPT
    ip6tables -D PREROUTING -t raw -d fd00::/64 -p udp --dport 1234 -j NOTRACK
    ip6tables -D INPUT -t filter -d fd00::/64 -p udp --dport 1234 -j DROP

Step 5
----


Setup:

    iptables -I PREROUTING -t mangle -d 198.18.0.0/24 -p udp --dport 1234 -j ACCEPT
    iptables -I PREROUTING -t raw -d 198.18.0.0/24 -p udp --dport 1234 -j DROP

    ip6tables -I PREROUTING -t mangle -d fd00::/64 -p udp --dport 1234 -j ACCEPT
    ip6tables -I PREROUTING -t raw -d fd00::/64 -p udp --dport 1234 -j DROP

Cleanup:

    iptables -D PREROUTING -t mangle -d 198.18.0.0/24 -p udp --dport 1234 -j ACCEPT
    iptables -D PREROUTING -t raw -d 198.18.0.0/24 -p udp --dport 1234 -j DROP

    ip6tables -D PREROUTING -t mangle -d fd00::/64 -p udp --dport 1234 -j ACCEPT
    ip6tables -D PREROUTING -t raw -d fd00::/64 -p udp --dport 1234 -j DROP


Step 6
------

Setup:

    nft add table netdev filter
    nft -- add chain netdev filter input { type filter hook ingress device vlan100 priority -500 \; policy accept \; }
    nft add rule netdev filter input ip daddr 198.18.0.0/24 udp dport 1234 counter drop
    nft add rule netdev filter input ip6 daddr fd00::/64 udp dport 1234 counter drop

Cleanup:

    nft delete table netdev filter

Verify:

    nft list chains

Step 7
-----

Setup:

    tc qdisc add dev vlan100 ingress
    tc filter add dev vlan100 parent ffff: prio 4 protocol ip u32 match ip protocol 17 0xff match ip dport 1234 0xffff match ip dst 198.18.0.0/24 flowid 1:1 action drop
    tc filter add dev vlan100 parent ffff: protocol ipv6 u32 match ip6 dport 1234 0xffff match ip6 dst fd00::/64 flowid 1:1 action drop

Cleanup:

    tc qdisc del dev vlan100 ingress

Verify

    tc -s filter  show dev vlan100 ingress

Step 8
----

Setup:

    ip link set dev ext0 xdp obj xdp-drop-ebpf.o

Cleanup:

    ip link set dev ext0 xdp off


