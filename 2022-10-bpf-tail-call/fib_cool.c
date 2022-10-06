#include <stdint.h>

static uint64_t fib_tail(uint64_t n, uint64_t a, uint64_t b)
{
	if (n == 0)
		return a;
	if (n == 1)
		return b;

	return fib_tail(n - 1, b, a + b);
}

uint64_t fib(uint64_t n)
{
	return fib_tail(n, 1, 1);
}
