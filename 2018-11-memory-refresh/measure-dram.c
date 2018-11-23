/*
 * Compile with
 *  gcc -msse4.1 -ggdb -O3 -Wall -Wextra measure-dram.c -o measure-dram \
 *           && objdump -dx measure-dram | egrep 'movntdqa' -A 2 -B 2
 */
#define _GNU_SOURCE // setaffinity

#include <emmintrin.h>
#include <fcntl.h>
#include <sched.h>
#include <smmintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define TIMESPEC_NSEC(ts) ((ts)->tv_sec * 1000000000ULL + (ts)->tv_nsec)
inline static uint64_t realtime_now()
{
	struct timespec now_ts;
	clock_gettime(CLOCK_MONOTONIC, &now_ts);
	return TIMESPEC_NSEC(&now_ts);
}

int main(int argc, char *argv[]){
	(void)argc;

	/* [1] Disable ASLR */
	int persona = personality(0xffffffff);
	if ((persona & ADDR_NO_RANDOMIZE) == 0) {
		int r = personality(persona | ADDR_NO_RANDOMIZE);
		if (r == -1) {
			perror("personality()");
			exit(-1);
		}
		// re-run yourself
		execvp(argv[0], argv);
	}

	/* For confirmation, print stack and code segments */
	fprintf(stderr, "[*] Verifying ASLR: main=%p stack=%p\n", main, &persona);

	/* [2] Setaffinity */
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(2, &set);
	int r = sched_setaffinity(0, sizeof(set), &set);
	if (r == -1) {
		perror("sched_setaffinity([0])");
		exit(-1);
	}

	/* [3] Mmap  */
	void *addr = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_POPULATE|MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		perror("mmap()");
		exit(-1);
	}

	/* It's fun exercise to try to mmap not WriteBack memory
	 * (uncacheable in MTRR). I failed to do it from userpace so
	 * far. Google says this might possibly work: */
	if (0) {
		int fd = open("/dev/mem", O_RDWR|O_SYNC);
		if (fd < 0) {
			perror("open(/dev/mem)");
			exit(-1);
		}
		void *uptr = mmap(addr, 4096, PROT_READ|PROT_WRITE, MAP_POPULATE|MAP_SHARED|MAP_LOCKED, fd, 0);
		if (uptr == MAP_FAILED) {
			perror("mmap()");
			exit(-1);
		}
		addr = uptr;
	}

	__m128i *one_global_var = (__m128i*)addr;
	_mm_clflush(one_global_var);

	/* [4] Spin the CPU to allow frequency scaling to kick in. */
	uint64_t base, rt0, rt1;
	base = realtime_now();
	int i;
	for (i=0; 1; i++) {
		rt1 = realtime_now();
		if (rt1 - base > 1000000000){
			break;
		}
	}
	fprintf(stderr, "[ ] Fun fact. I did %d clock_gettime()'s per second\n", i);

	/* [5] Do the work! */
	base = realtime_now();
	rt0 = base;

	#define DELTAS_SZ 32768*4
	struct {
		uint32_t t; // timestamp
		uint32_t d; // duration
	} deltas[DELTAS_SZ];

	fprintf(stderr, "[*] Measuring MOVNTDQA + CLFLUSH time. Running %d iterations.\n", DELTAS_SZ);
	for (i = 0; i < DELTAS_SZ; i++) {
		__m128i x;
		// Perform data load. MOVNT is here only for kicks,
		// for writeback memory it makes zero difference, the
		// caches are still polluted.
		asm volatile("movntdqa (%1), %0" :  "=x" (x) : "a" (one_global_var));
		asm volatile("" :  : "x" ( x ));
		_mm_clflush(one_global_var);
		asm volatile("mfence");

		rt1 = realtime_now();
		uint64_t td = rt1-rt0;
		rt0 = rt1;
		deltas[i].t = rt1 - base;
		deltas[i].d = td;
	}
	fprintf(stderr, "[*] Writing out data\n");
	for (i = 0; i < DELTAS_SZ; i++){
		printf("%u,\t%u\n", deltas[i].t, deltas[i].d);
	}
	return 0;
}
