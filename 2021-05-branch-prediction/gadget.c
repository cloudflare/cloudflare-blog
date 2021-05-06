#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifdef linux
#include <error.h>
#include <linux/memfd.h>
#include <linux/mman.h>
#else
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif

#include "branch.h"

#define FIXED_ADDR (void *)0x5ffffff00000

#ifdef __x86_64__
#define MFD_HUGE_X HUGETLB_FLAG_ENCODE_2MB
#define MAP_HUGE_X MAP_HUGE_2MB
#define ASSUMED_SZ 2 * 1024 * 1024
#elif __aarch64__
#define MFD_HUGE_X HUGETLB_FLAG_ENCODE_512MB
#define MAP_HUGE_X MAP_HUGE_512MB
#define ASSUMED_SZ 512 * 1024 * 1024
#endif

#if __APPLE__
void blob_alloc(struct blob *blob)
{
	/* I failed to get VM_FLAGS_SUPERPAGE_SIZE_2MB working on m1 */
	void *ptr = mmap(NULL, ASSUMED_SZ, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANON, -1, 0);
	if (ptr == MAP_FAILED) {
		error(-1, errno, "mmap");
	}

	blob->ptr = ptr;
}
#else
void blob_alloc(struct blob *blob)
{
	int memfd = memfd_create("code_blob", MFD_HUGETLB | MFD_HUGE_X);
	if (memfd < 0) {
		error(-1, errno, "memfd");
	}
	int r = ftruncate(memfd, ASSUMED_SZ);
	if (r != 0) {
		error(-1, errno, "ftruncate");
	}

	void *ptr =
		mmap(FIXED_ADDR, ASSUMED_SZ, PROT_READ | PROT_WRITE | PROT_EXEC,
		     MAP_SHARED | MAP_HUGETLB | MAP_HUGE_X | MAP_POPULATE,
		     memfd, 0);
	if (ptr == MAP_FAILED) {
		fprintf(stderr,
			"[!] MAP_HUGETLB failed. Maybe Run:\n\techo 1 | sudo tee /proc/sys/vm/nr_hugepages\n");

		ptr = mmap(FIXED_ADDR, ASSUMED_SZ,
			   PROT_READ | PROT_WRITE | PROT_EXEC,
			   MAP_SHARED | MAP_POPULATE, memfd, 0);
		if (ptr == MAP_FAILED) {
			error(-1, errno, "mmap");
		}
	} else {
		blob->hugepage = 1;
	}

	blob->memfd = memfd;
	blob->ptr = ptr;

	void *ptr_sh;
	if (blob->hugepage) {
		ptr_sh = mmap(
			NULL, ASSUMED_SZ, PROT_READ | PROT_WRITE | PROT_EXEC,
			MAP_SHARED | MAP_HUGETLB | MAP_HUGE_X | MAP_POPULATE,
			blob->memfd, 0);
	} else {
		ptr_sh = mmap(NULL, ASSUMED_SZ,
			      PROT_READ | PROT_WRITE | PROT_EXEC,
			      MAP_SHARED | MAP_POPULATE, blob->memfd, 0);
	}
	if (ptr_sh == MAP_FAILED) {
		error(-1, errno, "mmap(shadow)");
	}
	blob->ptr_shadow = ptr_sh;
}
#endif

#define PUT(x)                                                                 \
	{                                                                      \
		p[0] = x;                                                      \
		p++;                                                           \
	}
#define PUTN(x, n)                                                             \
	{                                                                      \
		int _i;                                                        \
		for (_i = 0; _i < n; _i++) {                                   \
			PUT(x);                                                \
		}                                                              \
	}

#define PUTQ(_y)                                                               \
	{                                                                      \
		uint32_t _x = (_y);                                            \
		PUT(_x & 0xff);                                                \
		PUT((_x >> 8) & 0xff);                                         \
		PUT((_x >> 16) & 0xff);                                        \
		PUT((_x >> 24) & 0xff);                                        \
	}
