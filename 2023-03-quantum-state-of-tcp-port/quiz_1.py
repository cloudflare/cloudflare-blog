#!/usr/bin/env -S unshare --user --map-root-user --net -- strace -e %net -- python

"""
Quiz #1
-------

>>> s1 = socket(AF_INET, SOCK_STREAM)
>>> s1.bind(('127.1.1.1', 0))
>>> s1.connect(('127.9.9.9', 1234))
>>> s1.getsockname(), s1.getpeername()
(('127.1.1.1', 60000), ('127.9.9.9', 1234))
>>>
>>> s2 = socket(AF_INET, SOCK_STREAM)
>>> s2.bind(('127.2.2.2', 0))
>>> s2.connect(('127.9.9.9', 1234))
>>> s2.getsockname(), s2.getpeername()
(('127.2.2.2', 60000), ('127.9.9.9', 1234))
>>>

Outcome: SUCCESS. Local port is shared.

Cleanup:

>>> s1.close()
>>> s2.close()
"""

from quiz_common import run_doctest
from socket import *

if __name__ == "__main__":
    run_doctest(__name__)
