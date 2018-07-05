#define ERRORF(x...) fprintf(stderr, x)

#define FATAL(x...)                                                            \
	do {                                                                   \
		ERRORF("[-] PROGRAM ABORT : " x);                              \
		ERRORF("\n\tLocation : %s(), %s:%u\n\n", __FUNCTION__,         \
		       __FILE__, __LINE__);                                    \
		exit(EXIT_FAILURE);                                            \
	} while (0)

#define PFATAL(x...)                                                           \
	do {                                                                   \
		ERRORF("[-] SYSTEM ERROR : " x);                               \
		ERRORF("\n\tLocation : %s(), %s:%u\n", __FUNCTION__, __FILE__, \
		       __LINE__);                                              \
		perror("      OS message ");                                   \
		ERRORF("\n");                                                  \
		exit(EXIT_FAILURE);                                            \
	} while (0)

#define TIMESPEC_NSEC(ts) ((ts)->tv_sec * 1000000000ULL + (ts)->tv_nsec)
#define TIMEVAL_NSEC(ts)                                                       \
	((ts)->tv_sec * 1000000000ULL + (ts)->tv_usec * 1000ULL)
#define NSEC_TIMESPEC(ns)                                                      \
	(struct timespec) { (ns) / 1000000000ULL, (ns) % 1000000000ULL }
#define NSEC_TIMEVAL(ns)                                                       \
	(struct timeval)                                                       \
	{                                                                      \
		(ns) / 1000000000ULL, ((ns) % 1000000000ULL) / 1000ULL         \
	}
#define MSEC_NSEC(ms) ((ms)*1000000ULL)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static void net_gethostbyname(struct sockaddr_storage *ss, const char *host,
			      int port)
{
	memset(ss, 0, sizeof(struct sockaddr_storage));

	struct in_addr in_addr;
	struct in6_addr in6_addr;

	/* Try ipv4 address first */
	if (inet_pton(AF_INET, host, &in_addr) == 1) {
		goto got_ipv4;
	}

	/* Then ipv6 */
	if (inet_pton(AF_INET6, host, &in6_addr) == 1) {
		goto got_ipv6;
	}

	FATAL("inet_pton(%s)", host);
	return;

got_ipv4:;
	struct sockaddr_in *sin4 = (struct sockaddr_in *)ss;
	sin4->sin_family = AF_INET;
	sin4->sin_port = htons(port);
	sin4->sin_addr = in_addr;
	return;

got_ipv6:;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ss;
	sin6->sin6_family = AF_INET6;
	sin6->sin6_port = htons(port);
	sin6->sin6_addr = in6_addr;
	return;
}

static int net_bind_udp(struct sockaddr_storage *ss)
{
	int sd = socket(ss->ss_family, SOCK_DGRAM, IPPROTO_UDP);
	if (sd < 0) {
		PFATAL("socket()");
	}

	int one = 1;
	int r = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
			   sizeof(one));
	if (r < 0) {
		PFATAL("setsockopt(SO_REUSEADDR)");
	}

	int zero = 0;
	r = setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&zero,
		       sizeof(zero));
	if (r < 0) {
		PFATAL("setsockopt()");
	}

	r = bind(sd, (struct sockaddr *)ss, sizeof(struct sockaddr_storage));
	if (r < 0) {
		PFATAL("bind()");
	}

	return sd;
}
