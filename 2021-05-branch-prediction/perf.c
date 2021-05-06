#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include "branch.h"

#if __linux__
#include <error.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
			    int cpu, int group_fd, unsigned long flags)
{
	int ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd,
			  flags);
	return ret;
}

/* Notice: the layout of this struct depends on the flags passed to perf_event_open. */
struct read_format {
	uint64_t nr;
	struct {
		uint64_t value;
	} values[];
};

struct events {
	uint32_t type;
	uint32_t config;
	char *name;
};

struct events events[] = {
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, "cycles" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, "instructions" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES, "missed_branches" },
	// the following don't work on arm64
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS, "branches" },
};

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#define EVENTS_SZ ((int)ARRAY_SIZE(events))

void perf_open(struct perf *perf, int raw_counter)
{
	if (ARRAY_SIZE(perf->fd) < EVENTS_SZ) {
		abort();
	}

	perf->fd[0] = -1;
	int i;
	for (i = 0; i < EVENTS_SZ; i++) {
		struct perf_event_attr pe = {
			.type = events[i].type,
			.size = sizeof(struct perf_event_attr),
			.config = events[i].config,
			.disabled = 1,
			.exclude_kernel = 1,
			.exclude_guest = 1,
			.exclude_hv = 1,
			.read_format = PERF_FORMAT_GROUP,
			.sample_type = PERF_SAMPLE_IDENTIFIER,
		};
		if (i == 3 && raw_counter) {
			pe.type = PERF_TYPE_RAW;
			pe.config = raw_counter;
			events[i].name = "raw_counter";
		}
		perf->fd[i] = perf_event_open(&pe, 0, -1, perf->fd[0],
					      PERF_FLAG_FD_CLOEXEC);
		if (perf->fd[i] == -1) {
			// allow for branches/raw_counter to fail
			if (i == 3)
				continue;
			fprintf(stderr, "Error opening perf %s\n",
				events[i].name);
			exit(EXIT_FAILURE);
		}
	}
}

