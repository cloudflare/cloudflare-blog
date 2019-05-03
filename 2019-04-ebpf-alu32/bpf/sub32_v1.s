
bpf/sub32_v1.o:	file format ELF64-BPF

Disassembly of section .text:
sub32_v1:
       0:	r0 = r1
       1:	r0 -= r2
       2:	r0 <<= 32
       3:	r0 >>= 32
       4:	exit
