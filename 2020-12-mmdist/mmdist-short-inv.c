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

static uint32_t distance(uint8_t *a, uint8_t *b, int len)
{
	uint32_t d = 0;
	int i;
	for (i = 0; i < len; i++) {
		uint32_t t = (uint16_t)a[i] - (uint16_t)b[i];
		d += t * t;
	}
	return d;
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
		for (j = 0; j < BATCH_SIZE; j++) {
			min_dist[j] = 0xffffffff;
			min_idx[j] = -1;
		}

		int i, c, l;
		for (i = 0; i < state->items; i += LANE_SIZE) {
			uint32_t scores[BATCH_SIZE][LANE_SIZE];
			for (c = 0; c < BATCH_SIZE; c++) {
				for (l = 0; l < LANE_SIZE; l++) {
					scores[c][l] = 0;
				}
			}

			for (j = 0; j < MIXER_SIZE; j++) {
				uint8_t *b =
					&sv[i * MIXER_SIZE + j * LANE_SIZE];
				for (l = 0; l < LANE_SIZE; l++) {
					uint16_t t = b[l];
					for (c = 0; c < BATCH_SIZE; c++) {
						uint32_t delta =
							(uint16_t)query[c][j] -
							t;
						uint32_t sq = delta * delta;
						scores[c][l] += sq;
					}
				}
			}

			for (c = 0; c < BATCH_SIZE; c++) {
				for (l = 0; l < LANE_SIZE; l++) {
					if (scores[c][l] < min_dist[c] &&
					    scores[c][l] <= threshold) {
						uint8_t *full_hash =
							state->hashes[i + l];
						uint32_t full_dist =
							scores[c][l] +
							distance(
								&query[c]
								      [MIXER_SIZE],
								&full_hash
									[MIXER_SIZE],
								144 - MIXER_SIZE);
						if (full_dist < min_dist[c] &&
						    full_dist < threshold) {
							min_dist[c] = full_dist;
							min_idx[c] = i + l;
						}
					}
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
