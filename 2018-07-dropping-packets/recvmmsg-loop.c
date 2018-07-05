// recvmmsg-loop
//
// Copyright (c) 2018 CloudFlare, Inc.

#define _GNU_SOURCE // recvmmsg

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "net.h"

#define MTU_SIZE 2048
#define MAX_MSG 32

static uint64_t packets = 0;
static uint64_t bytes = 0;

static void timer_handler()
{
	printf("packets=%lu bytes=%lu\n", packets, bytes);
	packets = 0;
	bytes = 0;
}

int main()
{
	struct sigaction sa = {0};
	sa.sa_handler = &timer_handler;
	sigaction(SIGALRM, &sa, NULL);

	struct itimerval timer = {0};
	timer.it_value.tv_sec = 1;
	timer.it_interval.tv_sec = 1;
	setitimer(ITIMER_REAL, &timer, NULL);

	struct sockaddr_storage listen_addr;
	net_gethostbyname(&listen_addr, "::", 1234);
	int fd = net_bind_udp(&listen_addr);

	struct mmsghdr messages[MAX_MSG] = {0};
	char buffers[MAX_MSG][MTU_SIZE];
	struct iovec iovecs[MAX_MSG] = {0};

	/* Setup recvmmsg data structures. */
	int i;
	for (i = 0; i < MAX_MSG; i++) {
		char *buf = &buffers[i][0];
		struct iovec *iovec = &iovecs[i];
		struct mmsghdr *msg = &messages[i];

		msg->msg_hdr.msg_iov = iovec;
		msg->msg_hdr.msg_iovlen = 1;

		iovec->iov_base = &buf[0];
		iovec->iov_len = MTU_SIZE;
	}

	while (1) {
		int r = recvmmsg(fd, messages, MAX_MSG, MSG_WAITFORONE, NULL);
		if (r == 0) {
			return 0;
		}

		if (r < 0) {
			if (errno == EINTR) {
				continue;
			}
			PFATAL("recv()");
		} else {
			for (i = 0; i < MAX_MSG; i++) {
				struct mmsghdr *msg = &messages[i];
				bytes += msg->msg_len;
				msg->msg_len = 0;
			}
			packets += r;
		}
	}
	close(fd);

	return 0;
}
