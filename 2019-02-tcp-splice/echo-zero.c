#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <linux/tcp.h>

// importing netinet/tcp.h conflicts with linux/tcp.h
#ifndef SOL_TCP
#define SOL_TCP 6
#endif

#include "common.h"

int main(int argc, char **argv)
{
	struct ztable *ztable = ztable_new(128);

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

	/* SO_ZEROCOPY must be done before bind() syscall or on the
	 * accept() socket. Enable ZEROCOPY. */
	int sd = net_bind_tcp(&listen, 1);
	if (sd < 0) {
		PFATAL("connect()");
	}

again_accept:;
	struct sockaddr_storage client;
	int cd = net_accept(sd, &client);

	if (busy_poll) {
		int val = 10 * 1000; // 10 ms, in us. requires CAP_NET_ADMIN
		int r = setsockopt(cd, SOL_SOCKET, SO_BUSY_POLL, &val,
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

	uint64_t t0 = realtime_now();

	uint64_t sum = 0;
	int zc_send_number = 0;
	int done = 0;
	while (done == 0) {
		struct zbuf *z = zbuf_new(ztable);
		int n = read(cd, zbuf_buf(z), zbuf_cap(z));

		if (n == 0) {
			zbuf_free(ztable, z);
			fprintf(stderr, "[-] edge side EOF\n");
			/* Ensure write buffer is flushed. */
			done = 1;
		} else {
			if (n <= 0) {
				PFATAL("read()");
			}

			int m = send(cd, zbuf_buf(z), n,
				     MSG_ZEROCOPY | MSG_NOSIGNAL);
			if (m < 0) {
				PFATAL("send(MSG_ZEROCOPY)");
			}

			if (m != n) {
				PFATAL("%d != %d", m, n);
			}

			zbuf_schedule(ztable, z, zc_send_number);

			zc_send_number += 1;
			sum += n;
		}

		/* Too many outstanding buffers -> reap notifications */
		int run_number = 0;
		while (ztable_items(ztable) > 16) {
			ztable_reap_completions(ztable, cd, run_number > 0);
			run_number += 1;
		}
	}

	int run_number = 0;
	while (ztable_items(ztable) > 0) {
		ztable_reap_completions(ztable, cd, run_number > 0);
		run_number += 1;
	}

	close(cd);
	uint64_t t1 = realtime_now();
	ztable_empty(ztable);

	fprintf(stderr, "[+] Read %.1fMiB in %.1fms\n", sum / (1024 * 1024.),
		(t1 - t0) / 1000000.);
	goto again_accept;

	return 0;
}
