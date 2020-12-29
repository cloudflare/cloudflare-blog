mmdist - Efficient Euclidean distance in high dimensional spaces
======

Finding a nearest neighbour in a high-dimiensialty spaces is
hard. Here, we have a 144-dimension uint8 coordinates, and a very
large number of points in the space.

This project contains various brute-force algorithm implementations,
each building on the previous one, geared towards solving the nearest
neightbour problem.


In addition to brute-force, take a look at the implentation using
https://github.com/falconn-lib/falconn/ (FALCONN - FAst Lookups of
Cosine and Other Nearest Neighbors) which is an implementation of
state-of-the-art algorithms for the nearest neighbor search problem.

To check Hyperplane FALCONN algorithm, which seems to be the fastest
among different parameters, run:

```
make falconn-hyperplane
```

Number of probes was determined empirically. Smaller values make the
code run faster but with possibility of incorrect results as the
algorithms are approximate.

If you want to play with FALCONN parameters, after the first run
`database.txt.npz` gets created which a saved `npz` NumPy file which
is much faster to load (40x on my laptop):

```
python3 falconnbench.py --numpy-database database.txt.npz --params hyperplane --probes 200 > test-vector.tmp
Reading database 979.753ms
Building index 5245.785ms
Total: 404.490ms, 1536 items, avg 0.263ms per query, 3797.374 qps
Files test-vector.good and test-vector.tmp are identical

real    0m6.905s
user    0m19.777s
sys     0m0.668s
```

Reading `database.txt` the first time:

```
python falconnbench.py --params hyperplane --probes 200 > test-vector.tmp
Reading database 40381.573ms
Building index 10597.627ms
Total: 469.901ms, 1536 items, avg 0.306ms per query, 3268.776 qps
Files test-vector.good and test-vector.tmp are identical

real    0m51.749s
user    1m13.323s
sys     0m4.036s
```
