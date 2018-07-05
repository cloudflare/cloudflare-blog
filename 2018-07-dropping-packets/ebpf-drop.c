// ebpf-drop
//
// Copyright (c) 2018 CloudFlare, Inc.

#include <arpa/inet.h>
#include <errno.h>
#include <linux/bpf.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

#include "net.h"

static __u64 ptr_to_u64(const void *ptr) { return (__u64)(unsigned long)ptr; }

static int bpf(int cmd, union bpf_attr *attr, unsigned int size)
{
	return syscall(__NR_bpf, cmd, attr, size);
}

#define BPF_MOV64_IMM(DST, IMM)                                                \
	((struct bpf_insn){.code = BPF_ALU64 | BPF_MOV | BPF_K,                \
			   .dst_reg = DST,                                     \
			   .src_reg = 0,                                       \
			   .off = 0,                                           \
			   .imm = IMM})

#define BPF_EXIT_INSN()                                                        \
	((struct bpf_insn){.code = BPF_JMP | BPF_EXIT,                         \
			   .dst_reg = 0,                                       \
			   .src_reg = 0,                                       \
			   .off = 0,                                           \
			   .imm = 0})

static int net_setup_ebpf(int sd)
{
	struct bpf_insn prog[] = {
		BPF_MOV64_IMM(BPF_REG_0, 0), /* r0 = 0 */
		BPF_EXIT_INSN(),
	};

	union bpf_attr attr = {
		.prog_type = BPF_PROG_TYPE_SOCKET_FILTER,
		.insns = ptr_to_u64(prog),
		.insn_cnt = ARRAY_SIZE(prog),
		.license = ptr_to_u64("BSD"),
	};

	int fd = bpf(BPF_PROG_LOAD, &attr, sizeof(attr));
	if (fd < 0) {
		PFATAL("bpf_prog_load()");
	}

	int r = setsockopt(sd, SOL_SOCKET, SO_ATTACH_BPF, &fd, sizeof(fd));
	if (r < 0) {
		PFATAL("setsockopt(SO_ATTACH_BPF)");
	}

	return sd;
}

#define MTU_SIZE 2048

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

	net_setup_ebpf(fd);

	char buf[MTU_SIZE];

	while (1) {
		int r = read(fd, buf, MTU_SIZE);
		if (r == 0) {
			int err = 0;
			socklen_t err_len = sizeof(err);
			getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &err_len);
			if (err == 0) {
				packets += 1;
				continue;
			}
			return 0;
		}

		if (r < 0) {
			if (errno == EINTR) {
				continue;
			}
			PFATAL("recv()");
		} else {
			packets += 1;
			bytes += r;
		}
	}
	close(fd);

	return 0;
}
