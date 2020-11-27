#include <errno.h>
#include <error.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/random.h>

uint8_t *to_hex(uint8_t *dst, uint8_t *src, int sz)
{
	static const uint8_t HEX[] = "0123456789abcdef";
	int i;
	for (i = 0; i < sz; i++) {
		dst[i * 2] = HEX[(src[i] >> 4) & 0xf];
		dst[i * 2 + 1] = HEX[src[i] & 0xf];
	}
	dst[i * 2] = '\x00';
	return dst;
}

int main()
{
	while (1) {
		uint8_t buf[144];
		int r = getrandom(buf, sizeof(buf), 0);
		if (r != sizeof(buf)) {
			error(-1, errno, "getrandom()");
		}

		uint8_t tmp[1024];
		printf("%s\n", to_hex(tmp, buf, sizeof(buf)));
	}
}
