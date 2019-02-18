#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

int main(int argc, char **argv)
{
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

	char buf[BUFFER_SIZE];

	uint64_t sum = 0;
	while (1) {
		int n = recv(cd, buf, sizeof(buf), 0);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			if (errno == ECONNRESET) {
				fprintf(stderr, "[!] ECONNRESET\n");
				break;
			}
			PFATAL("read()");
		}

		if (n == 0) {
			/* On TCP socket zero means EOF */
			fprintf(stderr, "[-] edge side EOF\n");
			break;
		}

		sum += n;

		int m = send(cd, buf, n, MSG_NOSIGNAL);
		if (m < 0) {
			if (errno == EINTR) {
				continue;
			}
			if (errno == ECONNRESET) {
				fprintf(stderr, "[!] ECONNRESET on origin\n");
				break;
			}
			if (errno == EPIPE) {
				fprintf(stderr, "[!] EPIPE on origin\n");
				break;
			}
			PFATAL("send()");
		}
		if (m == 0) {
			break;
		}
		if (m != n) {
			int err;
			socklen_t err_len = sizeof(err);
			int r = getsockopt(cd, SOL_SOCKET, SO_ERROR, &err,
					   &err_len);
			if (r < 0) {
				PFATAL("getsockopt()");
			}
			errno = err;
			if (errno == EPIPE || errno == ECONNRESET) {
				break;
			}
			PFATAL("send()");
		}
	}

	close(cd);
	uint64_t t1 = realtime_now();

	fprintf(stderr, "[+] Read %.1fMiB in %.1fms\n", sum / (1024 * 1024.),
		(t1 - t0) / 1000000.);
	goto again_accept;

	return 0;
}
