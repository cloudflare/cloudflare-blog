#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

#define __unused __attribute__((unused))

char __license[] SEC("license") = "GPL";

struct {
	__uint(type, BPF_MAP_TYPE_PROG_ARRAY);
	__uint(max_entries, 1);
	__uint(key_size, sizeof(__u32));
	__uint(value_size, sizeof(__u32));
} bar SEC(".maps");

SEC("tc")
int barista(struct __sk_buff *skb __unused)
{
	return 0xcafe;
}

static __noinline
int bring_order(struct __sk_buff *skb)
{
	bpf_tail_call(skb, &bar, 0);
	return 0xf00d;
}

SEC("tc")
int server1(struct __sk_buff *skb)
{
	return bring_order(skb);
}

SEC("tc")
int server2(struct __sk_buff *skb)
{
	__attribute__((musttail)) return bring_order(skb);
}
