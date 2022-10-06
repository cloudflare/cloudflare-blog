#include <stdint.h>

uint64_t fib(uint64_t n)
{
	if (n == 0 || n == 1)
		return 1;

	return fib(n - 1) + fib(n - 2);
}
