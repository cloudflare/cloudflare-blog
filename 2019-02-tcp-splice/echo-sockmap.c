#define _GNU_SOURCE /* POLLRDHUP */

#include <arpa/inet.h>
#include <errno.h>
#include <linux/bpf.h>
#include <linux/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "tbpf.h"

extern size_t bpf_insn_prog_parser_cnt;
extern struct bpf_insn bpf_insn_prog_parser[];
extern struct tbpf_reloc bpf_reloc_prog_parser[];

extern size_t bpf_insn_prog_verdict_cnt;
extern struct bpf_insn bpf_insn_prog_verdict[];
extern struct tbpf_reloc bpf_reloc_prog_verdict[];

int main(int argc, char **argv)
{
	/* [*] SOCKMAP requires more than 16MiB of locked mem */
	struct rlimit rlim = {
		.rlim_cur = 128 * 1024 * 1024,
		.rlim_max = 128 * 1024 * 1024,
	};
	/* ignore error */
	setrlimit(RLIMIT_MEMLOCK, &rlim);

	/* [*] Prepare ebpf */
	int sock_map = tbpf_create_map(BPF_MAP_TYPE_SOCKMAP, sizeof(int),
				       sizeof(int), 2, 0);
	if (sock_map < 0) {
		PFATAL("bpf(BPF_MAP_CREATE, BPF_MAP_TYPE_SOCKMAP)");
	}

	/* sockmap is only used in prog_verdict */
	tbpf_fill_symbol(bpf_insn_prog_verdict, bpf_reloc_prog_verdict,
			 "sock_map", sock_map);

	/* Load prog_parser and prog_verdict */
	char log_buf[16 * 1024];
	int bpf_parser = tbpf_load_program(
		BPF_PROG_TYPE_SK_SKB, bpf_insn_prog_parser,
		bpf_insn_prog_parser_cnt, "Dual BSD/GPL",
		KERNEL_VERSION(4, 4, 0), log_buf, sizeof(log_buf));
	if (bpf_parser < 0) {
		PFATAL("Bpf Log:\n%s\n bpf(BPF_PROG_LOAD, prog_parser)",
		       log_buf);
	}

	int bpf_verdict = tbpf_load_program(
		BPF_PROG_TYPE_SK_SKB, bpf_insn_prog_verdict,
		bpf_insn_prog_verdict_cnt, "Dual BSD/GPL",
		KERNEL_VERSION(4, 4, 0), log_buf, sizeof(log_buf));
	if (bpf_verdict < 0) {
		PFATAL("Bpf Log:\n%s\n bpf(BPF_PROG_LOAD, prog_verdict)",
		       log_buf);
	}

	/* Attach maps to programs. It's important to attach SOCKMAP
	 * to both parser and verdict programs, even though in parser
	 * we don't use it. The whole point is to make prog_parser
	 * hooked to SOCKMAP.*/
	int r = tbpf_prog_attach(bpf_parser, sock_map, BPF_SK_SKB_STREAM_PARSER,
				 0);
	if (r < 0) {
		PFATAL("bpf(PROG_ATTACH)");
	}

	r = tbpf_prog_attach(bpf_verdict, sock_map, BPF_SK_SKB_STREAM_VERDICT,
			     0);
	if (r < 0) {
		PFATAL("bpf(PROG_ATTACH)");
	}

	if (argc < 2) {
		FATAL("Usage: %s <listen:port>", argv[0]);
	}

	struct sockaddr_storage listen;
	net_parse_sockaddr(&listen, argv[1]);

	int busy_poll = 0;
	if (argc > 2) {
		busy_poll = 1;
	}

	fprintf(stderr, "[+] Accepting on %s busy_poll=%d\n", net_ntop(&listen),
		busy_poll);

	int sd = net_bind_tcp(&listen);
	if (sd < 0) {
		PFATAL("connect()");
	}

again_accept:;
	struct sockaddr_storage client;
	int fd = net_accept(sd, &client);

	if (busy_poll) {
		int val = 10 * 1000; // 10 ms, in us. requires CAP_NET_ADMIN
		int r = setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, &val,
				   sizeof(val));
		if (r < 0) {
			if (errno == EPERM) {
				fprintf(stderr,
					"[ ] Failed to set SO_BUSY_POLL. Are "
					"you "
					"CAP_NET_ADMIN?\n");
			} else {
				PFATAL("setsockopt(SOL_SOCKET, SO_BUSY_POLL)");
			}
		}
	}

	{
		/* There is a bug in sockmap which prevents it from
		 * working right when snd buffer is full. Set it to
		 * gigantic value. */
		int val = 32 * 1024 * 1024;
		setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
	}

	/* [*] Perform ebpf socket magic */
	/* Add socket to SOCKMAP. Otherwise the ebpf won't work. */
	int idx = 0;
	int val = fd;
	r = tbpf_map_update_elem(sock_map, &idx, &val, BPF_ANY);
	if (r != 0) {
		if (errno == EOPNOTSUPP) {
			perror("pushing closed socket to sockmap?");
			close(fd);
			goto again_accept;
		}
		PFATAL("bpf(MAP_UPDATE_ELEM)");
	}

	/* [*] Wait for the socket to close. Let sockmap do the magic. */
	struct pollfd fds[1] = {
		{.fd = fd, .events = POLLRDHUP},
	};
	poll(fds, 1, -1);

	/* Was there a socket error? */
	{
		int err;
		socklen_t err_len = sizeof(err);
		int r = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &err_len);
		if (r < 0) {
			PFATAL("getsockopt()");
		}
		errno = err;
		if (errno) {
			perror("sockmap fd");
		}
	}

	/* Get byte count from TCP_INFO */
	struct xtcp_info ta, ti = {};
	socklen_t ti_len = sizeof(ti);
	r = getsockopt(fd, IPPROTO_TCP, TCP_INFO, &ta, &ti_len);
	if (r < 0) {
		PFATAL("getsockopt(TPC_INFO)");
	}

	/* Cleanup the entry from sockmap. */
	idx = 0;
	r = tbpf_map_delete_elem(sock_map, &idx);
	if (r != 0) {
		if (errno == EINVAL) {
			fprintf(stderr,
				"[-] Removing closed sock from sockmap\n");
		} else {
			PFATAL("bpf(MAP_DELETE_ELEM, sock_map)");
		}
	}
	close(fd);

	fprintf(stderr, "[+] rx=%lu tx=%lu\n", ta.tcpi_bytes_received,
		ti.tcpi_bytes_sent - ti.tcpi_bytes_retrans);
	goto again_accept;
}
