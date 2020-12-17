import argparse
import binascii
import struct
import sys
import time
import itertools
import random

def distance(a, b, l=144):
    d = 0
    for i in range(l):
        t = a[i] - b[i]
        d += t*t
    return d

def main(params=sys.argv[1:]):
    parser = argparse.ArgumentParser()

    parser.add_argument('-i', '--input',
                        default="database.txt",
                        help="File with hashes")

    empty_h = b'\x00' * 144

    args = parser.parse_args(params)
    H = list()
    with open(args.input, 'rb') as fd:
        for line in fd:
            p = line.split(None, 2)
            h = binascii.a2b_hex(p[0])
            H.append( h )

    for count_per_mut in range(32*2):
        for mut_count in range(8):
            for _ in range(3):
                while True:
                    idx = random.randrange(0, len(H))
                    key_o = H[idx]
                    key_n = list(key_o)
                    for _ in range(mut_count):
                        pos = random.randrange(0,144)
                        key_n[pos] = random.getrandbits(8)
                    key_n = bytes(key_n)
                    if mut_count < 4:
                        if not(distance(key_n, key_o) <= 11000):
                            continue
                    elif mut_count < 8:
                        if not(65400 <= distance(key_n, key_o) <= 65600):
                            continue
                    dist = distance(key_n, key_o)
                    print("%s" % binascii.b2a_hex(key_n).decode())
                    print("sq_dist=%d idx=%d" % (dist, idx), file=sys.stderr)
                    break


if __name__ == '__main__':
    main()
