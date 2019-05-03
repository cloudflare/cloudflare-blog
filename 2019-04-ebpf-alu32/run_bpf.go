package main

import (
	"flag"
	"fmt"
	"os"
	"strconv"
	"syscall"

	"github.com/newtools/ebpf"
)

// Missing constnants in syscall
const (
	SIGSTOP       = 19
	SO_ATTACH_BPF = 50
)

// BPF map keys
const (
	ARG_0 uint32 = iota
	ARG_1
	RES_0
)

var (
	filterArg     = flag.String("filter", "alu64", "Filter variant to load: \"alu64\", \"alu32\", \"ir\", \"stv\"")
	stopAfterLoad = flag.Bool("stop-after-load", false, "Stop the process after loading BPF program")
)

type SocketPair [2]int

func (p SocketPair) Close() error {
	err1 := syscall.Close(p[0])
	err2 := syscall.Close(p[1])

	if err1 != nil {
		return err1
	}
	return err2
}

type Context struct {
	ArgsMap  *ebpf.Map
	SockPair SocketPair
}

func (c *Context) Close() error {
	err1 := c.ArgsMap.Close()
	err2 := c.SockPair.Close()

	if err1 != nil {
		return err1
	}
	return err2
}

func loadBPF(filterName string) (*Context, error) {
	coll, err := ebpf.LoadCollection("bpf/filter.o")
	if err != nil {
		return nil, err
	}
	defer coll.Close()

	progName := fmt.Sprintf("filter_%s", filterName)
	prog := coll.DetachProgram(progName)
	if prog == nil {
		return nil, fmt.Errorf("program %q not found", progName)
	}
	defer prog.Close()

	argsMap := coll.DetachMap("args")
	if argsMap == nil {
		return nil, fmt.Errorf("map 'args' not found")
	}

	sockPair, err := syscall.Socketpair(syscall.AF_UNIX, syscall.SOCK_DGRAM, 0)
	if err != nil {
		argsMap.Close()
		return nil, err
	}

	err = syscall.SetsockoptInt(sockPair[0], syscall.SOL_SOCKET, SO_ATTACH_BPF, prog.FD())
	if err != nil {
		argsMap.Close()
		SocketPair(sockPair).Close()
		return nil, err
	}

	return &Context{ArgsMap: argsMap, SockPair: sockPair}, nil
}

func runBPF(ctx *Context, arg0, arg1 uint64) (uint64, error) {
	var (
		err error
		res uint64
	)
	err = ctx.ArgsMap.Put(ARG_0, arg0)
	if err != nil {
		return 0, err
	}
	err = ctx.ArgsMap.Put(ARG_1, arg1)
	if err != nil {
		return 0, err
	}

	// Run an empty message through the BPF filter.
	_, err = syscall.Write(ctx.SockPair[1], nil)
	if err != nil {
		return 0, err
	}
	_, err = syscall.Read(ctx.SockPair[0], nil)
	if err != nil {
		return 0, err
	}

	_, err = ctx.ArgsMap.Get(RES_0, &res)
	if err != nil {
		return 0, err
	}

	return res, nil
}

func parseArgs() (uint64, uint64) {
	flag.Parse()

	if flag.NArg() != 2 {
		fmt.Fprintf(os.Stderr, "Usage: run-bpf [options] uint64 uint64\n\n")
		fmt.Fprintf(os.Stderr, "Options:\n")
		flag.PrintDefaults()
		os.Exit(1)
	}

	arg0, err := strconv.ParseUint(flag.Arg(0), 0, 64)
	if err != nil {
		panic(err)
	}
	arg1, err := strconv.ParseUint(flag.Arg(1), 0, 64)
	if err != nil {
		panic(err)
	}
	return arg0, arg1
}

func main() {
	arg0, arg1 := parseArgs()

	ctx, err := loadBPF(*filterArg)
	if err != nil {
		panic(err)
	}
	defer ctx.Close()

	if *stopAfterLoad {
		syscall.Kill(0, SIGSTOP)
	}

	diff, err := runBPF(ctx, arg0, arg1)
	if err != nil {
		panic(err)
	}

	fmt.Printf("arg0 %20d %#016[1]x\n", arg0)
	fmt.Printf("arg1 %20d %#016[1]x\n", arg1)
	fmt.Printf("diff %20d %#016[1]x\n", diff)
}
