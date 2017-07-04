Misc command line tools
=======================

mmwatch
--------

A tool like "watch" but instead of just printing the output of passed
command, it tries to print the numbers as rates. Best example:

    $ ./mmwatch 'ifconfig'


mmsum
-----

Sums numbers passed on standad input. Pretty trivial but it's not so
easy to do it in bash. Example:

    $ echo -e "1\n2" | ./mmsum
    3

mmhistogram
-----------

Prints a systemtap-like histogram from passed numbers.

    $ echo -e "1\n1\n4" | ./mmhistogram
    Values min:1.00 avg:2.00 med=1.00 max:4.00 dev:1.41 count:3
    Values:
     value |-------------------------------------------------- count
         0 |************************************************** 2
         1 |                                                   0
         2 |                         ************************* 1
