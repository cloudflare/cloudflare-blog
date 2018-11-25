/*
 * Compile with
 *  gcc -msse4.1 -ggdb -O3 -Wall -Wextra measure-dram.c -o measure-dram \
 *           && objdump -dx measure-dram | egrep 'movntdqa' -A 2 -B 2
 */
#define _GNU_SOURCE // setaffinity

#include <fcntl.h>
#include <sched.h>
#include <smmintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <time.h>
#include <unistd.h>

inline static uint64_t rdtsc() {
	register uint32_t lo, hi;
	asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
	return ((uint64_t)lo | ((uint64_t)hi << 32));
}

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
		perror("execvp()");
		exit(-1);
	}

	/* For confirmation, print stack and code segments */
	fprintf(stderr, "[*] Verifying ASLR: main=%p stack=%p\n", main, &persona);

	/* [2] Setaffinity */
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(0, &set);
	int r = sched_setaffinity(0, sizeof(set), &set);
	if (r == -1) {
		perror("sched_setaffinity([0])");
		exit(-1);
	}
	fprintf(stderr, "[*] CPU affinity set to CPU #0\n");

	/* [3] Mmap  */
	void *addr = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_POPULATE|MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		perror("mmap()");
		exit(-1);
	}

	int *one_global_var = (int*)addr;
	_mm_clflush(one_global_var);

	/* [4] Spin the CPU to allow frequency scaling to settle down. */
	uint64_t base, rt0, rt1, cl0, cl1;
	rt0 = realtime_now();
	cl0 = rdtsc();
	int i;
	for (i=0; 1; i++) {
		rt1 = realtime_now();
		if (rt1 - rt0 > 1000000000){
			break;
		}
	}
	cl1 = rdtsc();
	fprintf(stderr, "[ ] Fun fact. clock_gettime() takes roughly %.1fns and %.1f cycles\n", 1000000000.0 / i, (cl1-cl0)*1.0 / i);

	/* [5] Do the work! */
	base = realtime_now();
	rt0 = base;

	#define DELTAS_SZ 32768*4
	struct {
		uint32_t t; // timestamp
		uint32_t d; // duration
	} deltas[DELTAS_SZ];

	fprintf(stderr, "[*] Measuring MOV + CLFLUSH. Running %d iterations.\n", DELTAS_SZ);
	for (i = 0; i < DELTAS_SZ; i++) {
		// Perform memory load. Any will do/
		*(volatile int *) one_global_var;

		// Flush CPU cache. This is relatively slow
		_mm_clflush(one_global_var);

		// mfence is needed, otherwise sometimes the loop
		// takes very short time (25ns instead of like 160). I
		// blame reordering.
		asm volatile("mfence");

		rt1 = realtime_now();
		uint64_t td = rt1 - rt0;
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
