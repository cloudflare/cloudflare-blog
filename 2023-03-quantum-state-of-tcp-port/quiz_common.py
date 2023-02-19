from os import system
from socket import *

def setup(test):
    system("ip link set dev lo up")
    system("sysctl --write --quiet net.ipv4.ip_local_port_range='60000 60000'")
    
    # Open a listening socket at *:1234. We will connect to it.
    ln = socket(AF_INET, SOCK_STREAM)
    ln.bind(("", 1234))
    ln.listen(SOMAXCONN)

    # HACK: Docs say we shouldn't modify DocTest.globs
    # https://docs.python.org/3/library/doctest.html#doctest.DocTest
    test.globs["ln"] = ln

def teardown(test):
    ln = test.globs["ln"]
    ln.close()

def run_doctest(module_name):
    import doctest
    import unittest

    testsuite = doctest.DocTestSuite(module=module_name,
                                     setUp=setup,
                                     tearDown=teardown)
    unittest.TextTestRunner(verbosity=2).run(testsuite)
