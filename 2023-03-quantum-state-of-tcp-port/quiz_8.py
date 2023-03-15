#!/usr/bin/env -S unshare --user --map-root-user --net -- strace -e %net -- python

"""
Quiz #8
-------

>>> s1 = socket(AF_INET, SOCK_STREAM)
>>> s1.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
>>> s1.bind(('127.0.0.1', 60_000))
>>> s1.connect(('127.9.9.9', 1234))
>>> s1.getsockname(), s1.getpeername()
(('127.0.0.1', 60000), ('127.9.9.9', 1234))
>>>
>>> s2 = socket(AF_INET, SOCK_STREAM)
>>> s2.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
>>> s2.connect(('127.8.8.8', 1234))
Traceback (most recent call last):
  ...
OSError: [Errno 99] Cannot assign requested address
>>>

Outcome: FAILURE. Local (IP, port) can't be shared.

Cleanup:

>>> s1.close()
>>> s2.close()

Solution #8
-----------

There is NONE.

"""

from quiz_common import run_doctest
from socket import *

if __name__ == "__main__":
    run_doctest(__name__)
