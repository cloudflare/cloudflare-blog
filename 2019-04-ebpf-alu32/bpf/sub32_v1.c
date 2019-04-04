#include "common.h"

u64 sub32_v1(u64 x, u64 y)
{
	u32 r;
	r = (u32)x - (u32)y;
	return r;
}
