#include <stdarg.h>
#include <time.h>

#ifdef __aarch64__
/* this also works on apple. see mach_absolute_time assembly:
https://opensource.apple.com/source/xnu/xnu-7195.50.7.100.1/libsyscall/wrappers/mach_absolute_time.s
 */
#define RDTSC_START(c)                                                         \
	do {                                                                   \
		asm volatile("mrs %0, cntvct_el0" : "=r"(c)::"memory");        \
	} while (0)
#define RDTSC_STOP(c) RDTSC_START(c)
#define RDTSC_UNIT                                                             \
	({                                                                     \
		uint64_t c;                                                    \
		asm volatile("mrs %0, cntfrq_el0" : "=r"(c));                  \
		(1000000000 / c);                                              \
	});

#elif __x86_64__
#define RDTSC_DIRTY "%rax", "%rbx", "%rcx", "%rdx"
#define RDTSC_START(cycles)                                                    \
	do {                                                                   \
		register unsigned cyc_high, cyc_low;                           \
		asm volatile("CPUID\n\t"                                       \
			     "RDTSC\n\t"                                       \
			     "mov %%edx, %0\n\t"                               \
			     "mov %%eax, %1\n\t"                               \
			     : "=r"(cyc_high), "=r"(cyc_low)::RDTSC_DIRTY);    \
		(cycles) = ((uint64_t)cyc_high << 32) | cyc_low;               \
	} while (0)

#define RDTSC_STOP(cycles)                                                     \
	do {                                                                   \
		register unsigned cyc_high, cyc_low;                           \
		asm volatile("RDTSCP\n\t"                                      \
			     "mov %%edx, %0\n\t"                               \
			     "mov %%eax, %1\n\t"                               \
			     "CPUID\n\t"                                       \
			     : "=r"(cyc_high), "=r"(cyc_low)::RDTSC_DIRTY);    \
		(cycles) = ((uint64_t)cyc_high << 32) | cyc_low;               \
	} while (0)
#define RDTSC_UNIT 1
#else
#error unknown platform
#endif

struct blob {
	int memfd;
	void *ptr;
	int hugepage;

	void *ptr_fn;
	void *ptr_ret;
	void *ptr_shadow;
};

void blob_alloc(struct blob *blob);
void blob_fill_code(struct blob *blob, int alignment, int cycle_count,
		    char *type);
void blob_exec(struct blob *blob);
void blob_warm_itlb(struct blob *blob);
void blob_warm_icache(struct blob *blob, int alignment, char *type);

#if __APPLE__
static void error(int status, int errnum, const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	errno = errnum;
	perror(buf);
	if (status)
		exit(status);
}
#endif

void scramble_btb();

#define G_COUNTERS_COUNT 10

struct perf {
	uint64_t g_counters[G_COUNTERS_COUNT];
	int fd[16];
	uint64_t cycles;
	uint64_t branches;
	uint64_t missed_branches;
	uint64_t instructions;
};

void perf_open(struct perf *perf, int raw_counter);
void perf_start(struct perf *perf);
void perf_stop(struct perf *perf, uint64_t *numbers);