#define PUTQ3(_y)                                                              \
	{                                                                      \
		uint32_t _x = (_y);                                            \
		PUT(_x & 0xff);                                                \
		PUT((_x >> 8) & 0xff);                                         \
		PUT((_x >> 16) & 0xff);                                        \
	}

#define PUTSEQ(x, l)                                                           \
	{                                                                      \
		memcpy(p, x, l);                                               \
		p += l;                                                        \
	}

void __attribute__((noinline)) blob_trap(struct blob *blob)
{
	asm("" : /*no out*/ : "r"(blob));
}

void blob_fill_code(struct blob *blob, int alignment, int cycle_count,
		    char *type)
{
	char *p = blob->ptr;
	int i, j;
#if __x86_64__
	/*
	  0x00005fffffe0003b:	e9 0c 00 00 00	jmpq   0x5fffffe0004c
	  0x00005fffffe00040:	c3	retq
	  0x00005fffffe00041:	90	nop
	  0x00005fffffe00042:	90	nop
	  0x00005fffffe00043:	90	nop
	  0x00005fffffe00044:	90	nop
	  0x00005fffffe00045:	90	nop
	  0x00005fffffe00046:	c3	retq
	  0x00005fffffe00047:	90	nop
	  0x00005fffffe00048:	90	nop
	  0x00005fffffe00049:	90	nop
	  0x00005fffffe0004a:	90	nop
	  0x00005fffffe0004b:	90	nop
	  0x00005fffffe0004c:	e8 ef ff ff ff	callq  0x5fffffe00040
	  0x00005fffffe00051:	90	nop
	  0x00005fffffe00052:	e8 ef ff ff ff	callq  0x5fffffe00046
	  0x00005fffffe00057:	90	nop
	  0x00005fffffe00058:	c3	retq
	*/
	if (strcmp(type, "call_dedicated_ret") == 0) {
		if (alignment < 5)
			alignment = 5;
		PUTN(0x90, 63 - 5);
		PUT(0xc3);
		blob->ptr_fn = p;
		PUT(0xe9);
		PUTQ(alignment * cycle_count);

		for (i = 0; i < cycle_count; i++) {
			PUT(0xc3);
			for (j = 1; j < alignment; j++) {
				PUT(0x90);
			}
		}

		for (i = 0; i < cycle_count; i++) {
			for (j = 0; j < 1; j += 5) {
				PUT(0xe8);
				uint32_t x = -alignment * cycle_count - 5;
				PUTQ(x);
			}
			for (; j < alignment; j++) {
				PUT(0x90);
			}
		}
		blob->ptr_ret = p;
		PUT(0xc3);
	}

	/*
	  0x00005fffffe00040:	e8 03 00 00 00	callq  0x5fffffe00048
	  0x00005fffffe00045:	90	nop
	  0x00005fffffe00046:	90	nop
	  0x00005fffffe00047:	90	nop
	  0x00005fffffe00048:	48 83 c4 08	add    $0x8,%rsp
	  0x00005fffffe0004c:	e8 03 00 00 00	callq  0x5fffffe00054
	  0x00005fffffe00051:	90	nop
	  0x00005fffffe00052:	90	nop
	  0x00005fffffe00053:	90	nop
	  0x00005fffffe00054:	48 83 c4 08	add    $0x8,%rsp
	  0x00005fffffe00058:	c3	retq
	*/
	if (strcmp(type, "forward_call_without_ret") == 0) {
		PUTN(0x90, 63);
		PUT(0xc3);
		blob->ptr_fn = p;

		for (i = 0; i < cycle_count; i++) {
			for (j = 0; j < 1; j += 5) {
				PUT(0xe8);
				uint32_t x = alignment < 9 ? 0 : alignment - 9;
				PUTQ(x);
			}
			for (; j < alignment - 4; j++) {
				PUT(0x90);
			}
			PUT(0x48);
			PUT(0x83);
			PUT(0xc4);
			PUT(0x08);
		}
		blob->ptr_ret = p;
		PUT(0xc3);
	}

	/*
	  0x00005fffffe0003e:	c3	retq
	  0x00005fffffe0003f:	90	nop
	  0x00005fffffe00040:	e8 f9 ff ff ff	callq  0x5fffffe0003e
	  0x00005fffffe00045:	90	nop
	  0x00005fffffe00046:	e8 f3 ff ff ff	callq  0x5fffffe0003e
	  0x00005fffffe0004b:	90	nop
	  0x00005fffffe0004c:	c3	retq
	*/
	if (strcmp(type, "call_shared_ret") == 0) {
		PUTN(0x90, 62);
		PUT(0xc3);
		PUT(0x90);

		blob->ptr_fn = p;

		for (i = 0; i < cycle_count; i++) {
			for (j = 0; j < 1; j += 5) {
				PUT(0xe8);
				uint32_t x =
					-(p + 5 - (char *)blob->ptr_fn) - 1;
				PUTQ(x);
			}
			for (; j < alignment; j++) {
				PUT(0x90);
			}
		}
		blob->ptr_ret = p;
		PUT(0xc3);
	}

	/*
	  0x00005fffffe00039:	b9 01 00 00 00	mov    $0x1,%ecx
	  0x00005fffffe0003e:	ff c9	dec    %ecx
	  0x00005fffffe00040:	75 fe	jne    0x5fffffe00040
	  0x00005fffffe00042:	90	nop
	  0x00005fffffe00043:	90	nop
	  0x00005fffffe00044:	90	nop
	  0x00005fffffe00045:	90	nop
	  0x00005fffffe00046:	75 fe	jne    0x5fffffe00046
	  0x00005fffffe00048:	90	nop
	  0x00005fffffe00049:	90	nop
	  0x00005fffffe0004a:	90	nop
	  0x00005fffffe0004b:	90	nop
	  0x00005fffffe0004c:	c3	retq
	*/
	if (strcmp(type, "jne_never_taken") == 0 ||
	    strcmp(type, "jne_never_taken_forward") == 0 ||
	    strcmp(type, "jne_never_taken_backward") == 0) {
		int direction = 0xfe;
		if (strcmp(type, "jne_never_taken_backward") == 0) {
			direction -= alignment;
		}
		if (strcmp(type, "jne_never_taken_forward") == 0) {
			// deliberately invalid opcode
			direction += alignment + 1;
		}
		PUTN(0x90, 63 - 7);
		PUT(0xc3);
		blob->ptr_fn = p;

		char mov_mov_op[] = { 0xb9, 0x01, 0x00, 0x00, 0x00, 0xff, 0xc9 };
		PUTSEQ(mov_mov_op, sizeof(mov_mov_op));

		for (i = 0; i < cycle_count; i++) {
			for (j = 0; j < 2; j += 2) {
				PUT(0x75);
				PUT(direction);
			}
			for (; j < alignment; j++) {
				PUT(0x90);
			}
		}

		blob->ptr_ret = p;
		PUT(0xc3);
	}

	/*
	  0x00005fffffe0003e:	ff c9	dec    %ecx
	  0x00005fffffe00040:	74 02	je     0x5fffffe00044
	  0x00005fffffe00042:	90	nop
	  0x00005fffffe00043:	90	nop
	  0x00005fffffe00044:	74 02	je     0x5fffffe00048
	  0x00005fffffe00046:	90	nop
	  0x00005fffffe00047:	90	nop
	  0x00005fffffe00048:	c3	retq
	*/
	if (strcmp(type, "je_always_taken") == 0) {
		int direction = 0xfe + alignment;
		PUTN(0x90, 63 - 7);
		PUT(0xc3);
		blob->ptr_fn = p;

		char mov_mov_op[] = { 0xb9, 0x01, 0x00, 0x00, 0x00, 0xff, 0xc9 };
		PUTSEQ(mov_mov_op, sizeof(mov_mov_op));

		for (i = 0; i < cycle_count; i++) {
			for (j = 0; j < 2; j += 2) {
				PUT(0x74);
				PUT(direction);
			}
			for (; j < alignment; j++) {
				PUT(0x90);
			}
		}

		blob->ptr_ret = p;
		PUT(0xc3);
	}

	/*
	  0x00005fffffe00040:	eb 04	jmp    0x5fffffe00046
	  0x00005fffffe00042:	eb 04	jmp    0x5fffffe00048
	  0x00005fffffe00044:	90	nop
	  0x00005fffffe00045:	90	nop
	  0x00005fffffe00046:	eb 04	jmp    0x5fffffe0004c
	  0x00005fffffe00048:	eb 04	jmp    0x5fffffe0004e
	  0x00005fffffe0004a:	90	nop
	  0x00005fffffe0004b:	90	nop
	  0x00005fffffe0004c:	c3	retq
	  0x00005fffffe0004d:	90	nop
	  0x00005fffffe0004e:	c3	retq
	  0x00005fffffe0004f:	90	nop
	*/
	if (strcmp(type, "jmp_weaved") == 0) {
		PUTN(0x90, 63);
		PUT(0xc3);
		blob->ptr_fn = p;
		for (i = 0; i < cycle_count; i++) {
			for (j = 0; j < (alignment >= 4 ? 4 : 2); j += 2) {
				PUT(0xeb);
				PUT(alignment - 2);
			}
			for (; j < alignment; j++) {
				PUT(0x90);
			}
		}
		blob->ptr_ret = p;
		for (i = 0; i < 4; i += 2) {
			PUT(0xc3);
			PUT(0x90);
		}
	}

	/*
	  0x00005fffffe0003f:	c3	retq
	  0x00005fffffe00040:	eb 02	jmp    0x5fffffe00044
	  0x00005fffffe00042:	90	nop
	  0x00005fffffe00043:	90	nop
	  0x00005fffffe00044:	eb 02	jmp    0x5fffffe00048
	  0x00005fffffe00046:	90	nop
	  0x00005fffffe00047:	90	nop
	  0x00005fffffe00048:	c3	retq
	*/
	if (strcmp(type, "jmp") == 0) {
		if (alignment < 2) {
			alignment = 2;
		}
		PUTN(0x90, 63);
		PUT(0xc3);
		blob->ptr_fn = p;
		for (i = 0; i < cycle_count; i++) {
			PUT(0xeb);
			PUT(alignment - 2);
			for (j = 2; j < alignment; j++) {
				PUT(0x90);
			}
		}
		blob->ptr_ret = p;
		PUT(0xc3);
	}

	if (strcmp(type, "inc") == 0) {
		PUTN(0x90, 63);
		PUT(0xc3);
		blob->ptr_fn = p;
		for (i = 0; i < cycle_count; i++) {
			PUT(0xff);
			PUT(0xc9);
			for (j = 2; j < alignment; j++) {
				PUT(0x90);
			}
		}
		blob->ptr_ret = p;
		PUT(0xc3);
	}

#elif __aarch64__

	/*
	  0x0000600000000018:	fd 7b bf a9	stp	x29, x30, [sp, #-16]!
	  0x000060000000001c:	05 00 00 14	b	0x600000000030

	  0x0000600000000020:	c0 03 5f d6	ret
	  0x0000600000000024:	d5 03 20 1f	nop
	  0x0000600000000028:	c0 03 5f d6	ret
	  0x000060000000002c:	d5 03 20 1f	nop

	  0x0000600000000030:	fc ff ff 97	bl	0x600000000020
	  0x0000600000000034:	d5 03 20 1f	nop
	  0x0000600000000038:	fc ff ff 97	bl	0x600000000028
	  0x000060000000003c:	d5 03 20 1f	nop
	  0x0000600000000040:	fd 7b c1 a8	ldp	x29, x30, [sp], #16
	  0x0000600000000044:	c0 03 5f d6	ret
	*/
	if (strcmp(type, "call_dedicated_ret") == 0) {
		if (alignment < 4)
			alignment = 4;
		char nop_op[] = { 0xd5, 0x03, 0x20, 0x1f };
		char ret_op[] = { 0xc0, 0x03, 0x5f, 0xd6 };
		for (i = 0; i < 5; i++) {
			PUTSEQ(nop_op, 4);
		}
		PUTSEQ(ret_op, 4);
		blob->ptr_fn = p;
		char stp_x30[] = { 0xfd, 0x7b, 0xbf, 0xa9 };
		PUTSEQ(stp_x30, 4);
		PUTQ3((4 + alignment * cycle_count) / 4);
		PUT(0x14);

		for (i = 0; i < cycle_count; i++) {
			PUTSEQ(ret_op, 4);
			for (j = 4; j < alignment; j += 4) {
				PUTSEQ(nop_op, 4);
			}
		}

		for (i = 0; i < cycle_count; i++) {
			PUTQ3((-alignment * cycle_count) / 4);
			PUT(0x97);

			for (j = 4; j < alignment; j += 4) {
				PUTSEQ(nop_op, 4);
			}
		}

		char ldp_x30[] = { 0xfd, 0x7b, 0xc1, 0xa8 };
		PUTSEQ(ldp_x30, 4);
		blob->ptr_ret = p;
		PUTSEQ(ret_op, 4);
	}

	/*
	  0x0000600000000018:	c0 03 5f d6	ret

	  0x000060000000001c:	fd 7b bf a9	stp	x29, x30, [sp, #-16]!
	  0x0000600000000020:	fe ff ff 97	bl	0x600000000018
	  0x0000600000000024:	d5 03 20 1f	nop
	  0x0000600000000028:	fc ff ff 97	bl	0x600000000018
	  0x000060000000002c:	d5 03 20 1f	nop
	  0x0000600000000030:	fd 7b c1 a8	ldp	x29, x30, [sp], #16
	  0x0000600000000034:	c0 03 5f d6	ret
	*/
	if (strcmp(type, "call_shared_ret") == 0) {
		if (alignment < 4)
			alignment = 4;
		char nop_op[] = { 0xd5, 0x03, 0x20, 0x1f };
		char ret_op[] = { 0xc0, 0x03, 0x5f, 0xd6 };
		for (i = 0; i < 6; i++) {
			PUTSEQ(nop_op, 4);
		}
		PUTSEQ(ret_op, 4);
		blob->ptr_fn = p;
		char stp_x30[] = { 0xfd, 0x7b, 0xbf, 0xa9 };
		PUTSEQ(stp_x30, 4);

		for (i = 0; i < cycle_count; i++) {
			PUTQ3(-(8 + i * alignment) / 4);
			PUT(0x97);

			for (j = 4; j < alignment; j += 4) {
				PUTSEQ(nop_op, 4);
			}
		}

		char ldp_x30[] = { 0xfd, 0x7b, 0xc1, 0xa8 };
		PUTSEQ(ldp_x30, 4);
		blob->ptr_ret = p;
		PUTSEQ(ret_op, 4);
	}

	/*
	  0x000060000000001c:	40 00 80 52	mov	w0, #0x2                   	// #2
	  0x0000600000000020:	1f 2c 00 71	cmp	w0, #0xb
	  0x0000600000000024:	e0 ff ff 54	b.eq	0x600000000020  // b.none
	  0x0000600000000028:	d5 03 20 1f	nop
	  0x000060000000002c:	d5 03 20 1f	nop
	  0x0000600000000030:	e0 ff ff 54	b.eq	0x60000000002c  // b.none
	  0x0000600000000034:	d5 03 20 1f	nop
	  0x0000600000000038:	d5 03 20 1f	nop
	*/
	if (strcmp(type, "jne_never_taken") == 0) {
		if (alignment < 4)
			alignment = 4;
		char nop_op[] = { 0xd5, 0x03, 0x20, 0x1f };
		char ret_op[] = { 0xc0, 0x03, 0x5f, 0xd6 };
		for (i = 0; i < 6; i++) {
			PUTSEQ(nop_op, 4);
		}
		PUTSEQ(ret_op, 4);
		blob->ptr_fn = p;
		char mov_cmp[] = {
			0x40, 0x00, 0x80, 0x52, // mov w0, #0x2
			0x1f, 0x2c, 0x00, 0x71, // cmp w0, #0xb
		};
		PUTSEQ(mov_cmp, 8);

		for (i = 0; i < cycle_count; i++) {
			uint32_t x = -1;
			PUT(((x & 0x7) << 5 | 0x0)); //b.eq
			PUT(x >> 3);
			PUT(x >> 11);
			PUT(0x54);

			for (j = 4; j < alignment; j += 4) {
				PUTSEQ(nop_op, 4);
			}
		}

		blob->ptr_ret = p;
		PUTSEQ(ret_op, 4);
	}

	if (strcmp(type, "je_always_taken") == 0) {
		if (alignment < 4)
			alignment = 4;
		char nop_op[] = { 0xd5, 0x03, 0x20, 0x1f };
		char ret_op[] = { 0xc0, 0x03, 0x5f, 0xd6 };
		for (i = 0; i < 6; i++) {
			PUTSEQ(nop_op, 4);
		}
		PUTSEQ(ret_op, 4);
		blob->ptr_fn = p;
		char mov_cmp[] = {
			0x40, 0x00, 0x80, 0x52, // mov w0, #0xb
			0x1f, 0x2c, 0x00, 0x71, // cmp w0, #0xb
		};
		PUTSEQ(mov_cmp, 8);

		for (i = 0; i < cycle_count; i++) {
			uint32_t x = alignment / 4;
			PUT(((x & 0x7) << 5 | 0x1)); //b.ne
			PUT(x >> 3);
			PUT(x >> 11);
			PUT(0x54);

			for (j = 4; j < alignment; j += 4) {
				PUTSEQ(nop_op, 4);
			}
		}

		blob->ptr_ret = p;
		PUTSEQ(ret_op, 4);
	}

	/*
	  0x0000600000000020:	04 00 00 14	b	0x600000000030
	  0x0000600000000024:	04 00 00 14	b	0x600000000034
	  0x0000600000000028:	d5 03 20 1f	nop
	  0x000060000000002c:	d5 03 20 1f	nop
	  0x0000600000000030:	04 00 00 14	b	0x600000000040
	  0x0000600000000034:	04 00 00 14	b	0x600000000044
	  0x0000600000000038:	d5 03 20 1f	nop
	  0x000060000000003c:	d5 03 20 1f	nop
	  0x0000600000000040:	c0 03 5f d6	ret
	  0x0000600000000044:	c0 03 5f d6	ret
	 */
	if (strcmp(type, "jmp_weaved") == 0) {
		char nop_op[] = { 0xd5, 0x03, 0x20, 0x1f };
		char ret_op[] = { 0xc0, 0x03, 0x5f, 0xd6 };
		for (i = 0; i < 7; i++) {
			PUTSEQ(nop_op, 4);
		}
		PUTSEQ(ret_op, 4);
		blob->ptr_fn = p;

		for (i = 0; i < cycle_count; i++) {
			for (j = 0; j < (alignment >= 8 ? 8 : 4); j += 4) {
				PUT(alignment / 4);
				PUT(0x00);
				PUT(0x00);
				PUT(0x14);
			}
			for (; j < alignment; j += 4) {
				PUTSEQ(nop_op, 4);
			}
		}
		blob->ptr_ret = p;
		for (i = 0; i < 2; i++) {
			PUTSEQ(ret_op, 4);
		}
	}

	/*
	  0x0000600000000020:	04 00 00 14	b	0x600000000030
	  0x0000600000000024:	d5 03 20 1f	nop
	  0x0000600000000028:	d5 03 20 1f	nop
	  0x000060000000002c:	d5 03 20 1f	nop
	  0x0000600000000030:	04 00 00 14	b	0x600000000040
	  0x0000600000000034:	d5 03 20 1f	nop
	  0x0000600000000038:	d5 03 20 1f	nop
	  0x000060000000003c:	d5 03 20 1f	nop
	  0x0000600000000040:	c0 03 5f d6	ret
	 */
	if (strcmp(type, "jmp") == 0) {
		if (alignment < 4)
			alignment = 4;
		char nop_op[] = { 0xd5, 0x03, 0x20, 0x1f };
		char ret_op[] = { 0xc0, 0x03, 0x5f, 0xd6 };
		for (i = 0; i < 7; i++) {
			PUTSEQ(nop_op, 4);
		}
		PUTSEQ(ret_op, 4);
		blob->ptr_fn = p;

		for (i = 0; i < cycle_count; i++) {
			PUT(alignment / 4);
			PUT(0x00);
			PUT(0x00);
			PUT(0x14);
			for (j = 4; j < alignment; j += 4) {
				PUTSEQ(nop_op, 4);
			}
		}
		blob->ptr_ret = p;
		PUTSEQ(ret_op, 4);
	}

	int r = mprotect(blob->ptr, ASSUMED_SZ, PROT_READ | PROT_EXEC);
	if (r) {
		error(-1, errno, "mprotect");
	}
#else
#error bad arch
#endif
	if (p == blob->ptr) {
		error(-1, 0, "bad --type %s", type);
	}
	blob_trap(blob);
}

