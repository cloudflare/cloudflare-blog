#include <linux/bpf.h>
#include "common.h"

#define _inline_        __attribute__((always_inline))
#define _section_(NAME) __attribute__((section(NAME), used))
#define _unused_        __attribute__((unused))

/* This definition is specific to your BPF ELF loader */
struct bpf_map_def {
	unsigned int type;
	unsigned int key_size;
	unsigned int value_size;
	unsigned int max_entries;
	unsigned int map_flags;
};

static void *(*bpf_map_lookup_elem)(void *map, const void *key) =
	(void *) BPF_FUNC_map_lookup_elem;
static void *(*bpf_map_update_elem)(void *map, const void *key,
				    void *value, u64 flags) =
	(void *) BPF_FUNC_map_update_elem;


/* Trace function is available only for privileged users. */
#ifdef TRACE
static int (*bpf_trace_printk)(const char *fmt, int fmt_size, ...) =
	(void *) BPF_FUNC_trace_printk;

#define bpf_printk(fmt, ...) 					\
	({							\
		char ____fmt[] = fmt;				\
		bpf_trace_printk(____fmt, sizeof(____fmt),	\
				 ##__VA_ARGS__);		\
	})
#endif

enum args_keys {
	ARG_0,
	ARG_1,
	RES_0,
	MAX_KEY
};

_section_("maps")
struct bpf_map_def args = {
	.type		= BPF_MAP_TYPE_ARRAY,
	.key_size	= sizeof(u32),
	.value_size	= sizeof(u64),
	.max_entries	= MAX_KEY,
};

static _inline_ u64 args_get(u32 key)
{
	u64 *v = bpf_map_lookup_elem(&args, &key);
	return v ? *v : 0;
}

static _inline_ void args_put(u32 key, u64 v)
{
	bpf_map_update_elem(&args, &key, &v, BPF_ANY);
}

_section_("socket1")
int filter_alu64(struct __sk_buff *skb _unused_)
{
	u64 a, b, r;

	a = args_get(ARG_0);
	b = args_get(ARG_1);

	r = a - b;

	args_put(RES_0, r);

	return SK_PASS;
}

static _inline_ u64 sub64_alu32(u64 x, u64 y)
{
        u32 xh, xl, yh, yl;
        u32 hi, lo;

        xl = x;
        yl = y;
        lo = xl - yl;

        xh = x >> 32;
        yh = y >> 32;
        hi = xh - yh - (lo > xl); /* underflow? */

        return ((u64)hi << 32) | (u64)lo;
}

_section_("socket2")
int filter_alu32(struct __sk_buff *skb _unused_)
{
	u64 a, b, r;

	a = args_get(ARG_0);
	b = args_get(ARG_1);

	r = sub64_alu32(a, b);

	args_put(RES_0, r);

	return SK_PASS;
}

extern u64 sub64_ir(u64 x, u64 y);

_section_("socket3")
int filter_ir(struct __sk_buff *skb _unused_)
{
	u64 a, b, r;

	a = args_get(ARG_0);
	b = args_get(ARG_1);

	r = sub64_ir(a, b);

	args_put(RES_0, r);

	return SK_PASS;
}

static _inline_ u64 sub64_stv(u64 x, u64 y)
{
        u32 xh, xl, yh, yl;
        u32 hi, lo;

        xl = x;
        yl = y;
        ST_V(lo, xl - yl);

        xh = x >> 32;
        yh = y >> 32;
        ST_V(hi, xh - yh - (lo > xl) /* underflow? */);

        return ((u64)hi << 32) | (u64)lo;
}

_section_("socket4")
int filter_stv(struct __sk_buff *skb _unused_)
{
	u64 a, b, r;

	a = args_get(ARG_0);
	b = args_get(ARG_1);

	r = sub64_stv(a, b);

	args_put(RES_0, r);

	return SK_PASS;
}


char __license[] _section_("license") = "Dual BSD/GPL";
