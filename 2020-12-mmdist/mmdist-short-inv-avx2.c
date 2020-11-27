#include <errno.h>
#include <error.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <x86intrin.h>

#include "common.h"

#define BATCH_SIZE 6

#define MIXER_SIZE 24

// SIMD register size
#define LANE_SIZE 16

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

	if (state->items % LANE_SIZE != 0) {
		error(-1, 0, "database ALIGN ERROR");
	}

	uint8_t *sv = calloc(1, state->items * MIXER_SIZE);
	int i;
	// shuffle, transpose
	for (i = 0; i < state->items; i += LANE_SIZE) {
		uint8_t *b = &sv[i * MIXER_SIZE];
		int j, k;
		for (j = 0; j < LANE_SIZE; j++) {
			uint8_t *c = state->hashes[i + j];
			for (k = 0; k < MIXER_SIZE; k++) {
				b[k * LANE_SIZE + j] = c[k];
			}
		}
	}

	if (threshold >= UINT16_MAX) {
		threshold = UINT16_MAX - 1;
	}

	uint64_t ts0_ns = realtime_now();
	int count = 0;
	while (1) {
		uint8_t query[BATCH_SIZE][144] = {};
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
		int c;
		for (c = 0; c < BATCH_SIZE; c++) {
			min_dist[c] = threshold;
			min_idx[c] = -1;
		}

		__m256i thres_8[BATCH_SIZE];
		__m256i bcstd[BATCH_SIZE][MIXER_SIZE];
		for (c = 0; c < BATCH_SIZE; c++) {
			for (j = 0; j < MIXER_SIZE; j++) {
				bcstd[c][j] = _mm256_set1_epi16(
					(uint16_t)query[c][j]);
			}
			thres_8[c] = _mm256_set1_epi16(min_dist[c]);
		}

		int ii;
		for (ii = 0; ii < state->items; ii += LANE_SIZE) {
			__m256i score_16[BATCH_SIZE];
			for (c = 0; c < BATCH_SIZE; c++) {
				// Due to subs_epu16 arithmetic below and lack
				// of cmpgt_epu16 we must start with score ==
				// 1. The reason is simple, we do:
				//  saturated(score - threshold) == 0
				//
				// This works as less-or-equal. So if score ==
				// threshold, we will end up with "true". To
				// avoid that we better do
				// saturated(score+1-threshold) == 0, which will
				// give us less-than semantics.
				score_16[c] = _mm256_set1_epi16(1);
			}

			for (j = 0; j < MIXER_SIZE; j++) {
				uint8_t *b =
					&sv[ii * MIXER_SIZE + j * LANE_SIZE];

				__m128i hs = _mm_load_si128((__m128i *)b);
				__m256i h_16x16 = _mm256_cvtepu8_epi16(hs);
				// Very hot loop. Notice "adds" with saturation.
				for (c = 0; c < BATCH_SIZE; c++) {
					__m256i n_16x16 = bcstd[c][j];
					__m256i d_16x16 = _mm256_sub_epi16(
						h_16x16, n_16x16);
					__m256i sq = _mm256_mullo_epi16(
						d_16x16, d_16x16);
					score_16[c] = _mm256_adds_epu16(
						score_16[c], sq);
				}
			}

			uint64_t cc = 0;
			uint64_t mm[BATCH_SIZE] = {};
			for (c = 0; c < BATCH_SIZE; c++) {
				// To do comparison we need to hack it
				// together sadly. There is no
				// cmpgt_epu16. Instead let's
				// subtract-saturate one from another
				// and check for zero.
				__m256i d = _mm256_subs_epu16(score_16[c],
							      thres_8[c]);
				__m256i x = _mm256_cmpeq_epi16(
					_mm256_setzero_si256(), d);

				mm[c] = (uint32_t)_mm256_movemask_epi8(x);
				cc |= mm[c];
			}

			if (__builtin_expect(!cc, 1)) {
				continue;
			}

			for (c = 0; c < BATCH_SIZE; c++) {
				if (__builtin_expect(!mm[c], 1)) {
					continue;
				}
				// clear every second bit
				mm[c] &= 0x5555555555555555ULL;
				while (mm[c]) {
					int bitno = __builtin_ffsll(mm[c]) - 1;
					mm[c] &= ~(1ULL << bitno);

					// There are two bits per uint16. Look
					// only at even one.
					j = bitno / 2;

					uint32_t full_idx = ii + j;
					uint8_t *full_hash =
						state->hashes[full_idx];
					uint32_t full_dist = distance_avx2(
						query[c], full_hash, 144);
					if (full_dist < min_dist[c]) {
						min_dist[c] = full_dist;
						min_idx[c] = full_idx;
						thres_8[c] = _mm256_set1_epi16(
							full_dist);
					}
				}
			}
		}
		for (j = 0; j < batch_size; j++) {
			if (min_idx[j] != -1) {
				printf("sq_dist=%d idx=%d\n", min_dist[j],
				       min_idx[j]);
			} else {
				printf("sq_dist=%d idx=%d\n", -1, min_idx[j]);
			}
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
