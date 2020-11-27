#include <errno.h>
#include <error.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <x86intrin.h>

#include "common.h"

#define BATCH_SIZE 6

// Must be multiple of 16
#define MIXER_SIZE 32

static uint32_t distance_avx2(uint8_t *a, uint8_t *b, int len)
{
	__m256i zero = _mm256_setzero_si256();
	__m256i sum = zero;
	int i;
	for (i = 0; i < len; i += 16) {
		__m128i a_16x8 = _mm_loadu_si128((__m128i *)&a[i]);
		__m128i b_16x8 = _mm_loadu_si128((__m128i *)&b[i]);

		__m256i a_16x16 = _mm256_cvtepu8_epi16(a_16x8);
		__m256i b_16x16 = _mm256_cvtepu8_epi16(b_16x8);
		__m256i d_16x16 = _mm256_sub_epi16(a_16x16, b_16x16);

		__m256i sq_8x32 = _mm256_madd_epi16(d_16x16, d_16x16); // 8
		sum = _mm256_add_epi32(sum, sq_8x32);
	}
	__m256i e = sum;
	e = _mm256_hadd_epi32(e, zero); // 8 -> 4
	e = _mm256_hadd_epi32(e, zero); // 4 -> 2

	uint32_t sc1 = _mm256_extract_epi32(e, 0);
	uint32_t sc2 = _mm256_extract_epi32(e, 4);
	return sc1 + sc2;
}

struct shorth {
	uint8_t hashes[0][MIXER_SIZE];
};

int main(int argc, char **argv)
{
	uint32_t threshold = 0xffffffff;
	if (argc > 1) {
		threshold = atoi(argv[1]);
	}
	struct mmdist *state;
	int r = mmdist_load("database.txt", &state);
	if (r != 0) {
		error(-1, errno, "database.txt load");
	}

	struct shorth *shorth =
		calloc(1, sizeof(struct shorth) + state->items * MIXER_SIZE);
	int i;
	for (i = 0; i < state->items; i++) {
		// First X bytes
		memcpy(shorth->hashes[i], state->hashes[i], MIXER_SIZE);
	}

	uint64_t ts0_ns = realtime_now();
	int count = 0;
	while (1) {
		uint8_t query[BATCH_SIZE][144];
		int j;
		for (j = 0; j < BATCH_SIZE; j++) {
			uint8_t line[1024];
			char *fr = fgets((char *)line, sizeof(line), stdin);
			if (fr == NULL) {
				break;
			}
			line[1023] = 0;

			r = parse_hex(query[j], line, 144);
			if (r != 144) {
				error(-1, errno, "bad line, hex length");
			}
		}
		int batch_size = j;

		uint32_t min_dist[BATCH_SIZE];
		int min_idx[BATCH_SIZE];
		for (j = 0; j < BATCH_SIZE; j++) {
			min_dist[j] = 0xffffffff;
			min_idx[j] = -1;
		}
		int i;
		for (i = 0; i < state->items; i++) {
			uint8_t *short_hash = shorth->hashes[i];
			uint8_t *full_hash = state->hashes[i];
			for (j = 0; j < batch_size; j++) {
				uint32_t short_dist = distance_avx2(
					query[j], short_hash, MIXER_SIZE);
				if (short_dist > min_dist[j] ||
				    short_dist > threshold) {
					continue;
				}

				uint32_t full_dist =
					short_dist +
					distance_avx2(&query[j][MIXER_SIZE],
						      &full_hash[MIXER_SIZE],
						      144 - MIXER_SIZE);
				if (full_dist < min_dist[j] &&
				    full_dist < threshold) {
					min_dist[j] = full_dist;
					min_idx[j] = i;
				}
			}
		}
		for (j = 0; j < batch_size; j++) {
			printf("sq_dist=%d idx=%d\n", min_dist[j], min_idx[j]);
		}
		count += batch_size;
		if (batch_size == 0) {
			break;
		}
	}
	uint64_t td = realtime_now() - ts0_ns;
	uint64_t nsperquery = td / count;
	fprintf(stderr,
		"Total: %.3fms, %d items, avg %.3fms per query, %.3f qps\n",
		td / 1000000., count, nsperquery / 1000000.,
		1000000000. / nsperquery);
}
