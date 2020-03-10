#include <errno.h>
#include <error.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifdef NOSSE
#include "csiphash.c"
#else
#include <x86intrin.h>
#endif

/* Input buffer size. Read amount. Limit of line length. */
#define BUF_SZ (256 * 1024)

static char *out_buf;
static int out_buf_pos;

/* Desired fill ratio of the hash table. Resize hash table if it's crossed. */
#define DEFAULT_FILL_RATE 0.6

static void print_usage(int default_hash_size)
{
	printf("Usage: ./mmuniq < input-file > output-file\n"
	       "Filters duplicate lines using a simple probabilistic data structure\n"
	       "Internally uses a hash table, indexed with a fast 64-bit hashes. \n"
	       "Reads newline-delimited data from STDIN, writes unique lines to STDOUT.\n"
	       "The hash table will automatically resize if needed. Due to birthday\n"
	       "paradox and a simplified hash function, the chance of false positive\n"
	       "is relatively high. For large data sets you must assume some unique\n"
	       "input lines may conflict and be missed.\n"
	       "\n"
	       "  -n   Hint about cardinality of input. Default %d\n"
	       "  -f   Desired hash table fill rate. Default %.3f\n"
	       "  -v   Print some stats.\n"
	       "  -D   Print all repeated lines.\n"
	       "  -H   Debug hash.\n"
	       "",
	       default_hash_size, DEFAULT_FILL_RATE);
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

/* Hash table size must be power of two. This is mask. */
static uint64_t global_size_mask;
/* Print only repeated items? */
static int global_repeated;
/* Number of items in hash table */
static uint64_t global_items;
/* Hash table pointer  */
static uint64_t *global_bm;
/* Processed lines */
static uint64_t line_count = 0;

/* For better prefetching we first count hashes and prefetch the hash
 * table. This is how many items to prefetch before actually doing
 * hash table inserts. */
#define C_LINE_SZ 32
struct c_line {
	char *s;
	int l;
	uint64_t h;
} c_lines[C_LINE_SZ];
uint64_t c_writer;
uint64_t c_reader;
static int global_debug_hash;

/* When doing open linear-probing of hash table, give up after
 * hardcoded number of fill buffers. This is to give the program sane
 * termination point even if the hash table gets overfilled. Shout
 * loudly if this is ever reached. */
#define OVERFLOW_AFTER 4096
static uint64_t overflow_count = 0;

static int hash_insert(uint64_t h, uint64_t *bm, uint64_t size_mask)
{
	uint64_t i;
	int miss = -1;
	for (i = h; i < h + OVERFLOW_AFTER; i += 1) {
		uint64_t *v = &bm[i & size_mask];
		if (*v == 0) {
			*v = h;
			miss = 1;
			break;
		} else if (*v == h) {
			miss = 0;
			break;
		}
	}
	if (miss == -1) {
		overflow_count += 1;
	}
	return miss;
}

static void process_flush(char *s, int l, uint64_t h)
{
	int miss = hash_insert(h, global_bm, global_size_mask);
	if (miss == 1) {
		global_items += 1;
	}

	if ((global_repeated == 0 && miss == 1) ||
	    (global_repeated == 1 && miss == 0)) {
		memcpy(&out_buf[out_buf_pos], s, l);
		out_buf_pos += l;
		if (global_debug_hash) {
			int r = sprintf(&out_buf[out_buf_pos], "\t%08lx", h);
			out_buf_pos += r;
		}
		out_buf[out_buf_pos] = '\n';
		out_buf_pos += 1;
		// Assumption is that we'll never go above BUF_SZ+1,
		// and we have 2x BUF_SZ space for debug hash
		if (out_buf_pos > (BUF_SZ * 2 - 1)) {
			abort();
		}
	}
}

static void process_flush_line()
{
	struct c_line *c = &c_lines[c_reader % C_LINE_SZ];
	c_reader += 1;
	process_flush(c->s, c->l, c->h);
}

static int process_line(char *s, int l, uint64_t h, int force)
{
	if (s != NULL) {
		line_count += 1;
		// Issue the prefetch as soon as possible
		__builtin_prefetch(&global_bm[h & global_size_mask]);

		c_lines[c_writer % C_LINE_SZ] = (struct c_line){s, l, h};
		c_writer += 1;
	}

	uint64_t c_size = c_writer - c_reader;
	if (c_size == C_LINE_SZ) {
		process_flush_line();
	}

	if (force) {
		while (c_reader < c_writer) {
			process_flush_line();
		}
	}
	if (force && out_buf_pos) {
		int r = write(1, out_buf, out_buf_pos);
		if (r <= 0) {
			// Broken pipe
			return -1;
		}
		(void)r;
		out_buf_pos = 0;
	}
	return 0;
}

/* Find new line \n and compute hash at the same time. Doing it both
 * saves a bit of CPU on loadu_si128 - we just need to load memory
 * once. */
#ifndef NOSSE
static const __m128i m2_mask = {0x0706050403020100ULL, 0x0f0e0d0c0b0a0908ULL};
static char *find_nl_and_hash(char *b, size_t sz, uint64_t *h_ptr)
{
	__m128i rk0 = {0x736f6d6570736575ULL, 0x646f72616e646f6dULL};
	__m128i rk1 = {0xb549200b5168588fULL, 0xfd5c07200540ada5ULL};
	__m128i hash = rk0;

	char *ret = NULL;
	int ret_sz = sz;
	char *e = &b[sz];
	char *i;
	__m128i q = _mm_set1_epi8('\n');
	for (i = b; i < e; i += 16) {
		__m128i x = _mm_loadu_si128((__m128i *)i);
		if (e - i < 16) {
			// Mask loaded bits out of buffer
			int remainig_sz = e - i;
			__m128i m1 = _mm_set1_epi8(remainig_sz);
			__m128i mask = _mm_cmpgt_epi8(m1, m2_mask);
			x = _mm_and_si128(x, mask);
		}
		__m128i r = _mm_cmpeq_epi8(x, q);
		int z = _mm_movemask_epi8(r);
		int ffs = __builtin_ffs(z);

		__m128i piece = x;
		if (z != 0) {
			// Don't take \n into account when counting hash
			__m128i m1 = _mm_set1_epi8((ffs - 1) & 0x7f);
			__m128i mask = _mm_cmpgt_epi8(m1, m2_mask);
			piece = _mm_and_si128(x, mask);
		}

		// Don't add 0x00 line to the hash if first character
		// is \n. Otherwise add row to hash.
		if ((z & 0x1) == 0) {
			__m128i h_xor_p = _mm_xor_si128(hash, piece);
			__m128i hash_tmp = _mm_aesenc_si128(h_xor_p, rk0);
			hash = _mm_aesenc_si128(hash_tmp, rk1);
		}

		if (z) {
			// \x0a found indeed
			ret = i + ffs - 1;
			ret_sz = ret - b;
			break;
		}
	}

	hash = _mm_aesenc_si128(hash, _mm_set_epi64x(ret_sz, ret_sz));
	*h_ptr = hash[0] ^ hash[1];
	return ret;
}

#else

static char *find_nl_and_hash(char *s, size_t n, uint64_t *h_ptr)
{
	char *c = memchr(s, '\n', n);
	if (c == NULL) {
		*h_ptr = siphash24((const uint8_t *)s, n, 0xa224e4507e645d91,
				   0xe7a9131a4b842813, NULL);
		return NULL;
	}

	size_t sz = c - s;
	*h_ptr = siphash24((const uint8_t *)s, sz, 0xa224e4507e645d91,
			   0xe7a9131a4b842813, NULL);
	;
	return c;
}
#endif

static double fill_rate = DEFAULT_FILL_RATE;
static int global_resizes;

static int global_hugetlb = 0;

static void *do_mmap(size_t sz)
{
	int hugetlb = MAP_HUGETLB;
mmap_again:;
	void *r = mmap(NULL, sz, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE | hugetlb, -1,
		       0);
	if (r == MAP_FAILED && hugetlb) {
		hugetlb = 0;
		goto mmap_again;
	}
	if (r == MAP_FAILED) {
		error(-1, errno, "mmap()");
	}
	global_hugetlb = hugetlb;
	madvise(r, sz, MADV_RANDOM | MADV_WILLNEED | MADV_HUGEPAGE);
	return r;
}

int main(int argc, char **argv)
{
	signal(SIGPIPE, SIG_IGN);

	int verbose = 0;
	uint64_t global_size;

	long l3_cache = sysconf(_SC_LEVEL3_CACHE_SIZE);
	if (l3_cache <= 0) {
		global_size = 4 * 1024 * 1024;
	} else {
		global_size = 4096;
		while (global_size * 2 * 8 < (unsigned long)l3_cache) {
			global_size *= 2;
		}
	}

	{
		static struct option long_options[] = {
			{"n", required_argument, 0, 'n'},
			{"fill-rate", required_argument, 0, 'f'},
			{"verbose", no_argument, 0, 'v'},
			{"all-repeated", no_argument, 0, 'D'},
			{"debug-hash", no_argument, 0, 'H'},
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
				print_usage(global_size);
				exit(1);
				break;
			case 'n':
				global_size = llabs(atoll(optarg));
				break;
			case 'f':
				fill_rate = atof(optarg);
				if (fill_rate < 0.1 || fill_rate > 0.9) {
					fill_rate = 0.6;
				}
				break;
			case 'v':
				verbose = 1;
				break;
			case 'D':
				global_repeated = 1;
				break;
			case 'H':
				global_debug_hash = 1;
				break;
			}
		}
	}

	global_size_mask = 1;
	while (global_size_mask < global_size) {
		global_size_mask <<= 1;
	}
	global_size_mask -= 1;

	size_t bm_sz = (global_size_mask + 1) * 8;
	global_bm = do_mmap(bm_sz);

	uint64_t fill_threshold = fill_rate * (double)(global_size_mask + 1);

	fflush(stdout);
	out_buf = calloc(1, BUF_SZ * 2);
	out_buf_pos = 0;

	if (verbose == 1) {
		fprintf(stderr, "[.] Start parameters: ");
		fprintf(stderr, "n=%lu size=%luMiB ", global_size_mask + 1,
			(global_size_mask + 1) * 8 / 1024 / 1024);
		fprintf(stderr, "desired fill rate=%.3f\n", fill_rate);
	}

	char buf[BUF_SZ] = {0};
	int buf_pos = 0;
	int res = 0;
	while (res == 0) {
		int r = read(0, &buf[buf_pos], BUF_SZ - buf_pos);
		if (r < 0) {
			break;
		}

		int buf_sz = buf_pos + r;
		buf_pos = 0;
		while (1) {
			uint64_t h;
			char *nl = find_nl_and_hash(&buf[buf_pos],
						    buf_sz - buf_pos, &h);
			if (nl == NULL) {
				break;
			}
			int nl_idx = nl - &buf[0];
			int sz = nl_idx - buf_pos;
			res |= process_line(&buf[buf_pos], sz, h, 0);
			buf_pos = nl_idx + 1;
		}
		res |= process_line(NULL, 0, 0, 1);

		memmove(&buf[0], &buf[buf_pos], buf_sz - buf_pos);
		buf_pos = buf_sz - buf_pos;

		// Filled buffer, no new line in sight
		if (buf_pos == BUF_SZ || r == 0) {
			uint64_t h;
			// We know there is no \n in the buffer
			find_nl_and_hash(&buf[0], BUF_SZ, &h);
			res |= process_line(&buf[0], BUF_SZ, h, 1);
			buf_pos = 0;
			if (r == 0) {
				break;
			}
		}

		if (global_items > fill_threshold) {
			global_resizes += 1;
			uint64_t size_mask_new =
				((global_size_mask + 1) * 2) - 1;
			size_t bm_sz_new = (size_mask_new + 1) * 8;
			uint64_t *bm_new = do_mmap(bm_sz_new);
			uint64_t j;
			for (j = 0; j < global_size_mask; j++) {
				uint64_t h = global_bm[j];
				if (h == 0) {
					continue;
				}
				hash_insert(h, bm_new, size_mask_new);
			}

			munmap(global_bm, bm_sz);
			global_bm = bm_new;
			bm_sz = bm_sz_new;
			global_size_mask = size_mask_new;
			fill_threshold =
				fill_rate * (double)(size_mask_new + 1);
		}
	}
	fflush(stdout);

	if (overflow_count) {
		fprintf(stderr,
			"[!] Overflow detected %ld times. Potentially lost "
			"lines.\n",
			overflow_count);
	}
	if (verbose == 1) {
		fprintf(stderr,
			"[.] %ldm lines processed."
			" Unique items: %ldm."
			" Hash table fill rate %.3f,"
			" resized %d times,"
			" size %ldMiB%s.\n",
			line_count / 1000000, global_items / 1000000,
			(double)global_items / (global_size_mask + 1),
			global_resizes,
			(global_size_mask + 1) * 8 / 1024 / 1024,
			global_hugetlb ? " (hugetlb)" : "");
	}
	munmap(global_bm, bm_sz);
	return 0;
}
