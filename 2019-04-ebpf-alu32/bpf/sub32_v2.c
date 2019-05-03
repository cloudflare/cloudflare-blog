#include "common.h"

u64 sub32_v2(u64 x, u64 y)
{
	volatile u32 r;
	r = (u32)x - (u32)y;
	return r;
}
