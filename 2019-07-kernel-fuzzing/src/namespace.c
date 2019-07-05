#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"

/* Preserve the handle to global network namespace */
int netns_save()
{
	int net_ns = open("/proc/self/ns/net", O_RDONLY);
	if (net_ns < 0) {
		PFATAL("open(/proc/self/ns/net)");
	}
	return net_ns;
}

/* Create new namespace. Set it up with dummy0 and lo interfaces. */
void netns_new()
{
	int r = unshare(CLONE_NEWNET);
	if (r != 0) {
		if (errno != ENOSPC) {
			PFATAL("unshare(CLONE_NEWNET)");
		}
		return;
	}

	int nl_fd = socket(AF_NETLINK, SOCK_RAW | SOCK_NONBLOCK, NETLINK_ROUTE);
	struct sockaddr_nl sa = {
		.nl_family = AF_NETLINK,
	};

	bind(nl_fd, (struct sockaddr *)&sa, sizeof(sa));

	struct iovec iov;
	struct sockaddr_nl sax = {
		.nl_family = AF_NETLINK,
	};
	struct msghdr msg = {
		.msg_name = &sax,
		.msg_namelen = sizeof(sax),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};

	{
		// ip link set lo up
		char data1[] =
			"\x00\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x01"
			"\x00\x00\x00";

		char buf[128];
		struct nlmsghdr *nlmsg = (void *)buf;
		*nlmsg = (struct nlmsghdr){
			.nlmsg_len =
				sizeof(data1) - 1 + sizeof(struct nlmsghdr),
			.nlmsg_type = 0x10,
			.nlmsg_flags = NLM_F_REQUEST,
			.nlmsg_seq = 1,
		};
		memcpy(&nlmsg[1], data1, sizeof(data1));
		iov = (struct iovec){
			.iov_base = buf,
			.iov_len = nlmsg->nlmsg_len,
		};
		sendmsg(nl_fd, &msg, 0);
	}
	{
		// ip link add dummy1 type dummy
		char data2[] =
			"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x0b\x00\x03\x00\x64\x75\x6d\x6d\x79\x31"
			"\x00\x00\x10\x00\x12\x00\x09\x00\x01\x00\x64\x75\x6d"
			"\x6d\x79\x00\x00\x00";

		char buf[128];
		struct nlmsghdr *nlmsg = (void *)buf;
		*nlmsg = (struct nlmsghdr){
			.nlmsg_len =
				sizeof(data2) - 1 + sizeof(struct nlmsghdr),
			.nlmsg_type = 0x10,
			.nlmsg_flags = NLM_F_REQUEST | 0x600,
			.nlmsg_seq = 1,
		};
		memcpy(&nlmsg[1], data2, sizeof(data2));
		iov = (struct iovec){
			.iov_base = buf,
			.iov_len = nlmsg->nlmsg_len,
		};
		sendmsg(nl_fd, &msg, 0);
	}

	{
		// ip link set dummy1 up
		char data3[] =
			"\x00\x00\x00\x00\x02\x00\x00\x00\x01\x00\x00\x00\x01"
			"\x00\x00\x00";

		char buf[128];
		struct nlmsghdr *nlmsg = (void *)buf;
		*nlmsg = (struct nlmsghdr){
			.nlmsg_len =
				sizeof(data3) - 1 + sizeof(struct nlmsghdr),
			.nlmsg_type = 0x10,
			.nlmsg_flags = NLM_F_REQUEST,
			.nlmsg_seq = 1,
		};
		memcpy(&nlmsg[1], data3, sizeof(data3));
		iov = (struct iovec){
			.iov_base = buf,
			.iov_len = nlmsg->nlmsg_len,
		};
		sendmsg(nl_fd, &msg, 0);
	}

	close(nl_fd);
}

/* Restore global network namespace. */
void netns_restore(int net_ns)
{
	int r = setns(net_ns, CLONE_NEWNET);
	if (r != 0) {
		PFATAL("setns(net_ns, CLONE_NEWNET)");
	}
}
