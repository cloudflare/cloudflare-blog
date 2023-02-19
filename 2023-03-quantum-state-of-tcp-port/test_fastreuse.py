#!/usr/bin/env -S unshare --net -- python3

"""
Tests for when a local TCP port can be shared.

Covers SO_REUSEADDR.
Does not cover SO_REUSEPORT.
"""

import errno
from socket import *

from test_common import (
    DST1,
    DST2,
    IP_BIND_ADDRESS_NO_PORT,
    bind_ex,
    dump_bind_bucket,
    listener,
    set_local_port_range,
    setup_netns,
)


# GIVEN: fastreuse = -1
# WHEN: bind() to same local ephemeral port
# THEN: success IFF local IP unique, else error
def test_reuse_neg_1a(sport):
    s1 = socket()
    s1.connect(DST1)
    dump_bind_bucket(sport)

    # same local IP
    s2 = socket()
    e = bind_ex(s2, ("127.0.0.1", 0))
    assert e == errno.EADDRINUSE
    dump_bind_bucket(sport)

    # unique local IP
    s3 = socket()
    s3.bind(("127.1.1.1", 0))
    dump_bind_bucket(sport)

    # bucket now in fastreuse = 0


# GIVEN: fastreuse = -1
# WHEN: bind() to same local specified port
# THEN: success IFF local IP unique, else error
def test_reuse_neg_1b(sport):
    s1 = socket()
    s1.connect(DST1)
    dump_bind_bucket(sport)

    # same local IP
    s2 = socket()
    e = bind_ex(s2, ("127.0.0.1", sport))
    assert e == errno.EADDRINUSE
    dump_bind_bucket(sport)

    # unique local IP
    s3 = socket()
    s3.bind(("127.3.3.3", sport))
    dump_bind_bucket(sport)

    # bucket now in fastreuse = 0


# GIVEN: fastreuse = -1
# WHEN: bind() to the specific port with SO_REUSEADDR
# THEN: success IFF local IP is unique OR conflicting socket uses SO_REUSEADDR, else error
def test_reuse_neg_2a(sport):
    s1 = socket()
    s1.connect(DST1)
    dump_bind_bucket(sport)

    # same local IP
    s2 = socket()
    s2.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    e = bind_ex(s2, ("127.0.0.1", sport))
    assert e == errno.EADDRINUSE
    dump_bind_bucket(sport)

    # unique local IP
    s3 = socket()
    s3.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s3.bind(("127.3.3.3", sport))
    dump_bind_bucket(sport)

    # conflicting socket uses SO_REUSEADDR
    s4 = socket()
    s4.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s4.bind(("127.3.3.3", sport))
    dump_bind_bucket(sport)


# GIVEN: fastreuse = -1
# WHEN: bind() to the specific port with SO_REUSEADDR, conflicting socket uses SO_REUSEADDR
# THEN: success
def test_reuse_neg_2b(sport):
    s1 = socket()
    s1.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s1.connect(DST1)
    dump_bind_bucket(sport)

    # same local IP
    s2 = socket()
    s2.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s2.bind(("127.0.0.1", sport))
    dump_bind_bucket(sport)


# GIVEN: fastreuse = -1
# WHEN: connect() from the same ephemeral port to the same remote (IP, port)
# THEN: success IFF local IP unique, else error
def test_reuse_neg_3(sport):
    s1 = socket()
    s1.setsockopt(SOL_IP, IP_BIND_ADDRESS_NO_PORT, 1)
    s1.bind(("127.1.1.1", 0))
    s1.connect(DST1)
    dump_bind_bucket(sport)

    # same local IP
    s2 = socket()
    s2.setsockopt(SOL_IP, IP_BIND_ADDRESS_NO_PORT, 1)
    s2.bind(("127.1.1.1", 0))
    e = s2.connect_ex(DST1)
    assert e == errno.EADDRNOTAVAIL
    dump_bind_bucket(sport)

    # unique local IP
    s3 = socket()
    s3.connect(DST1)
    dump_bind_bucket(sport)


