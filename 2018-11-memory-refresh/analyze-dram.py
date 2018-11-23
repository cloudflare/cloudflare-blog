import sys
import numpy.fft

# [*] load data
A = []
for i, line in enumerate(sys.stdin):
    try:
        ts, v = map(int, map(str.strip, line.split(",")))
    except ValueError:
        print("# line %i ignored" % (i+1,), file=sys.stderr)
        continue
    A.append( [ts, v] )

# [*] Compute average and cutoff values
def compute_cutoff(X):
    X=list(X)
    Xmin=min(X)
    Xmax=max(X)
    Xavg=sum(X)/len(X)
    Xmed = sorted(X[:])[len(X) // 2]
    print("[*] Input data: min=%i avg=%i med=%i max=%i items=%i" % (Xmin, Xavg, Xmed, Xmax, len(X)))
    return Xavg

# V is just values
V = list(map(lambda a:a[1], A))
Vavg = compute_cutoff(V)

# [*] assert cutoff prooportional to average
cutoffl = Vavg * 1.2
cutoffu = float("+Inf")
print("[*] Cutoff range %.0f-%.0f" % (cutoffl, cutoffu))
lower = len(list(filter(lambda v: v < cutoffl, V)))
higher = len(list(filter(lambda v: v > cutoffu, V)))
print("[ ] %d items below cutoff, %d items above cutoff, %d items non-zero" % (lower, higher, len(V)-lower-higher))

# [*] Rounding / sampling algos
def frange(start, stop, step):
    a = start
    while a < stop:
        yield a
        a += step

def estimate_closest(v1, v2, t1, tc, t2):
    assert(t1 <= tc <= t2)
    if (tc-t1) < (t2-tc):
        v = v1
    else:
        v = v2
    return v

def estimate_largest(v1, v2, t1, tc, t2):
    return max(v1,v2)

def estimate_smallest(v1, v2, t1, tc, t2):
    return min(v1,v2)

def estimate_linear(v1, v2, t1, tc, t2):
    d = (tc-t1) / (t2-t1)
    assert( 0<= d <= 1.0)
    v = v1 + d * (v2 - v1)
    assert((v1 <= v <= v2) or  (v1 >= v >= v2))
    return v

# [*] Perform the sampling
estimate = estimate_linear
UNIT = 100 # in nanoseconds

B = []

fist_ts = (A[0][0] / UNIT) * UNIT + UNIT
last_ts = (A[-1][0] / UNIT) * UNIT

p = 1
for curr_ts in frange(fist_ts, last_ts, UNIT):
    while not(A[p-1][0] <= curr_ts < A[p][0]):
        p += 1
    v1 = 1 if cutoffl <= A[p-1][1] <= cutoffu else 0
    v2 = 1 if cutoffl <= A[p][1] <= cutoffu else 0
    v = estimate(v1, v2, A[p-1][0], curr_ts, A[p][0])
    B.append( v )

# [*] FFT time!
print("[*] Running FFT")
C = numpy.fft.fft(B)
T = numpy.fft.fftfreq(len(B)) * (1000000000/UNIT)

# [*] cutoff spikes in fft
#
# Cheating a bit here. Let's look for threshold/spike by inspecting
# frequencies from 2kHz up to 350khz.
#
lowHz = 2000
highHz = 350000
for i, t in enumerate(T):
    if t > lowHz:
        low = i
        break

for i, t in enumerate(T):
    if t > highHz:
        high = i
        break

D = list(numpy.abs(C))[low:high]
value = max(D)
cutoffl = value / 2
print("[*] Top frequency above %ikHz below %ikHz has magnitude of %d" % (lowHz/1000, highHz / 1000, value))
print("[+] Top frequency spikes above %ikHz are at:" % (lowHz/1000,))
j = 0
for i, (t, d) in enumerate(zip(T, numpy.abs(C))):
    if cutoffl <= abs(d) and t > lowHz:
        print("%dHz\t%d" % (t, abs(d)))
        j += 1
        if j > 10:
            print("...")
            break
