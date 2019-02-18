To run the benchmark
-------------------

1. Prepare flow steering to pin RX traffic on right box:

       ethtool -n eth2 | grep Filter: |cut -d " " -f 2|xargs -L1 sudo ethtool -N eth2 delete
       ethtool -N eth2 flow-type tcp4 dst-ip $RIP dst-port 1234  action 2

2. Prepare flow steering to pin RX traffic on left box:

       ethtool -n eth2 | grep Filter: |cut -d " " -f 2|xargs -L1 sudo ethtool -N eth2 delete
       ethtool -N eth2 flow-type tcp4 src-ip $RIP src-port 1234  action 2

3. Disable turbo on both boxes:

       echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo

4. Pin TX traffic to one CPU on both boxes:

       for TX in /sys/class/net/eth2/queues/tx-*/xps_cpus; do
           echo 0  > \$TX
       done
       echo ff,ffffffff > /sys/class/net/eth2/queues/tx-1/xps_cpus

5. Run the tests:

       ssh -n $RIGHT sudo taskset -c 4 ./echo-naive 0.0.0.0:1234 $BUSYPOLL
       ssh -n $LEFT  sudo taskset -c 4 ./test-burst $RIP:1234 $SIZE $ROUNDS

6. Trim bottom 1% of the data:

       SZ=`cat $F/data-$TEST.txt | wc -l`
       # kill bottom 1% of times
       NNP=$[$SZ - $SZ*1 / 100]
       cat $F/data-$TEST.txt|sort -n| head -n $NNP \
           | ~/bin/mmhistogram -t "" \
           | grep "max" \
           | tr := "  " \
           | awk '{ print $6 ", " $10 ", " $2 ", " $8 }'

7. Plot the numbers:


```
cat <<EOF > data.csv
naive, 220.00, 619.01, 157.00, 8637.00
iosubmit, 220.00, 622.39, 158.00, 8647.00
splice, 258.00, 875.19, 204.00, 8878.00
sockmap, 263.00, 802.62, 201.00, 8812.00
EOF
cat <<EOF > data-busypoll.csv
naive, 175.00, 707.85, 152.00, 8729.00
iosubmit, 181.00, 703.33, 153.00, 8667.00
splice, 257.00, 882.49, 204.00, 8871.00
sockmap, 271.00, 834.47, 201.00, 8879.00
EOF

cat <<EOF > data.gnuplot
set title "TCP echo latency for $T blocks"
set terminal pngcairo transparent enhanced linewidth 2 font 'arial,15' size 1000, 600
set xtics nomirror scale 0 offset 4 # rotate by -45
set grid ytics
set ylabel "Latency in us"
set datafile separator ","
set output "numbers-max.png"
set bars 8.0
plot "data.csv" using (\$0+0.5):(\$2):4:5:(\$2+\$3/2):xtic(1) with candlesticks title "normal run",\
     "data-busypoll.csv" using (\$0+0.75):(\$2):4:5:(\$2+\$3/2) with candlesticks title "with BUSYPOLL"
EOF
```
