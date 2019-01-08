#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

static int trunc_iov(struct iovec *iov, int iov_cnt, int b_sz, int desired)
{
	int i;
	for (i = 0; i < iov_cnt; i++) {
		int m = MIN(desired, b_sz);
		iov[i].iov_len = m;
		desired -= m;
		if (desired == 0) {
			return i + 1;
		}
	}
	return iov_cnt;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		FATAL("Usage: %s <target:port> <amount in KiB> <number of "
		      "bursts>",
		      argv[0]);
	}

	struct sockaddr_storage target;
	net_parse_sockaddr(&target, argv[1]);

	uint64_t burst_sz = 1 * 1024 * 1024; // 1MiB
	if (argc > 2) {
		char *endptr;
		double ts = strtod(argv[2], &endptr);
		if (ts < 0 || *endptr != '\0') {
			FATAL("Can't parse number %s", argv[2]);
		}
		if (ts < 1.0) {
			burst_sz = BUFFER_SIZE;
		} else {
			burst_sz = ts * 1024.0; // In KiB
		}
	}

	long burst_count = 1;
	if (argc > 3) {
		char *endptr;
		burst_count = strtol(argv[3], &endptr, 10);
		if (burst_count < 0 || *endptr != '\0') {
			FATAL("Can't parse number %s", argv[2]);
		}
	}

	fprintf(stderr, "[+] Sending %ld blocks of %.1fMiB to %s\n",
		burst_count, burst_sz / (1024 * 1024.), net_ntop(&target));

	int fd = net_connect_tcp_blocking(&target, 1);
	if (fd < 0) {
		PFATAL("connect()");
	}

	sleep(1);

	int val = 10 * 1000; // 10 ms, in us. requires CAP_NET_ADMIN
	int r = setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, &val, sizeof(val));
	if (r < 0) {
		if (errno == EPERM) {
			fprintf(stderr,
				"[ ] Failed to set SO_BUSY_POLL. Are you "
				"CAP_NET_ADMIN?\n");
		} else {
			PFATAL("setsockopt(SOL_SOCKET, SO_BUSY_POLL)");
		}
	}

	/* Attempt to set large TX and RX buffer. Why not. */
	val = burst_sz * 2;
	setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
	setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));

	char tx_buf[BUFFER_SIZE];
	char rx_buf[BUFFER_SIZE];
	memset(tx_buf, 'a', sizeof(tx_buf));

	int iov_cnt = (burst_sz / sizeof(tx_buf)) + 1;
	struct iovec tx_iov[iov_cnt];
	struct iovec rx_iov[iov_cnt];
	int i;
	for (i = 0; i < iov_cnt; i++) {
		tx_iov[i] = (struct iovec){.iov_base = tx_buf,
					   .iov_len = sizeof(tx_buf)};
		rx_iov[i] = (struct iovec){.iov_base = rx_buf,
					   .iov_len = sizeof(rx_buf)};
	}

	uint64_t total_t0 = realtime_now();

	int burst_i;
	for (burst_i = 0; burst_i < burst_count; burst_i += 1) {
		uint64_t t0 = realtime_now();

		uint64_t tx_bytes = burst_sz;
		uint64_t rx_bytes = burst_sz;

		while (tx_bytes || rx_bytes) {
			if (tx_bytes) {
				int d = trunc_iov(tx_iov, iov_cnt,
						  sizeof(tx_buf), MIN(128*1024*1024, tx_bytes));
				struct msghdr msg_hdr = {
					.msg_iov = tx_iov,
					.msg_iovlen = d,
				};
				int n = sendmsg(fd, &msg_hdr, MSG_DONTWAIT);
				if (n < 0) {
					if (errno == EINTR) {
						continue;
					}
					if (errno == ECONNRESET) {
						fprintf(stderr,
							"[!] ECONNRESET\n");
						break;
					}
					if (errno == EPIPE) {
						fprintf(stderr, "[!] EPIPE\n");
						break;
					}
					if (errno == EAGAIN) {
						// pass
					} else {
						PFATAL("send()");
					}
				}
				if (n == 0) {
					PFATAL("?");
				}
				if (n > 0) {
					tx_bytes -= n;
				}
			}

			if (rx_bytes) {
				int flags = MSG_DONTWAIT;
				if (tx_bytes == 0) {
					// block. allright. Let busy_poll do
					// work from here.
					flags = MSG_WAITALL;
				}

				int d = trunc_iov(rx_iov, iov_cnt,
						  sizeof(rx_buf), MIN(128*1024*1024,rx_bytes));
				struct msghdr msg_hdr = {
					.msg_iov = rx_iov,
					.msg_iovlen = d,
				};
				int n = recvmsg(fd, &msg_hdr, flags);
				if (n < 0) {
					if ((flags & MSG_DONTWAIT) != 0 &&
					    errno == EAGAIN) {
						continue;
					}
					PFATAL("recvmsg()");
				}
				if (n == 0) {
					PFATAL("?");
				}
				if (n > 0) {
					rx_bytes -= n;
				}
			}
		}

		uint64_t t1 = realtime_now();
		printf("%ld\n", (t1 - t0) / 1000);
	}
	close(fd);

	uint64_t total_t1 = realtime_now();

	fprintf(stderr, "[+] Wrote %ld bursts of %.1fMiB in %.1fms\n",
		burst_count, burst_sz / (1024 * 1024.),
		(total_t1 - total_t0) / 1000000.);
	return 0;
}
