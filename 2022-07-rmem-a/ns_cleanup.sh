#!/bin/bash
echo "[-] Cleaning up namespace"
iptables -t nat -D POSTROUTING --src 192.168.99.0/24 -j MASQUERADE
ip6tables -t nat -D POSTROUTING --src ad00:ffff::/64 -j MASQUERADE
ip netns del ns0
sync
sleep 1
