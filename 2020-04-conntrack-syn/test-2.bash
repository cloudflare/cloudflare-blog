
ip link set lo up
ip tuntap add name tun0 mode tun
ip link set tun0 up
ip addr add 192.0.2.1 peer 192.0.2.2 dev tun0
ip route add 0.0.0.0/0 via 192.0.2.2 dev tun0

iptables -t raw -A PREROUTING -j CT

echo "[*] tcpdump"
nc -k -l 80 &
NC_PID=$!

tcpdump -ni any -B 16384 ip -t 2>/dev/null &
TCPDUMP_PID=$!
function finish_tcpdump {
    kill ${NC_PID}
    wait ${NC_PID} 2>/dev/null
    kill ${TCPDUMP_PID}
    wait
}
trap finish_tcpdump EXIT

sleep 0.3
./venv/bin/python3 send_syn.py

echo "[*] conntrack"
conntrack -L

