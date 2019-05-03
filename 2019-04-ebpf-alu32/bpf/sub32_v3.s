
bpf/sub32_v3.o:	file format ELF64-BPF

Disassembly of section .text:
sub32_v3:
       0:	r0 = r1
       1:	w0 -= w2
       2:	*(u32 *)(r10 - 4) = r0
       3:	exit
