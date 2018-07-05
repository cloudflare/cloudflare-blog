#!/usr/bin/env python3

import subprocess
import math
import string
import time

vals = []

KEY="rx2_packets"
#KEY="rx2_xdp_drop"

try:
    for i in range(14):
        output = subprocess.check_output("/usr/bin/sudo /sbin/ethtool -S ext0".split())
        for line in output.split('\n'):
            k, _, v = map(string.strip, line.partition(":"))

            try:
                v = float(v)
            except ValueError:
                pass

            if k == KEY:
                vals.append(v)
        # print(".")
        time.sleep(1)
except KeyboardInterrupt:
    pass

# move from absolute to differences - packets per second
deltas = []
for a, b in zip(vals[:-1],vals[1:]):
    deltas.append(b-a)

# kill first and last measurement
deltas = sorted(deltas)[1:-1]

# kill first or last measurement, depending on which one is further from average
avg = float(sum(deltas)) / len(deltas)
if abs(deltas[0]-avg) > abs(deltas[-1]-avg):
    deltas = deltas[1:]
else:
    deltas = deltas[:-1]

avg = float(sum(deltas)) / len(deltas)
sum_err_sq = sum([(v-avg)**2 for v in deltas])
var = sum_err_sq / len(deltas)
dev = math.sqrt(var)

print("%.0f, %.0f" % (avg, dev))
if avg != 0:
    perc = (dev / avg) * 100.0
else:
    perc = 0
print("%.0f (\xc2\xb1%.2f%%)" % (avg, perc))