# GIVEN: fastreuse = -1
# WHEN: connect() from the same ephemeral port to a unique remote (IP, port)
# THEN: success
def test_reuse_neg_4(sport):
    s1 = socket()
    s1.connect(DST1)
    dump_bind_bucket(sport)

    s2 = socket()
    s2.connect(DST2)
    dump_bind_bucket(sport)


# GIVEN: fastreuse = 0
# WHEN: bind() to the same port (ephemeral or specified)
# THEN: success IFF local IP unique
def test_reuse_zero_1(sport):
    s1 = socket()
    s1.bind(("127.1.1.1", sport))
    dump_bind_bucket(sport)

    # same IP, ephemeral port
    s2 = socket()
    e = bind_ex(s2, ("127.1.1.1", 0))
    assert e == errno.EADDRINUSE
    dump_bind_bucket(sport)

    # same IP, specified port
    s3 = socket()
    e = bind_ex(s3, ("127.1.1.1", sport))
    assert e == errno.EADDRINUSE
    dump_bind_bucket(sport)

    # unique IP, ephemeral port
    s3 = socket()
    s3.bind(("127.3.3.3", 0))
    dump_bind_bucket(sport)

    # unique IP, specified port
    s4 = socket()
    s4.bind(("127.4.4.4", sport))
    dump_bind_bucket(sport)


# GIVEN: fastreuse = 0
# WHEN: bind() to the specific port with SO_REUSEADDR
# THEN: success IFF local IP is unique OR conflicting socket uses SO_REUSEADDR, else error
def test_reuse_zero_2(sport):
    s1 = socket()
    s1.bind(("127.1.1.1", sport))
    dump_bind_bucket(sport)

    # same local IP
    s2 = socket()
    s2.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    e = bind_ex(s2, ("127.1.1.1", sport))
    assert e == errno.EADDRINUSE
    dump_bind_bucket(sport)

    # unique local IP
    s3 = socket()
    s3.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s3.bind(("127.3.3.3", sport))
    dump_bind_bucket(sport)

    # conflicting socket uses SO_REUSEADDR
    s4 = socket()
    s4.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s4.bind(("127.3.3.3", sport))
    dump_bind_bucket(sport)


# GIVEN: fastreuse = 0
# WHEN: connect() from the same ephemeral port to the same remote (IP, port)
# THEN: error
def test_reuse_zero_3(sport):
    s1 = socket()
    s1.bind(("127.0.0.1", sport))
    s1.connect(DST1)
    dump_bind_bucket(sport)

    # same local IP
    s2 = socket()
    e = s2.connect_ex(DST1)
    assert e == errno.EADDRNOTAVAIL
    dump_bind_bucket(sport)

    # unique local IP
    s3 = socket()
    s3.setsockopt(SOL_IP, IP_BIND_ADDRESS_NO_PORT, 1)
    s3.bind(("127.3.3.3", 0))
    e = s3.connect_ex(DST1)
    assert e == errno.EADDRNOTAVAIL
    dump_bind_bucket(sport)


# GIVEN: fastreuse = 0
# WHEN: connect() from the same ephemeral port to a unique remote (IP, port)
# THEN: error
def test_reuse_zero_4(sport):
    s1 = socket()
    s1.bind(("127.0.0.1", sport))
    s1.connect(DST1)
    dump_bind_bucket(sport)

    # same local IP
    s2 = socket()
    e = s2.connect_ex(DST2)
    assert e == errno.EADDRNOTAVAIL
    dump_bind_bucket(sport)

    # unique local IP
    s3 = socket()
    s3.setsockopt(SOL_IP, IP_BIND_ADDRESS_NO_PORT, 1)
    s3.bind(("127.3.3.3", 0))
    e = s3.connect_ex(DST2)
    assert e == errno.EADDRNOTAVAIL
    dump_bind_bucket(sport)


