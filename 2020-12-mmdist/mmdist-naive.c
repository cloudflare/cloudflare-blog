#include <errno.h>
#include <error.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "common.h"

static uint32_t full_distance(uint8_t *a, uint8_t *b)
{
	uint32_t d = 0;
	int i;
	for (i = 0; i < 144; i++) {
		uint32_t t = (uint16_t)a[i] - (uint16_t)b[i];
		d += t * t;
	}
	return d;
}

int main()
{
	struct mmdist *state;
	int r = mmdist_load("database.txt", &state);
	if (r != 0) {
		error(-1, errno, "database.txt load");
	}

	uint64_t ts0_ns = realtime_now();
	int count = 0;
	while (1) {
		uint8_t line[1024];
		char *fr = fgets((char *)line, sizeof(line), stdin);
		if (fr == NULL) {
			break;
		}
		line[1023] = 0;

		uint8_t query[144];
		r = parse_hex(query, line, 144);
		if (r != 144) {
			error(-1, errno, "bad line, hex length");
		}

		uint32_t min_dist = 0xffffffff;
		int min_idx = -1;
		int i;
		for (i = 0; i < state->items; i++) {
			uint8_t *hash = state->hashes[i];
			uint32_t dist = full_distance(query, hash);
			if (dist < min_dist) {
				min_dist = dist;
				min_idx = i;
			}
		}
		printf("sq_dist=%d idx=%d\n", min_dist, min_idx);
		count += 1;
	}
	uint64_t td = realtime_now() - ts0_ns;
	uint64_t nsperquery = td / count;
	fprintf(stderr,
		"Total: %.3fms, %d items, avg %.3fms per query, %.3f qps\n",
		td / 1000000., count, nsperquery / 1000000.,
		1000000000. / nsperquery);
}
