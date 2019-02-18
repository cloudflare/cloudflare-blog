#include <stdint.h>

#include "bpf.h"
#include "bpf_helpers.h"

struct bpf_map_def SEC("maps") sock_map = {
	.type = BPF_MAP_TYPE_SOCKMAP,
	.key_size = sizeof(int),
	.value_size = sizeof(int),
	.max_entries = 2,
};

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#ifndef NDEBUG
/* Only use this for debug output. Notice output from bpf_trace_printk()
 * end-up in /sys/kernel/debug/tracing/trace_pipe
 */
#define bpf_debug(fmt, ...)                                                    \
	({                                                                     \
		char ____fmt[] = fmt;                                          \
		bpf_trace_printk(____fmt, sizeof(____fmt), ##__VA_ARGS__);     \
	})
#else
#define bpf_debug(fmt, ...)                                                    \
	{                                                                      \
	}                                                                      \
	while (0)
#endif

SEC("prog_parser")
int _prog_parser(struct __sk_buff *skb)
{
	return skb->len;
}

SEC("prog_verdict")
int _prog_verdict(struct __sk_buff *skb)
{
	uint32_t idx = 0;
	return bpf_sk_redirect_map(skb, &sock_map, idx, 0);
}
