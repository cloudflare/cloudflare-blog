package main

import (
	"net"
	"os"
	"syscall"
	"time"
)

func MagicTCPDialTimeoutTTL(dst *net.TCPAddr, timeout time.Duration) (net.Conn, int, error) {
	var domain int
	if dst.IP.To4() != nil {
		domain = syscall.AF_INET
	} else {
		domain = syscall.AF_INET6
	}

	fd, err := syscall.Socket(domain, syscall.SOCK_STREAM, 0)
	if err != nil {
		return nil, 255, err
	}

	mapFd, bpfFd, _ := AttachTTLBPF(fd)

	var sa syscall.Sockaddr
	if domain == syscall.AF_INET {
		var x [4]byte
		copy(x[:], dst.IP.To4())
		sa = &syscall.SockaddrInet4{Port: dst.Port, Addr: x}
	} else {
		var x [16]byte
		copy(x[:], dst.IP.To16())
		sa = &syscall.SockaddrInet6{Port: dst.Port, Addr: x}
	}

	if timeout.Nanoseconds() > 0 {
		// Set SO_SNDTIMEO
		var (
			tv syscall.Timeval
			ns = timeout.Nanoseconds()
		)
		tv.Sec = ns / 1000000000
		tv.Usec = (ns - tv.Sec*1000000000) / 1000

		if err := syscall.SetsockoptTimeval(fd, syscall.SOL_SOCKET, syscall.SO_SNDTIMEO, &tv); err != nil {
			syscall.Close(fd)
			return nil, 255, err
		}
	}

	// This is blocking.
	if err := syscall.Connect(fd, sa); err != nil {
		syscall.Close(fd)
		return nil, 255, err
	}

	if timeout.Nanoseconds() > 0 {
		// Clear SO_SNDTIMEO
		var tv syscall.Timeval
		syscall.SetsockoptTimeval(fd, syscall.SOL_SOCKET, syscall.SO_SNDTIMEO, &tv)
	}

	minDist := ReadTTLBPF(mapFd)
	DetachTTLBPF(fd, mapFd, bpfFd)

	f := os.NewFile(uintptr(fd), "socket")
	c, err := net.FileConn(f)
	f.Close()
	if err != nil {
		return nil, 255, err
	}
	return c, minDist, nil
}
