#!/bin/bash

sysctl -w net.ipv4.ip_forward=1
sysctl -w net.ipv6.conf.all.forwarding=1

echo "[-] Setting up namespace"
mkdir /etc/netns/ns0 -p
echo "nameserver 1.1.1.1" > /etc/netns/ns0/resolv.conf

sysctl net.core.rmem_max=$[1024*1024*32]

ip netns add ns0
ip link add veth1 type veth peer name veth2 netns ns0
ip l set dev veth1 up
iptables -t nat -I POSTROUTING --src 192.168.99.0/24 -j MASQUERADE
ip6tables -t nat -I POSTROUTING --src ad00:ffff::/64 -j MASQUERADE
ip add add dev veth1 192.168.99.1/24
ip -6 add add dev veth1 ad00:ffff::1/64

NS='ip netns exec ns0'

$NS sysctl -w net.ipv4.tcp_timestamps=0
$NS sysctl -w net.ipv4.tcp_no_metrics_save=1

$NS ip l set lo up
$NS ip l set dev veth2 up
$NS ip add add dev veth2 192.168.99.2/24
$NS ip -6 add add dev veth2 ad00:ffff::2/64
$NS ip r add default via 192.168.99.1 dev veth2
$NS ip -6 r add default via ad00:ffff::1 dev veth2

# We don't want GSO and friends
$NS ethtool -K lo tso off gro off gso off

# Path attributes are useful - initrwnd 1000; quickack 1
$NS sudo ip route change local 127.0.0.0/8 dev lo initcwnd 1024
