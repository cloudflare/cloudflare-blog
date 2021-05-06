#!/usr/bin/env python3
import subprocess
import shlex
import sys
import os

JJJJ = (4,8,16,32,64)
TTTT =list(range(128,8192+128,128))


A={}

for j in JJJJ:
    cmd = "./branch -a%d %s" % (j, ' '.join(sys.argv[1:]))
    for t in TTTT:
        if os.environ.get("SKIP") and j*t >= 200000:
            continue
        p = subprocess.run(
            shlex.split(cmd + ' -c%d' % t),
            stdout=subprocess.PIPE)

        x_min = float("inf")
        for l in p.stdout.split(b'\n'):
            if not l:continue
            x = float(l.decode())
            x_min = min(x_min, x)

        A[(j,t)] = x_min


print("," + (",".join(map(str,JJJJ))))
for t in TTTT:
    print("%d" % t, end="")
    for j in JJJJ:
        if (j,t) in A:
            print(",%.3f" % A[(j,t)], end="")
        else:
            print(",", end="")
    print()
