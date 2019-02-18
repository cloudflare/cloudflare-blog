#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"

struct net_addr {
	sa_family_t family;
	union {
		struct in_addr inet;
		struct in6_addr inet6;
	} u;
};

static void net_addr_from_name(struct sockaddr_storage *ss, const char *host)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)ss;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ss;

	if (inet_pton(AF_INET, host, &sin->sin_addr) == 1) {
		sin->sin_family = AF_INET;
		return;
	}

	if (inet_pton(AF_INET6, host, &sin6->sin6_addr) == 1) {
		sin6->sin6_family = AF_INET6;
		return;
	}

	PFATAL("inet_pton(%s)", host);
}

int net_parse_sockaddr(struct sockaddr_storage *ss, const char *addr)
{
	memset(ss, 0, sizeof(struct sockaddr_storage));

	char *colon = strrchr(addr, ':');
	if (colon == NULL || colon[1] == '\0') {
		FATAL("%s doesn't contain a port number.", addr);
	}

	char *endptr;
	long port = strtol(&colon[1], &endptr, 10);
	if (port < 0 || port > 65535 || *endptr != '\0') {
		FATAL("Invalid port number %s", &colon[1]);
	}

	char host[255];
	int addr_len = colon - addr > 254 ? 254 : colon - addr;
	strncpy(host, addr, addr_len);
	host[addr_len] = '\0';
	net_addr_from_name(ss, host);

	struct sockaddr_in *sin = (struct sockaddr_in *)ss;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ss;

	switch (ss->ss_family) {
	case AF_INET:
		sin->sin_port = htons(port);
		break;
	case AF_INET6:
		sin6->sin6_port = htons(port);
		break;
	default:
		FATAL("");
	}
	return -1;
}

static size_t sizeof_ss(struct sockaddr_storage *ss)
{
	switch (ss->ss_family) {
	case AF_INET:
	case AF_INET6:
		return sizeof(struct sockaddr_storage);
	default:
		// AF_UNIX has sizeof defined in differnet way
		FATAL("");
	}
}

int net_connect_tcp_blocking(struct sockaddr_storage *ss, int do_zerocopy)
{
	int sd = socket(ss->ss_family, SOCK_STREAM, IPPROTO_TCP);
	if (sd < 0) {
		PFATAL("socket()");
	}

	/* Don't buffer partial packets */
	int one = 1;
	int r = setsockopt(sd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));
	if (r < 0) {
		PFATAL("setsockopt()");
	}

	/* Cubic is a bit more stable in tests than bbr */
	char *cong = "cubic";
	r = setsockopt(sd, SOL_TCP, TCP_CONGESTION, cong, strlen(cong));
	if (r < 0) {
		PFATAL("setsockopt(TCP_CONGESTION)");
	}

	if (do_zerocopy) {
		/* Zerocopy shall be set on the parent accept socket. */
		one = 1;
		r = setsockopt(sd, SOL_SOCKET, SO_ZEROCOPY, &one, sizeof(one));
		if (r < 0) {
			PFATAL("getsockopt()");
		}
	}

again:;
	r = connect(sd, (struct sockaddr *)ss, sizeof_ss(ss));
	if (r < 0) {
		if (errno == EINTR) {
			goto again;
		}
		PFATAL("connect()");
		return -1;
	}

	return sd;
}

int net_getpeername(int sd, struct sockaddr_storage *ss)
{
	memset(ss, 0, sizeof(struct sockaddr_storage));

	socklen_t ss_len = sizeof(struct sockaddr_storage);
	int r = getpeername(sd, (struct sockaddr *)ss, &ss_len);
	if (r < 0) {
		PFATAL("getpeername()");
	}

	/* stick trailing zero to AF_UNIX name */
	if (ss_len < sizeof(struct sockaddr_storage)) {
		((char *)ss)[ss_len] = '\0';
	}

	return 0;
}

int net_getsockname(int sd, struct sockaddr_storage *ss)
{
	memset(ss, 0, sizeof(struct sockaddr_storage));

	socklen_t ss_len = sizeof(struct sockaddr_storage);
	int r = getsockname(sd, (struct sockaddr *)ss, &ss_len);
	if (r < 0) {
		PFATAL("getsockname()");
	}

	/* stick trailing zero to AF_UNIX name */
	if (ss_len < sizeof(struct sockaddr_storage)) {
		((char *)ss)[ss_len] = '\0';
	}

	return 0;
}

const char *net_ntop(struct sockaddr_storage *ss)
{
	char s[INET6_ADDRSTRLEN + 1];
	static char a[INET6_ADDRSTRLEN + 32];
	struct sockaddr_in *sin = (struct sockaddr_in *)ss;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ss;
	int port;
	const char *r;
	switch (ss->ss_family) {
	case AF_INET:
		port = htons(sin->sin_port);
		r = inet_ntop(sin->sin_family, &sin->sin_addr, s, sizeof(s));
		if (r == NULL) {
			PFATAL("inet_ntop()");
		}
		snprintf(a, sizeof(a), "%s:%i", s, port);
		break;
	case AF_INET6:
		r = inet_ntop(sin6->sin6_family, &sin6->sin6_addr, s,
			      sizeof(s));
		if (r == NULL) {
			PFATAL("inet_ntop()");
		}
		port = htons(sin6->sin6_port);
		snprintf(a, sizeof(a), "[%s]:%i", s, port);
		break;
	default:
		FATAL("");
	}
	return a;
}

int net_bind_tcp(struct sockaddr_storage *ss)
{
	int sd = socket(ss->ss_family, SOCK_STREAM, IPPROTO_TCP);
	if (sd < 0) {
		PFATAL("socket()");
	}

	int one = 1;
	int r = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
			   sizeof(one));
	if (r < 0) {
		PFATAL("setsockopt(SO_REUSEADDR)");
	}

	one = 1;
	r = setsockopt(sd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));
	if (r < 0) {
		PFATAL("setsockopt()");
	}

	/* Cubic is a bit more stable in tests than bbr */
	char *cong = "cubic";
	r = setsockopt(sd, SOL_TCP, TCP_CONGESTION, cong, strlen(cong));
	if (r < 0) {
		PFATAL("setsockopt(TCP_CONGESTION)");
	}

	r = bind(sd, (struct sockaddr *)ss, sizeof_ss(ss));
	if (r < 0) {
		PFATAL("bind()");
	}

	listen(sd, 1024);
	return sd;
}

int net_accept(int sd, struct sockaddr_storage *ss)
{
again_accept:;
	socklen_t ss_len = sizeof(struct sockaddr_storage);
	int cd = accept(sd, (struct sockaddr *)ss, &ss_len);
	if (cd < 0) {
		if (errno == EINTR) {
			goto again_accept;
		}
		PFATAL("accept()");
	}

	/* stick trailing zero to AF_UNIX name */
	if (ss_len < sizeof(struct sockaddr_storage)) {
		((char *)ss)[ss_len] = '\0';
	}
	return cd;
}

void set_nonblocking(int fd)
{
	int flags, ret;
	flags = fcntl(fd, F_GETFL, 0);
	if (-1 == flags) {
		flags = 0;
	}
	ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (-1 == ret) {
		PFATAL("fcntl(O_NONBLOCK)");
	}
}
