#!/usr/bin/env python

import sys
import time
import subprocess
import datetime
import re


digits_re = re.compile("([0-9eE.+]*)")
to = 2.0
CLS='\033[2J\033[;H'
digit_chars = set('0123456789.')


def isfloat(v):
    try:
        float(v)
    except ValueError:
        return False
    return True

def total_seconds(td):
    return (td.microseconds + (td.seconds + td.days * 24. * 3600) * 10**6) / 10**6

def main(cmd):
    prevp = []
    prevt = None

    while True:
        t0 = datetime.datetime.now()
        out = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).communicate()[0]

        p = digits_re.split(out.decode())

        if len(prevp) != len(p):
            s = p
        else:
            s = []
            i = 0
            for i, (n, o) in enumerate(zip(p, prevp)):
                if isfloat(n) and isfloat(o) and float(n) > float(o):
                    td = t0 - prevt
                    v = (float(n) - float(o)) / total_seconds(td)
                    if v > 1000000000:
                        v, suffix = v / 1000000000., 'g'
                    elif v > 1000000:
                        v, suffix = v / 1000000., 'm'
                    elif v > 1000:
                        v, suffix = v / 1000.,'k'
                    else:
                        suffix = ''
                    s.append('\x1b[7m')
                    s.append('%*s' % (len(n), '%.1f%s/s' % (v, suffix)))
                    s.append('\x1b[0m')
                else:
                    s.append(n)
            s += n[i:]

        prefix = "%sEvery %.1fs: %s\t\t%s" % (CLS, to, ' '.join(cmd), t0)
        sys.stdout.write(prefix + '\n\n' + ''.join(s).rstrip() + '\n')
        sys.stdout.flush()

        prevt = t0
        prevp = p
        time.sleep(to)

if __name__ == '__main__':
    try:
        main(sys.argv[1:])
    except KeyboardInterrupt:
        print('Interrupted')
        sys.exit(0)
    except SystemExit:
        os._exit(0)
