#include <linux/bpf.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>

#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>

int _version SEC("version") = 1;

SEC("connect4")
int connect4_prog(struct bpf_sock_addr *ctx)
{
	if (ctx->type != SOCK_DGRAM) {
		return 1;
	}

	struct bpf_sock *sk = ctx->sk;

	/* Ignore if bind wasn't called or was called with port 0 */
	if (sk->src_port == 0 || sk->src_ip4 == 0) {
		return 1;
	}
	if (ctx->user_port == 0 || ctx->user_ip4 == 0) {
		return 1;
	}

	/*
	  On struct bpf_sock
	__u32 src_port;		// host byte order
	__u32 dst_port;		// network byte order
	*/
	// (daddr, dport) on tuple are matching ss local address
	struct bpf_sock_tuple tuple = {};
	tuple.ipv4.saddr = ctx->user_ip4;
	tuple.ipv4.sport = ctx->user_port;
	tuple.ipv4.daddr = sk->src_ip4;
	tuple.ipv4.dport = bpf_htons(sk->src_port);

	struct bpf_sock *nsk = bpf_sk_lookup_udp(
		ctx, &tuple, sizeof(tuple.ipv4), BPF_F_CURRENT_NETNS, 0);

	if (!nsk) {
		// Not found. This is unexpected since we are supposed
		// to find our own socket.
		return 1;
	}

	int state = nsk->state;
	bpf_sk_release(nsk);

	if (state == 1) {
		// ESTABLISHED: Ok, some other estab socket fits the
		// 4-tuple. Our socket is not ESTAB yet. Fail.
		return 0; // fail with EPERM
	}

	// Most likey state==7 which is UNCONN. This most likely finds
	// our own socket. This means there isn't more specific ESTAB
	// around.
	return 1;
}

char _license[] SEC("license") = "GPL";
