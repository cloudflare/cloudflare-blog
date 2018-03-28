package main

import (
	"flag"
	"fmt"
	"net"
	"os"
	"strings"
	"time"
)

func main() {
	flag.Parse()

	for _, targetUri := range flag.Args() {
		p := strings.SplitN(targetUri, "://", 2)
		network, target := p[0], p[1]
		addr, err := net.ResolveTCPAddr(network, target)
		if err != nil {
			fmt.Fprintf(os.Stderr, "[!] resolv failed %s: %s\n", target, err)
			os.Exit(1)
		}

		conn, ttl, err := MagicTCPDialTimeoutTTL(addr, 0*time.Second)
		if err != nil {
			fmt.Fprintf(os.Stderr, "[!] conn failed %s: %s\n", target, err)
			os.Exit(1)
		}
		fmt.Printf("[+] All good. Measured TTL distance to %s://%s %s is %d\n", network, target, addr, ttl)
		conn.Close()
	}
	return
}