inline void perf_start(struct perf *perf)
{
	asm volatile("" ::: "memory");
	ioctl(perf->fd[0], PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
	ioctl(perf->fd[0], PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
	asm volatile("" ::: "memory");
}

inline void perf_stop(struct perf *perf, uint64_t *numbers)
{
	asm volatile("" ::: "memory");
	ioctl(perf->fd[0], PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
	char perf_buf[4096];
	int r = read(perf->fd[0], &perf_buf, sizeof(perf_buf));
	if (r < 1)
		error(-1, errno, "read(perf)");

	struct read_format *rf = (struct read_format *)perf_buf;
	switch (rf->nr) {
	case 4:
		numbers[1] = rf->values[3].value; // branches
		/* fallthrough */
	case 3:
		numbers[0] = rf->values[0].value; // cycles
		numbers[3] = rf->values[1].value; // instructions
		numbers[2] = rf->values[2].value; // missed_branches
		break;
	default:
		abort();
	}
}

#elif __APPLE__

/*
  Sources:
  https://github.com/lemire/Code-used-on-Daniel-Lemire-s-blog/blob/master/2021/03/24/m1cycles.cpp
  https://gist.github.com/dougallj/5bafb113492047c865c0c8cfbc930155#file-m1_robsize-c-L390
  http://blog.stuffedcow.net/2013/05/measuring-rob-capacity
*/

#include <dlfcn.h>
#include <pthread.h>

#define KPERF_LIST                                                             \
	/*  ret, name, params */                                               \
	F(int, kpc_get_counting, void)                                         \
	F(int, kpc_force_all_ctrs_set, int)                                    \
	F(int, kpc_set_counting, uint32_t)                                     \
	F(int, kpc_set_thread_counting, uint32_t)                              \
	F(int, kpc_set_config, uint32_t, void *)                               \
	F(int, kpc_get_config, uint32_t, void *)                               \
	F(int, kpc_set_period, uint32_t, void *)                               \
	F(int, kpc_get_period, uint32_t, void *)                               \
	F(uint32_t, kpc_get_counter_count, uint32_t)                           \
	F(uint32_t, kpc_get_config_count, uint32_t)                            \
	F(int, kperf_sample_get, int *)                                        \
	F(int, kpc_get_thread_counters, int, unsigned int, void *)

#define F(ret, name, ...)                                                      \
	typedef ret name##proc(__VA_ARGS__);                                   \
	static name##proc *name;
KPERF_LIST
#undef F

#define CFGWORD_EL0A32EN_MASK (0x10000)
#define CFGWORD_EL0A64EN_MASK (0x20000)
#define CFGWORD_EL1EN_MASK (0x40000)
#define CFGWORD_EL3EN_MASK (0x80000)
#define CFGWORD_ALLMODES_MASK (0xf0000)

#define CPMU_NONE 0
#define CPMU_CORE_CYCLE 0x02
#define CPMU_INST_A64 0x8c
#define CPMU_INST_BRANCH 0x8d
#define CPMU_SYNC_DC_LOAD_MISS 0xbf
#define CPMU_SYNC_DC_STORE_MISS 0xc0
#define CPMU_SYNC_DTLB_MISS 0xc1
#define CPMU_SYNC_ST_HIT_YNGR_LD 0xc4
#define CPMU_SYNC_BR_ANY_MISP 0xcb
#define CPMU_FED_IC_MISS_DEM 0xd3
#define CPMU_FED_ITLB_MISS 0xd4

#define KPC_CLASS_FIXED (0)
#define KPC_CLASS_CONFIGURABLE (1)
#define KPC_CLASS_POWER (2)
#define KPC_CLASS_RAWPMU (3)
#define KPC_CLASS_FIXED_MASK (1u << KPC_CLASS_FIXED)
#define KPC_CLASS_CONFIGURABLE_MASK (1u << KPC_CLASS_CONFIGURABLE)
#define KPC_CLASS_POWER_MASK (1u << KPC_CLASS_POWER)
#define KPC_CLASS_RAWPMU_MASK (1u << KPC_CLASS_RAWPMU)

#define CONFIG_COUNT 8
#define KPC_MASK (KPC_CLASS_CONFIGURABLE_MASK | KPC_CLASS_FIXED_MASK)

static void init_rdtsc()
{
	void *kperf = dlopen(
		"/System/Library/PrivateFrameworks/kperf.framework/Versions/A/kperf",
		RTLD_LAZY);
	if (!kperf) {
		error(-1, errno, "kperf = %p\n", kperf);
		return;
	}
#define F(ret, name, ...)                                                      \
	name = (name##proc *)(dlsym(kperf, #name));                            \
	if (!name) {                                                           \
		printf("%s = %p\n", #name, (void *)name);                      \
		return;                                                        \
	}
	KPERF_LIST
#undef F

	if (kpc_get_counter_count(KPC_MASK) != G_COUNTERS_COUNT) {
		error(-1, errno, "wrong fixed counters count");
		return;
	}

	if (kpc_get_config_count(KPC_MASK) != CONFIG_COUNT) {
		error(-1, errno, "wrong fixed config count");
		return;
	}

	uint64_t g_config[G_COUNTERS_COUNT] = { 0 };
	g_config[0] = CPMU_CORE_CYCLE | CFGWORD_EL0A64EN_MASK;
	g_config[3] = CPMU_INST_BRANCH | CFGWORD_EL0A64EN_MASK;
	g_config[4] = CPMU_SYNC_BR_ANY_MISP | CFGWORD_EL0A64EN_MASK;
	g_config[5] = CPMU_INST_A64 | CFGWORD_EL0A64EN_MASK;

	if (kpc_set_config(KPC_MASK, g_config)) {
		error(-1, errno, "kpc_set_config failed");
		return;
	}

	if (kpc_force_all_ctrs_set(1)) {
		error(-1, errno, "kpc_force_all_ctrs_set failed");
		return;
	}

	if (kpc_set_counting(KPC_MASK)) {
		error(-1, errno, "kpc_set_counting failed");
		return;
	}

	if (kpc_set_thread_counting(KPC_MASK)) {
		error(-1, errno, "kpc_set_thread_counting failed");
		return;
	}
}

void perf_open(struct perf *perf, int raw_counter)
{
	(void)perf;
	(void)raw_counter;
	init_rdtsc();
}

inline void perf_start(struct perf *perf)
{
	if (kpc_force_all_ctrs_set(1)) {
		error(-1, errno, "kpc_force_all_ctrs_set failed");
	}
	if (kpc_get_thread_counters(0, G_COUNTERS_COUNT, perf->g_counters)) {
		error(-1, errno,
		      "kpc_get_thread_counters failed, run as sudo?");
	}
	asm volatile("" ::: "memory");
}

inline void perf_stop(struct perf *perf, uint64_t *numbers)
{
	asm volatile("" ::: "memory");
	uint64_t g_counters[G_COUNTERS_COUNT];
	if (kpc_get_thread_counters(0, G_COUNTERS_COUNT, g_counters)) {
		error(-1, errno,
		      "kpc_get_thread_counters failed, run as sudo?");
	}

	numbers[0] = g_counters[0 + 2] - perf->g_counters[0 + 2]; // cycles
	numbers[1] = g_counters[3 + 2] - perf->g_counters[3 + 2]; // branches
	numbers[2] =
		g_counters[4 + 2] - perf->g_counters[4 + 2]; // missed_branches
	numbers[3] =
		g_counters[5 + 2] - perf->g_counters[5 + 2]; // instructions
}

#endif
