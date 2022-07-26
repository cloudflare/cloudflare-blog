#!/bin/bash

echo "[+] Staring up"
NS='ip netns exec ns0'

$NS tshark -ni lo -c 6  port 1234 &
TPID="$!"

~marek/bin/bpftrace -q tcp.bt -o bt.txt &
BPID="$!"
sleep 4

function finish {
    echo "[-] Cleaning up"
    kill $BPID
    kill $TPID
    echo
}
trap finish EXIT

TAWS=1
TAWSA="(50%)"

RTT=200
LATENCY=$[$RTT/2]

$NS tc qdisc del dev lo root
$NS tc qdisc add dev lo root netem limit 10000 delay ${LATENCY}ms
$NS sysctl -w net.ipv4.tcp_sack=1

FILL="burst"
#FILL="chunk+512"
DRAIN="none"
RCVBUF=$[2*1024*1024/2]

MSS=1024
$NS sysctl -w net.ipv4.tcp_rmem="4096 $[128*1024] $[6*1024*1024]"
$NS sysctl -w net.ipv4.tcp_adv_win_scale=$TAWS

#PATHATTR="quickack 1"
PATHATTR="$PATHATTR initrwnd 1024"
$NS ip route change local 127.0.0.0/8 dev lo initcwnd 1024 $PATHATTR

# Without taskset we hit TcpExtTCPBacklogCoalesce case
taskset -c 1 $NS python3 window.py $RCVBUF $FILL $DRAIN $MSS

kill $BPID
wait $BPID
cat bt.txt|egrep 'xxx|Lost|@'
sort -n bt.txt |egrep -v 'xxx|Lost|@' |sponge bt.txt

echo ./plot.sh \
    bt.txt \
    \""send=$FILL drain=$DRAIN rtt=${RTT}ms mss=$MSS $PATHATTR"\" \
    out.png \
    \""tcp\\\_adv\\\_win\\\_scale=$TAWS $TAWSA\""

./plot.sh \
    bt.txt \
    \""send=$FILL drain=$DRAIN rtt=${RTT}ms mss=$MSS $PATHATTR"\" \
    out.png \
    \""tcp\\\_adv\\\_win\\\_scale=$TAWS $TAWSA\""