# GIVEN: fastreuse = +1
# WHEN: bind() to the same port (ephemeral or specified)
# THEN: success IFF local IP unique, else error
def test_reuse_pos_1(sport):
    s1 = socket()
    s1.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s1.bind(("127.1.1.1", sport))
    dump_bind_bucket(sport)

    # same local IP
    s2 = socket()
    e = bind_ex(s2, ("127.1.1.1", sport))
    assert e == errno.EADDRINUSE
    dump_bind_bucket(sport)

    # unique local IP
    s3 = socket()
    s3.bind(("127.3.3.3", sport))
    dump_bind_bucket(sport)


# GIVEN: fastreuse = +1
# WHEN: bind() to the specific port with SO_REUSEADDR
# THEN: success
def test_reuse_pos_2(sport):
    s1 = socket()
    s1.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s1.bind(("127.1.1.1", sport))
    dump_bind_bucket(sport)

    # same local IP
    s2 = socket()
    s2.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s2.bind(("127.1.1.1", sport))
    dump_bind_bucket(sport)

    # unique local IP
    s3 = socket()
    s3.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s3.bind(("127.3.3.3", sport))
    dump_bind_bucket(sport)


# GIVEN: fastreuse = +1
# WHEN: connect() from the same ephemeral port to the same remote (IP, port)
# THEN: error
def test_reuse_pos_3(sport):
    s1 = socket()
    s1.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s1.bind(("127.1.1.1", sport))
    s1.connect(DST1)
    dump_bind_bucket(sport)

    # same local IP
    s2 = socket()
    s2.setsockopt(SOL_IP, IP_BIND_ADDRESS_NO_PORT, 1)
    s2.bind(("127.1.1.1", 0))
    e = s2.connect_ex(DST1)
    assert e == errno.EADDRNOTAVAIL
    dump_bind_bucket(sport)

    # unique local IP
    s3 = socket()
    e = s3.connect_ex(DST1)
    assert e == errno.EADDRNOTAVAIL
    dump_bind_bucket(sport)


# GIVEN: fastreuse = +1
# WHEN: connect() from the same ephemeral port to a unique remote (IP, port)
# THEN: error
def test_reuse_pos_4(sport):
    s1 = socket()
    s1.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s1.bind(("127.1.1.1", sport))
    s1.connect(DST1)
    dump_bind_bucket(sport)

    # same local IP
    s2 = socket()
    s2.setsockopt(SOL_IP, IP_BIND_ADDRESS_NO_PORT, 1)
    s2.bind(("127.1.1.1", 0))
    e = s2.connect_ex(DST2)
    assert e == errno.EADDRNOTAVAIL
    dump_bind_bucket(sport)

    # unique local IP
    s3 = socket()
    e = s3.connect_ex(DST2)
    assert e == errno.EADDRNOTAVAIL
    dump_bind_bucket(sport)


def main():
    setup_netns()
    ln = listener()

    # fastreuse = -1
    set_local_port_range(40_010)
    test_reuse_neg_1a(40_010)

    set_local_port_range(40_011)
    test_reuse_neg_1b(40_011)

    set_local_port_range(40_020)
    test_reuse_neg_2a(40_020)

    set_local_port_range(40_021)
    test_reuse_neg_2b(40_021)

    set_local_port_range(40_030)
    test_reuse_neg_3(40_030)

    set_local_port_range(40_040)
    test_reuse_neg_4(40_040)

    # fastreuse = 0
    set_local_port_range(50_010)
    test_reuse_zero_1(50_010)

    set_local_port_range(50_020)
    test_reuse_zero_2(50_020)

    set_local_port_range(50_030)
    test_reuse_zero_3(50_030)

    set_local_port_range(50_040)
    test_reuse_zero_4(50_040)

    # fastreuse = +1
    set_local_port_range(60_010)
    test_reuse_pos_1(60_010)

    set_local_port_range(60_020)
    test_reuse_pos_2(60_020)

    set_local_port_range(60_030)
    test_reuse_pos_3(60_030)

    set_local_port_range(60_040)
    test_reuse_pos_4(60_040)


if __name__ == "__main__":
    main()
