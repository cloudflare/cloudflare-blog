#define _GNU_SOURCE

#include <errno.h>
#include <getopt.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef linux
#include <error.h>
#include <sys/personality.h>
#elif __APPLE__
#include <pthread.h>
#endif

#include "branch.h"

const char *optstring_from_long_options(const struct option *opt)
{
	static char optstring[256] = { 0 };
	char *osp = optstring;

	for (; opt->name != NULL; opt++) {
		if (opt->flag == 0 && opt->val > 0 && opt->val < 256) {
			*osp++ = opt->val;
			switch (opt->has_arg) {
			case optional_argument:
				*osp++ = ':';
				*osp++ = ':';
				break;
			case required_argument:
				*osp++ = ':';
				break;
			}
		}
	}
	*osp++ = '\0';

	if (osp - optstring >= (int)sizeof(optstring)) {
		abort();
	}
	return optstring;
}

int main(int argc, char **argv)
{
#ifdef linux
	/* [1] Disable ASLR */
	{
		int persona = personality(0xffffffff);
		if ((persona & ADDR_NO_RANDOMIZE) == 0) {
			int r = personality(persona | ADDR_NO_RANDOMIZE);
			if (r == -1) {
				error(-1, errno, "personality()");
			}
			// re-run yourself
			execvp(argv[0], argv);
			perror("execvp()");
			exit(-1);
		}
	}
#endif

	int cycle_count = 1000;
	int alignment = 2;
	int scramble_mode = 0;
	int cpu_number = 0;
	char *type = "jmp";

	/* Here is a problem. On aarch64 the timer is not precise
	 * enough to catch very short loops. Lets run the thing N
	 * times (10?) for small runs. */
	int repeats = 1;

	int use_perf = 0;
	int raw_counter = 0;

	static struct option long_options[] = {
		{ "count", required_argument, 0, 'c' },
		{ "alignment", required_argument, 0, 'a' },
		{ "scramble-mode", required_argument, 0, 's' },
		{ "cpu-pin", required_argument, 0, 'p' },
		{ "type", required_argument, 0, 't' },
		{ "repeats", required_argument, 0, 'r' },
		{ "perf", no_argument, 0, 'x' },
		{ "raw-counter", required_argument, 0, 'w' },
		{ NULL, 0, 0, 0 }
	};

	const char *optstring = optstring_from_long_options(long_options);

	optind = 1;
	while (1) {
		int option_index = 0;

		int arg = getopt_long(argc, argv, optstring, long_options,
				      &option_index);
		if (arg == -1) {
			break;
		}

		switch (arg) {
		default:
			error(-1, errno, "Unknown option: %s", argv[optind]);
			break;
		case 'h':
			error(-1, errno,
			      "Options: --taken --jump-length --scramble-mode");
			break;
		case '?':
			exit(-1);
			break;

		case 'c':
			cycle_count = atoi(optarg);
			break;
		case 'p':
			cpu_number = atoi(optarg);
			break;
		case 'a':
			/* You're allowed to shoot yourself in a foot here. */
			alignment = atoi(optarg);
			break;
		case 's':
			scramble_mode = atoi(optarg);
			break;
		case 'r':
			repeats = atoi(optarg);
			break;
		case 't':
			type = strdup(optarg);
			break;
		case 'x':
			use_perf += 1;
			break;
		case 'w':
			raw_counter = atoi(optarg);
			break;
		}
	}

	if (argv[optind]) {
		error(-1, errno, "Not sure what you mean by %s", argv[optind]);
	}

#ifdef __linux__
	/* [2] Setaffinity */
	{
		cpu_set_t set;
		CPU_ZERO(&set);
		CPU_SET(cpu_number, &set);
		int r = sched_setaffinity(0, sizeof(set), &set);
		if (r == -1) {
			perror("sched_setaffinity([0])");
			exit(-1);
		}
	}
#elif __APPLE__
	{
		/* OSX does not support pinning thread to cpu */
		if (cpu_number == 0) {
			pthread_set_qos_class_self_np(
				QOS_CLASS_USER_INTERACTIVE, 0);
		} else {
			pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND, 0);
		}
	}
#endif

	/* Attempt to clear BTB */
	{
		int r = system("python3 -c 'import math'");
		(void)r;
	}
	struct perf perf;
	memset(&perf, 0, sizeof(struct perf));
	if (use_perf) {
		perf_open(&perf, raw_counter);
	}

	int j;
	struct blob blob;
	memset(&blob, 0, sizeof(struct blob));
	blob_alloc(&blob);
	blob_fill_code(&blob, alignment, cycle_count, type);
	blob_warm_itlb(&blob);
	if (scramble_mode & 0x8) {
		blob_warm_icache(&blob, alignment, type);
	}

	if (repeats > 1 && scramble_mode) {
		fprintf(stderr,
			"repeat with scramble makes no sense, skipping repeat\n");
		repeats = 1;
	}

	uint64_t numbers[20][4] = { { 0 } };
	for (j = 0; j < 20; j++) {
		if (scramble_mode) {
			if (scramble_mode & 0x1) {
				/* This is not effective on mac because we
			 * can't pin to specific cpu core so python is
			 * likely to be launched somewhere else. */
				int r = system("python3 -c 'import math'");
				(void)r;
			}
			if (scramble_mode & 0x2) {
				scramble_btb();
			}
			if (scramble_mode & 0x4) {
				blob_warm_icache(&blob, alignment, type);
			}
		}

		// Warming itlb doens't hurt at all.
		blob_warm_itlb(&blob);

		int k;
		if (!use_perf) {
			uint64_t c0, c1;
			RDTSC_START(c0);
			for (k = 0; k < repeats; k++) {
				blob_exec(&blob);
			}
			RDTSC_STOP(c1);
			uint64_t d = (c1 - c0); // counts full loop latency
			numbers[j][0] = d * RDTSC_UNIT;
		} else {
			perf_start(&perf);
			for (k = 0; k < repeats; k++) {
				blob_exec(&blob);
			}
			perf_stop(&perf, &numbers[j][0]);
		}
	}

	double div = repeats * cycle_count;
	if (use_perf == 0 || (use_perf == 1 && raw_counter == 0)) {
		for (j = 0; j < 20; j++) {
			double d = numbers[j][0] * 1.0 / div;
			printf("%5.3f\n", d);
		}
	} else if (use_perf == 1 && raw_counter != 0) {
		for (j = 0; j < 20; j++) {
			double d = numbers[j][3];
			printf("%5.3f\n", d);
		}
	} else {
		for (j = 0; j < 20; j++) {
			double cycles = numbers[j][0] * 1.0 / div;
			double branches = numbers[j][1] * 1.0; // or raw
			double missed_branches = numbers[j][2] * 1.0;
			double instructions = numbers[j][3] * 1.0;
			printf("%5.3f, %5.3f, %5.3f, %5.3f\n", cycles,
			       instructions, missed_branches, branches);
		}
	}
}
