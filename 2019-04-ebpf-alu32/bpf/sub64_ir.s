
bpf/sub64_ir.o:	file format ELF64-BPF

Disassembly of section .text:
sub64_ir:
       0:	r0 = r1
       1:	w1 -= w2
       2:	w3 = 1
       3:	w4 = w0
       4:	r4 <<= 32
       5:	r4 >>= 32
       6:	if r1 > r4 goto +1 <LBB0_2>
       7:	w3 = 0

LBB0_2:
       8:	r2 >>= 32
       9:	r0 >>= 32
      10:	w0 -= w2
      11:	w0 -= w3
      12:	r0 <<= 32
      13:	r0 |= r1
      14:	exit
