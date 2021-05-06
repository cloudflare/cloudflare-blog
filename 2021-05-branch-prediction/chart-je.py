#!/usr/bin/env python3
import subprocess
import shlex
import sys
import os

JJJJ = (("jmp", "-a8 -tjmp"), ("je taken", "-a8 -tje_always_taken"),("jne not-taken", "-a8 -tjne_never_taken"))
TTTT =list(range(128,8192+128,128))

A={}

for j in JJJJ:
    cmd = "./branch %s %s" % (j[1], ' '.join(sys.argv[1:]))
    for t in TTTT:
        p = subprocess.run(
            shlex.split(cmd + ' -c %d' % t),
            stdout=subprocess.PIPE)

        x_min = float("inf")
        for l in p.stdout.split(b'\n'):
            if not l:continue
            x = float(l.decode())
            x_min = min(x_min, x)

        A[(j,t)] = x_min

print("," + (",".join(j[0] for j in JJJJ)))
for t in TTTT:
    print("%d" % t, end="")
    for j in JJJJ:
        if (j,t) in A:
            print(",%.3f" % A[(j,t)], end="")
        else:
            print(",", end="")
    print()
