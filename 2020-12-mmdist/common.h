
struct mmdist {
	int items;
	uint8_t hashes[0][144];
};

int mmdist_load(const char *fname, struct mmdist **state_ptr);

int parse_hex(uint8_t *restrict dst, uint8_t *restrict src, int max);

#define THRESHOLD 49000

#ifndef TIMESPEC_NSEC
#define TIMESPEC_NSEC(ts) ((ts)->tv_sec * 1000000000ULL + (ts)->tv_nsec)
inline static uint64_t realtime_now()
{
	struct timespec now_ts;
	clock_gettime(CLOCK_MONOTONIC, &now_ts);
	return TIMESPEC_NSEC(&now_ts);
}
#endif
