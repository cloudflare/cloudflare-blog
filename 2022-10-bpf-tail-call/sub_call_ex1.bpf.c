#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

#define __unused __attribute__((unused))
#define __musttail __attribute__((musttail))


char __license[] SEC("license") = "GPL";

static __noinline
int sub_func(struct __sk_buff *skb __unused)
{
	return 0xf00d;
}

SEC("tc")
int entry_prog(struct __sk_buff *skb)
{
	__musttail return sub_func(skb);
}
