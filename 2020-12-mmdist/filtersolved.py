import argparse
import sys


def main(params=sys.argv[1:]):
    parser = argparse.ArgumentParser()

    parser.add_argument('-d', '--max-distance',
                        type=int,
                        default=65534,
                        help="max-distance")
    args = parser.parse_args(params)

    for line in sys.stdin:
        dist, _, rest = line.rstrip().partition(" ")
        _, _, dist = dist.partition("=")
        dist = int(dist)
        if dist < args.max_distance:
            print(line.rstrip())
        else:
            print("sq_dist=-1 idx=-1")

if __name__ == '__main__':
    main()
