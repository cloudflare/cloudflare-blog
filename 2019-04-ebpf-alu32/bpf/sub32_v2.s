
bpf/sub32_v2.o:	file format ELF64-BPF

Disassembly of section .text:
sub32_v2:
       0:	w1 -= w2
       1:	*(u32 *)(r10 - 4) = r1
       2:	r0 = *(u32 *)(r10 - 4)
       3:	exit
