// This is a trivial TCP server leaking sockets.

package main

import (
	"fmt"
	"net"
	"time"
)

func handle(conn *net.TCPConn) {
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
			c := conn.(*net.TCPConn)
			// OS should not send keep-alive messages on the connection.
			// By default, keep-alive probes are sent with a default value
			// (currently 15 seconds), if supported by the protocol and operating
			// system. Hence, SetKeepAlive should be turned off for the experiment.
			c.SetKeepAlive(false)
			if i < 800 {
				go handle(c)
			} else {
				conn.Close()
			}
		} else {
			panic(err)
		}
	}
}
