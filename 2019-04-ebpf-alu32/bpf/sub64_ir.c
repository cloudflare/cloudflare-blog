#include "common.h"

u64 sub64_ir(u64 x, u64 y)
{
        u32 xh, xl, yh, yl;
        u32 hi, lo;

        xl = (u32) x;
        yl = (u32) y;
        lo = xl - yl;

        xh = (u32) (x >> 32);
        yh = (u32) (y >> 32);
        hi = xh - yh - (lo > xl); /* underflow? */

        return ((u64)hi << 32) | (u64)lo;
}
