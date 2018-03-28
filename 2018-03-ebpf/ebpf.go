package main

import (
	"encoding/binary"
	"fmt"
	"github.com/majek/ebpf"
	"syscall"
	"time"
)

const (
	SO_ATTACH_BPF = 50
	SO_DETACH_BPF = 27
	ETH_P_IPV6    = uint16(0x86DD)
)

func htons(a uint16) uint16 {
	p0 := a & 0xFF
	p1 := (a >> 8) & 0xFF
	return (p0 << 8) | p1
}

var (
	byteOrder = binary.LittleEndian
)

type MapU64 uint64

func (k MapU64) MarshalBinary() ([]byte, error) {
	ret := make([]byte, 8)
	byteOrder.PutUint64(ret, uint64(k))
	return ret, nil
}
func (k *MapU64) UnmarshalBinary(data []byte) error {
	*k = MapU64(byteOrder.Uint64(data))
	return nil
}

type MapU32 uint32

func (k MapU32) MarshalBinary() ([]byte, error) {
	ret := make([]byte, 4)
	byteOrder.PutUint32(ret, uint32(k))
	return ret, nil
}
func (k *MapU32) UnmarshalBinary(data []byte) error {
	*k = MapU32(byteOrder.Uint32(data))
	return nil
}

func AttachTTLBPF(sockFd int) (int, int, error) {
	var (
		mapFd = -1
		bpfFd = -1
	)

	bpfMap, err := ebpf.NewMap(ebpf.Hash, 4, 8, 4, 0)
	if err != nil {
		// BPF_MAP_CREATE often fails due to "locked memory"
		// limits. By default 'ulimit -l' is only 64 KiB and
		// apparently that's little. Instead of just failing,
		// it makes sense to retry it. Surprisingly often
		// that'll actually work.

		// TODO
		// mapCreateRetryCtr.Inc()

		time.Sleep(10 * time.Millisecond)
		bpfMap, err = ebpf.NewMap(ebpf.Hash, 4, 8, 4, 0)
	}
	if err != nil {
		// TODO
		// mapCreateErrorCtr.Inc()
		return -1, -1, err
	}
	mapFd = bpfMap.GetFd()

	ebpfInss := ebpf.Instructions{
		// r1 has ctx
		// r0 = ctx[16] (aka protocol)
		ebpf.BPFIDstOffSrc(ebpf.LdXW, ebpf.Reg0, ebpf.Reg1, 16),

		// Perhaps ipv6
		ebpf.BPFIDstOffImm(ebpf.JEqImm, ebpf.Reg0, 3, int32(htons(ETH_P_IPV6))),

		// otherwise assume ipv4
		// 8th byte in IPv4 is TTL
		// LDABS requires ctx in R6
		ebpf.BPFIDstSrc(ebpf.MovSrc, ebpf.Reg6, ebpf.Reg1),
		ebpf.BPFIImm(ebpf.LdAbsB, int32(-0x100000+8)),
		ebpf.BPFIDstOff(ebpf.Ja, ebpf.Reg0, 2),

		// 7th byte in IPv6 is Hop count
		// LDABS requires ctx in R6
		ebpf.BPFIDstSrc(ebpf.MovSrc, ebpf.Reg6, ebpf.Reg1),
		ebpf.BPFIImm(ebpf.LdAbsB, int32(-0x100000+7)),

		// stash the load result into FP[-4]
		ebpf.BPFIDstOffSrc(ebpf.StXW, ebpf.RegFP, ebpf.Reg0, -4),
		// stash the &FP[-4] into r2
		ebpf.BPFIDstSrc(ebpf.MovSrc, ebpf.Reg2, ebpf.RegFP),
		ebpf.BPFIDstImm(ebpf.AddImm, ebpf.Reg2, -4),

		// r1 must point to map
		ebpf.BPFILdMapFd(ebpf.Reg1, mapFd),
		ebpf.BPFIImm(ebpf.Call, ebpf.MapLookupElement),

		// load ok? inc. Otherwise? jmp to mapupdate
		ebpf.BPFIDstOff(ebpf.JEqImm, ebpf.Reg0, 3),
		ebpf.BPFIDstImm(ebpf.MovImm, ebpf.Reg1, 1),
		ebpf.BPFIDstSrc(ebpf.XAddStSrc, ebpf.Reg0, ebpf.Reg1),
		ebpf.BPFIDstOff(ebpf.Ja, ebpf.Reg0, 9),

		// MapUpdate
		// r1 has map ptr
		ebpf.BPFILdMapFd(ebpf.Reg1, mapFd),
		// r2 has key -> &FP[-4]
		ebpf.BPFIDstSrc(ebpf.MovSrc, ebpf.Reg2, ebpf.RegFP),
		ebpf.BPFIDstImm(ebpf.AddImm, ebpf.Reg2, -4),
		// r3 has value -> &FP[-16] , aka 1
		ebpf.BPFIDstOffImm(ebpf.StDW, ebpf.RegFP, -16, 1),
		ebpf.BPFIDstSrc(ebpf.MovSrc, ebpf.Reg3, ebpf.RegFP),
		ebpf.BPFIDstImm(ebpf.AddImm, ebpf.Reg3, -16),
		// r4 has flags, 0
		ebpf.BPFIDstImm(ebpf.MovImm, ebpf.Reg4, 0),
		ebpf.BPFIImm(ebpf.Call, ebpf.MapUpdateElement),

		// set exit code to -1, don't trunc packet
		ebpf.BPFIDstImm(ebpf.MovImm, ebpf.Reg0, -1),
		ebpf.BPFIOp(ebpf.Exit),
	}

	bpfProgram, err := ebpf.NewProgram(ebpf.SocketFilter, &ebpfInss, "GPL", 0)
	if err != nil {
		fmt.Printf("%s\n", err)
		// TODO
		// bpfProgLoadErrorCtr.Inc()
		bpfMap.Close()
		return -1, -1, err
	}
	bpfFd = bpfProgram.GetFd()

	err = syscall.SetsockoptInt(sockFd, syscall.SOL_SOCKET, SO_ATTACH_BPF, bpfFd)
	if err != nil {
		bpfMap.Close()
		bpfProgram.Close()
		return -1, -1, err
	}
	return mapFd, bpfFd, nil
}

func ReadTTLBPF(mapFd int) int {
	if mapFd < 0 {
		return 255
	}
	bpfMap := ebpf.Map(mapFd)

	var (
		minDist = 255
		value   MapU64
		a, b    MapU32
	)
	for {
		ok, err := bpfMap.Get(a, &value, 8)
		if ok {
			// a is TTL, value is counter
			ttl := int(a)
			var dist int
			switch {
			case ttl > 128:
				dist = 255 - ttl
			case ttl > 64:
				dist = 128 - ttl
			case ttl > 32:
				dist = 64 - ttl
			default:
				dist = 32 - ttl
			}
			if minDist > dist {
				minDist = dist
			}
			bpfMap.Delete(a)
		}

		ok, err = bpfMap.GetNextKey(a, &b, 4)
		if err != nil || ok == false || a == b {
			break
		}
		a = b
	}
	return minDist
}

func DetachTTLBPF(sockFd, mapFd, bpfFd int) {
	syscall.SetsockoptInt(sockFd, syscall.SOL_SOCKET, SO_DETACH_BPF, bpfFd)
	syscall.Close(bpfFd)
	syscall.Close(mapFd)
}
