#!/usr/bin/env -S unshare --user --map-root-user --net -- strace -e %net -- python

"""
Quiz #7
-------

>>> s1 = socket(AF_INET, SOCK_STREAM)
>>> s1.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
>>> s1.connect(('127.8.8.8', 1234))
>>> s1.getsockname(), s1.getpeername()
(('127.0.0.1', 60000), ('127.8.8.8', 1234))
>>>
>>> s2 = socket(AF_INET, SOCK_STREAM)
>>> s2.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
>>> s2.bind(('127.0.0.1', 60_000))
>>> s2.connect(('127.9.9.9', 1234))
>>> s2.getsockname(), s2.getpeername()
(('127.0.0.1', 60000), ('127.9.9.9', 1234))
>>>

Outcome: SUCCESS. Local (IP, port) is shared.

Cleanup:

>>> s1.close()
>>> s2.close()
"""

from quiz_common import run_doctest
from socket import *

if __name__ == "__main__":
    run_doctest(__name__)
