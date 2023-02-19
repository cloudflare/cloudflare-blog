#!/usr/bin/env -S unshare --user --map-root-user --net -- strace -e %net -- python

"""
Quiz #3
-------

>>> s1 = socket(AF_INET, SOCK_STREAM)
>>> s1.bind(('127.1.1.1', 0))
>>> s1.connect(('127.9.9.9', 1234))
>>> s1.getsockname(), s1.getpeername()
(('127.1.1.1', 60000), ('127.9.9.9', 1234))

>>> s2 = socket(AF_INET, SOCK_STREAM)
>>> s2.connect(('127.9.9.9', 1234))
Traceback (most recent call last):
  ...
OSError: [Errno 99] Cannot assign requested address

Outcome: FAILURE. Local port can't be shared.

Cleanup:

>>> p1, _ = ln.accept()
>>> p1.close() # TIME-WAIT on server side 
>>> s1.close()
>>> s2.close()

Solution #3
-----------

Use `IP_BIND_ADDRESS_NO_PORT` socket option.

>>> s1 = socket(AF_INET, SOCK_STREAM)
>>> s1.setsockopt(SOL_IP, IP_BIND_ADDRESS_NO_PORT, 1)
>>> s1.bind(('127.1.1.1', 0))
>>> s1.connect(('127.9.9.9', 1234))
>>> s1.getsockname(), s1.getpeername()
(('127.1.1.1', 60000), ('127.9.9.9', 1234))
>>>
>>> s2 = socket(AF_INET, SOCK_STREAM)
>>> s2.connect(('127.9.9.9', 1234))
>>> s2.getsockname(), s2.getpeername()
(('127.0.0.1', 60000), ('127.9.9.9', 1234))
>>>

Outcome: SUCCESS. Local port is shared when using `IP_BIND_ADDRESS_NO_PORT` option.

Cleanup:

>>> s1.close()
>>> s2.close()
"""

from quiz_common import run_doctest
from socket import *

if __name__ == "__main__":
    run_doctest(__name__)