void blob_exec(struct blob *blob)
{
	void (*fn)(void) = blob->ptr_fn;
	fn();
}

void blob_warm_itlb(struct blob *blob)
{
	/* Exec RET in front of the block */
	void (*fn1)(void) = blob->ptr;
	fn1();
	/* Exec RET at the end of the block */
	void (*fn2)(void) = blob->ptr_ret;
	fn2();
}

void blob_warm_icache(struct blob *blob, int alignment, char *type)
{
#if __x86_64__
	/* On x86_64 we found two ways of warming L1 code cache
	 * without warming up the BTB. One is to run the shadow
	 * version of the code, another is to run the code in real
	 * location, but just run weaved jmp instructions, not the
	 * real ones. Both work and reduce the jitter on the first run
	 * - without BTB warmed up - drastically. */
	if (strcmp(type, "jmp_weaved") == 0) {
		if (alignment < 4) {
			return;
		}
		/* For x86 we can run the offset jumps and this will prime
		 * iTLB/icache without priming BPU - since we're running
		 * differnt branches/jmp than the ones in real benchmark.
		 * Another idea is to run the code on another cpu, assuming
		 * that the shared L2 cache will be primed, while the BPU of
		 * another CPU will remain clear. But this sounds tricky to
		 * execute.
		 */
		/* Not doing this on x86 causes pretty high volatility in
		 * first run of the code. */
		void (*fn3)(void) = (void *)((char *)blob->ptr_fn + 2);
		fn3();
	} else {
		/* For x86 we need ot warm up the icache as well. The best way
		 * is to follow https://stackoverflow.com/a/48574203 advice:
		 * L1I$ is physically addressed, Branch-prediction and uop
		 * caches are virtually addressed, so with the right choice of
		 * virtual addresses, a warm-up run of the function at the
		 * alternate virtual address will prime L1I, but not branch
		 * prediction or uop caches.
		 */
		uint64_t d = (char *)blob->ptr_fn - (char *)blob->ptr;
		void (*fn)(void) = (void *)((char *)blob->ptr_shadow + d);
		fn();
	}
#else
	if (strcmp(type, "jmp_weaved") == 0) {
		if (alignment < 8) {
			return;
		}
		void (*fn3)(void) = (void *)((char *)blob->ptr_fn + 4);
		fn3();
	}
#endif
}
