#include <byteswap.h>
#include <errno.h>
#include <error.h>
#include <getopt.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "csiphash.c"

/* Input buffer size. Limit of line length. */
#define BUF_SZ (1024 * 1024)

/* Stdout buffer when piping to not-tty. */
#define STDOUTBUF_SZ (1024 * 1024)

#define DEFAULT_HASH_SIZE 500000000

static void print_usage(void)
{
	printf("Usage: ./mmuniq < input-file > output-file\n"
	       "Filters duplicate lines using a simple hash table, indexed\n"
	       "by 64-bit hash function. Reads newline-delimited data from\n"
	       "STDIN, writes unique lines to STDOUT.\n"
	       "\n"
	       "  -n   Estimated cardinality of input. Default %d\n"
	       "  -q   Quiet.\n"
	       "  -D   Print all repeated lines.\n"
	       "",
	       DEFAULT_HASH_SIZE);
}

const char *optstring_from_long_options(const struct option *opt)
{
	static char optstring[256] = {0};
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

static uint64_t global_size = DEFAULT_HASH_SIZE;
static uint64_t global_size_mask;
static int global_repeated;

static const uint64_t k0 = 0xdeadbeeffadebabeULL;
static const uint64_t k1 = 0xdeadbeeffadebabeULL;

static int overflow_count = 0;
static int hash_conflicts = 0;
static int line_count = 0;

static void process_line(char *s, int l, uint64_t *bm)
{
	line_count += 1;

	int miss = -1;
	uint64_t h = siphash24((const uint8_t *)s, l, k0, k1, NULL);

	uint64_t i = 0;
	for (i = h; i < h + 4096; i += 1) {
		uint64_t *v = &bm[i & global_size_mask];
		if (*v == 0) {
			*v = h;
			miss = 1;
			break;
		} else if (*v == h) {
			miss = 0;
			break;
		}
		hash_conflicts += 1;
	}

	if (miss == -1) {
		overflow_count += 1;
		miss = 0;
	}

	if ((global_repeated == 0 && miss == 1) ||
	    (global_repeated == 1 && miss == 0)) {
		fwrite(s, l, 1, stdout);
		fputc('\n', stdout);
	}
}

int main(int argc, char **argv)
{
	int quiet = 0;

	{
		static struct option long_options[] = {
			{"n", required_argument, 0, 'n'},
			{"q", no_argument, 0, 'q'},
			{"all-repeated", no_argument, 0, 'D'},
			{"help", no_argument, 0, 'h'},
			{NULL, 0, 0, 0}};
		optind = 1;
		while (1) {
			int option_index = 0;
			int arg = getopt_long(
				argc, argv,
				optstring_from_long_options(long_options),
				long_options, &option_index);
			if (arg == -1) {
				break;
			}

			switch (arg) {
			default:
			case 0:
				fprintf(stderr, "Unknown option: %s",
					argv[optind]);
				exit(-1);
				break;
			case '?':
				exit(-1);
				break;
			case 'h':
				print_usage();
				exit(1);
				break;
			case 'n':
				global_size = llabs(atoll(optarg));
				break;
			case 'q':
				quiet = 1;
				break;
			case 'D':
				global_repeated = 1;
				break;
			}
		}
	}

	global_size_mask = 1;
	while (global_size_mask <= global_size) {
		global_size_mask <<= 1;
	}
	global_size_mask -= 1;

	size_t bm_sz = (global_size_mask + 1) * 8;
	uint64_t *bm = mmap(NULL, bm_sz, PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);

	if (bm == MAP_FAILED) {
		error(-1, errno, "mmap()");
	}

	if (isatty(1) == 0) {
		char *stdoutbuff = calloc(1, STDOUTBUF_SZ);
		fflush(stdout);
		setvbuf(stdout, stdoutbuff, _IOFBF, STDOUTBUF_SZ);
	}

	if (quiet == 0) {
		fprintf(stderr, "[.] Parameters: ");
		fprintf(stderr, "n=%lu size=%luMiB\n", global_size_mask + 1,
			(global_size_mask + 1) / 1024 / 1024);
	}

	char buf[BUF_SZ];
	int buf_pos = 0;
	while (1) {
		int r = read(0, &buf[buf_pos], BUF_SZ - buf_pos);
		if (r <= 0) {
			break;
		}

		int buf_sz = buf_pos + r;
		buf_pos = 0;
		while (1) {
			char *nl =
				memchr(&buf[buf_pos], '\n', buf_sz - buf_pos);
			if (nl == NULL) {
				break;
			}
			int nl_idx = nl - &buf[0];
			int sz = nl_idx - buf_pos;
			process_line(&buf[buf_pos], sz, bm);
			buf_pos = nl_idx + 1;
		}
		memmove(&buf[0], &buf[buf_pos], buf_sz - buf_pos);
		buf_pos = buf_sz - buf_pos;

		// Filled buffer, no new line in sight
		if (buf_pos == BUF_SZ) {
			process_line(&buf[0], BUF_SZ, bm);
			buf_pos = 0;
		}
	}
	fflush(stdout);

	if (overflow_count) {
		fprintf(stderr, "[!] Overflow detected %d times\n",
			overflow_count);
	}
	if (quiet == 0 && hash_conflicts) {
		fprintf(stderr, "[.] Hash conflicts %.3f per line\n",
			(double)hash_conflicts / line_count);
	}
	munmap(bm, bm_sz);
	return 0;
}
