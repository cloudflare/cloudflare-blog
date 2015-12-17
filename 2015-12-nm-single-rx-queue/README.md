Netmap Single RX queue mode test
================================

A working example showing the single RX queue Netmap mode.

To build the example first clone the Netmap repository into the deps folder:
```sh
$ cd cloudflare-blog/2015-12-nm-single-rx-queue
$ git clone https://github.com/luigirizzo/netmap deps/netmap
```

build the application:
```sh
$ make
```

build and load Netmap:
```sh
$ cd deps/netmap/LINUX
$ ./configure --kernel-sources=/path/to/kernel/sources --driver=ixgbe
$ make
$ sudo insmod netmap.ko
$ sudo insmod ixgbe/ixgbe.ko
```

and launch the example:
```sh
$ sudo ./nm-single-rx-queue eth0 1
```
