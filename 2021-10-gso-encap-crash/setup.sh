#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail

# Clean up namespaces from last run
ip netns del A || true
ip netns del B || true

# Create namespaces
ip netns add A
ip netns add B

# Enable forwarding between devices in netns B
ip netns exec B sysctl -q -w net.ipv4.conf.all.forwarding=1

# Create a veth pair
ip -n A link add AB type veth peer name BA netns B
ip -n A addr add dev AB 10.1.1.1/24
ip -n B addr add dev BA 10.1.1.2/24

# Create an egress sink
ip -n B link add sink type dummy
ip -n B addr add dev sink 10.2.2.1/24

# Bring all links up
ip -n A link set dev AB up
ip -n A link set dev lo up
ip -n B link set dev BA up
ip -n B link set dev lo up
ip -n B link set dev sink up

# Attach a dummy XDP prog on ingress to B
ip -n B link set BA xdp obj bpf/xdp_pass.o

# Route to 10.2.2.2 via AB/BA
ip -n A route add 10.2.2.0/24 via 10.1.1.2

if [[ "${1:-}" == "--with-delay" ]]; then
  # Help GRO
  ip netns exec A tc qdisc add dev AB root netem delay 200us slot 5ms 10ms packets 2 bytes 64k
fi
