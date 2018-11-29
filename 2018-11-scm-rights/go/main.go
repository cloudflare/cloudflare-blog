package main

import (
	"errors"
	"fmt"
	"net"
	"os"
	"syscall"
)

type scmListener struct {
	*net.UnixListener
}

type scmConn struct {
	*net.UnixConn
}

var path = "/tmp/scm_example.sock"

func listenSCM() (*scmListener, error) {
	syscall.Unlink(path)

	addr, err := net.ResolveUnixAddr("unix", path)
	if err != nil {
		return nil, err
	}

	ul, err := net.ListenUnix("unix", addr)
	if err != nil {
		return nil, err
	}

	err = os.Chmod(path, 0777)
	if err != nil {
		return nil, err
	}

	return &scmListener{ul}, nil
}

func (l *scmListener) Accept() (*scmConn, error) {
	uc, err := l.AcceptUnix()
	if err != nil {
		return nil, err
	}
	return &scmConn{uc}, nil
}

func (c *scmConn) ReadFD() (*os.File, error) {
	msg, oob := make([]byte, 2), make([]byte, 128)

	_, oobn, _, _, err := c.ReadMsgUnix(msg, oob)
	if err != nil {
		return nil, err
	}

	cmsgs, err := syscall.ParseSocketControlMessage(oob[0:oobn])
	if err != nil {
		return nil, err
	} else if len(cmsgs) != 1 {
		return nil, errors.New("invalid number of cmsgs received")
	}

	fds, err := syscall.ParseUnixRights(&cmsgs[0])
	if err != nil {
		return nil, err
	} else if len(fds) != 1 {
		return nil, errors.New("invalid number of fds received")
	}

	fd := os.NewFile(uintptr(fds[0]), "")
	if fd == nil {
		return nil, errors.New("could not open fd")
	}

	return fd, nil
}

func main() {
	ul, _ := listenSCM()
	defer ul.Close()

	for {
		conn, _ := ul.Accept()

		fd, err := conn.ReadFD()
		if err != nil {
			fmt.Println("Error:", err)
		}

		fConn, err := net.FileConn(fd)
		tcpConn := fConn.(*net.TCPConn)

		tcpConn.Write([]byte("Hello from Go!\n"))
		tcpConn.Close()
		conn.Close()
	}
}
