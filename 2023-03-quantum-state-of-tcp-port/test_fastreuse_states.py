#!/usr/bin/env -S unshare --net -- python3

"""
Tests for fastreuse state transitions
"""

from socket import *

from test_common import (
    DST1,
    DST2,
    dump_bind_bucket,
    setup_netns,
    listener,
    set_local_port_range,
)


def test_from_neg_1(sport):
    s1 = socket()
    s1.connect(DST1)
    assert dump_bind_bucket(sport) == -1

    s2 = socket()
    s2.connect(DST2)
    assert dump_bind_bucket(sport) == -1

    s3 = socket()
    s3.setsockopt(SOL_IP, IP_BIND_ADDRESS_NO_PORT, 1)
    s3.bind(("127.3.3.3", 0))
    s3.connect(DST1)
    assert dump_bind_bucket(sport) == -1


def test_from_neg_2(sport):
    s1 = socket()
    s1.connect(DST1)
    assert dump_bind_bucket(sport) == -1

    s2 = socket()
    s2.bind(("127.2.2.2", sport))
    assert dump_bind_bucket(sport) == 0


def test_from_neg_3(sport):
    s1 = socket()
    s1.connect(DST1)
    assert dump_bind_bucket(sport) == -1

    s2 = socket()
    s2.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s2.bind(("127.2.2.2", sport))
    assert dump_bind_bucket(sport) == -1


def test_from_zero_1(sport):
    s1 = socket()
    s1.bind(("127.1.1.1", sport))
    assert dump_bind_bucket(sport) == 0

    s2 = socket()
    s2.bind(("127.2.2.2", sport))
    assert dump_bind_bucket(sport) == 0


def test_from_zero_2(sport):
    s1 = socket()
    s1.bind(("127.1.1.1", sport))
    assert dump_bind_bucket(sport) == 0

    s2 = socket()
    s2.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s2.bind(("127.2.2.2", sport))
    assert dump_bind_bucket(sport) == 0


def test_from_pos_1(sport):
    s1 = socket()
    s1.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s1.bind(("127.1.1.1", sport))
    assert dump_bind_bucket(sport) == +1

    s2 = socket()
    s2.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s2.bind(("127.2.2.2", sport))
    assert dump_bind_bucket(sport) == +1


def test_from_pos_2(sport):
    s1 = socket()
    s1.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
    s1.bind(("127.1.1.1", sport))
    dump_bind_bucket(sport)

    s2 = socket()
    s2.bind(("127.2.2.2", sport))
    dump_bind_bucket(sport)


def main():
    setup_netns()
    ln = listener()

    # transitions from fastreuse = -1
    set_local_port_range(40_001)
    test_from_neg_1(40_001)

    set_local_port_range(40_002)
    test_from_neg_2(40_002)

    set_local_port_range(40_003)
    test_from_neg_3(40_003)

    # transitions from fastreuse = 0
    set_local_port_range(50_001)
    test_from_zero_1(50_001)

    set_local_port_range(50_002)
    test_from_zero_2(50_002)

    # transitions from fastreuse = +1
    set_local_port_range(60_001)
    test_from_pos_1(60_001)

    set_local_port_range(60_002)
    test_from_pos_2(60_002)


if __name__ == "__main__":
    main()
