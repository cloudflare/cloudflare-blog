// This is a trivial TCP server leaking sockets.

package main

import (
	"fmt"
	"net"
	"time"
)

func handle(conn net.Conn) {
	defer conn.Close()
	for {
		time.Sleep(time.Second)
	}
}

func main() {
	IP := ""
	Port := 5000
	listener, err := net.Listen("tcp4", fmt.Sprintf("%s:%d", IP, Port))
	if err != nil {
		panic(err)
	}

	i := 0

	for {
		if conn, err := listener.Accept(); err == nil {
			i += 1
			if i < 800 {
				go handle(conn)
			} else {
				conn.Close()
			}
		} else {
			panic(err)
		}
	}
}
