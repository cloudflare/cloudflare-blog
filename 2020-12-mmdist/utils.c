#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

// Produces x if f is true, else y if f is false.
#define BRANCHLESS_IF_ELSE(f, x, y)                                            \
	(((x) & -((typeof(x)) !!(f))) | ((y) & -((typeof(y)) !(f))))

int parse_hex(uint8_t *restrict dst, uint8_t *restrict src, int max)
{
	int n = 0;
	while (n < max) {
		uint8_t a = ' ';
		while (a == ' ') {
			a = *src++;
		}
		if (a == 0) {
			// correct exit
			return n;
		}
		uint8_t b = ' ';
		while (b == ' ') {
			b = *src++;
		}
		if (b == 0) {
			// error, odd characters
			return -1;
		}
		a |= 0x20;
		b |= 0x20;

		uint32_t a1 = BRANCHLESS_IF_ELSE(
			(a >= '0' && a <= '9'), a - '0',
			BRANCHLESS_IF_ELSE((a >= 'a' && a <= 'f'), a - 'a' + 10,
					   0x100));
		uint32_t b1 = BRANCHLESS_IF_ELSE(
			(b >= '0' && b <= '9'), b - '0',
			BRANCHLESS_IF_ELSE((b >= 'a' && b <= 'f'), b - 'a' + 10,
					   0x100));
		if (a1 == 0x100 || b1 == 0x100) {
			// error, not a-f chars
			return -2;
		}
		*dst++ = (a1 << 4) | b1;
		n += 1;
	}
	return n;
}

int mmdist_load(const char *fname, struct mmdist **state_ptr)
{
	int fd = open(fname, O_RDONLY);
	if (fd < 0) {
		return errno ? errno : EINVAL;
	}

	struct stat statbuf;
	int r = fstat(fd, &statbuf);
	if (r < 0) {
		return errno ? errno : EINVAL;
	}
	uint64_t size = statbuf.st_size;

	if (size % (144 * 2 + 1) != 0) {
		close(fd);
		return EMSGSIZE;
	}

	uint8_t *file_mem =
		mmap(NULL, size, PROT_READ, MAP_SHARED | MAP_POPULATE, fd, 0);
	if (file_mem == MAP_FAILED) {
		return errno ? errno : EINVAL;
	}

	int items = size / (144 * 2 + 1);

	struct mmdist *state = calloc(1, sizeof(struct mmdist) + 144 * items);
	if (state == NULL) {
		munmap(file_mem, size);
		close(fd);
		return ENOMEM;
	}
	state->items = items;

	int item;
	for (item = 0; item < items; item++) {
		parse_hex(state->hashes[item], &file_mem[item * (144 * 2 + 1)],
			  144);
	}

	munmap(file_mem, size);
	close(fd);

	*state_ptr = state;
	return r;
}
