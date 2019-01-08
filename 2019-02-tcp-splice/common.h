/*  */
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

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define BUFFER_SIZE (128 * 1024)

/* net.c */
int net_parse_sockaddr(struct sockaddr_storage *ss, const char *addr);
int net_connect_tcp_blocking(struct sockaddr_storage *sas, int do_zerocopy);
int net_getpeername(int sd, struct sockaddr_storage *ss);
int net_getsockname(int sd, struct sockaddr_storage *ss);
const char *net_ntop(struct sockaddr_storage *ss);
int net_bind_tcp(struct sockaddr_storage *ss);
int net_accept(int sd, struct sockaddr_storage *ss);
void set_nonblocking(int fd);

/* inlines */
#define TIMESPEC_NSEC(ts) ((ts)->tv_sec * 1000000000ULL + (ts)->tv_nsec)
inline static uint64_t realtime_now()
{
	struct timespec now_ts;
	clock_gettime(CLOCK_MONOTONIC, &now_ts);
	return TIMESPEC_NSEC(&now_ts);
}

/* zerocopy.c */
struct ztable;
struct zbuf;

struct ztable *ztable_new();
struct zbuf *zbuf_new(struct ztable *t);
void zbuf_free(struct ztable *t, struct zbuf *z);
void zbuf_schedule(struct ztable *t, struct zbuf *z, int n);
void zbuf_deschedule_and_free(struct ztable *t, int n);
void ztable_empty(struct ztable *t);
int ztable_items(struct ztable *t);

int zbuf_cap(struct zbuf *z);
char *zbuf_buf(struct zbuf *z);

int ztable_reap_completions(struct ztable *t, int fd, int block);

/* tcp_info */
struct xtcp_info {
	uint8_t tcpi_state;
	uint8_t tcpi_ca_state;
	uint8_t tcpi_retransmits;
	uint8_t tcpi_probes;
	uint8_t tcpi_backoff;
	uint8_t tcpi_options;
	uint8_t tcpi_snd_wscale : 4, tcpi_rcv_wscale : 4;
	uint8_t tcpi_delivery_rate_app_limited : 1;

	uint32_t tcpi_rto;
	uint32_t tcpi_ato;
	uint32_t tcpi_snd_mss;
	uint32_t tcpi_rcv_mss;

	uint32_t tcpi_unacked;
	uint32_t tcpi_sacked;
	uint32_t tcpi_lost;
	uint32_t tcpi_retrans;
	uint32_t tcpi_fackets;

	/* Times. */
	uint32_t tcpi_last_data_sent;
	uint32_t tcpi_last_ack_sent; /* Not remembered, sorry. */
	uint32_t tcpi_last_data_recv;
	uint32_t tcpi_last_ack_recv;

	/* Metrics. */
	uint32_t tcpi_pmtu;
	uint32_t tcpi_rcv_ssthresh;
	uint32_t tcpi_rtt;
	uint32_t tcpi_rttvar;
	uint32_t tcpi_snd_ssthresh;
	uint32_t tcpi_snd_cwnd;
	uint32_t tcpi_advmss;
	uint32_t tcpi_reordering;

	uint32_t tcpi_rcv_rtt;
	uint32_t tcpi_rcv_space;

	uint32_t tcpi_total_retrans;

	uint64_t tcpi_pacing_rate;
	uint64_t tcpi_max_pacing_rate;
	uint64_t tcpi_bytes_acked; /* RFC4898 tcpEStatsAppHCThruOctetsAcked */
	uint64_t tcpi_bytes_received; /* RFC4898
					 tcpEStatsAppHCThruOctetsReceived */
	uint32_t tcpi_segs_out;       /* RFC4898 tcpEStatsPerfSegsOut */
	uint32_t tcpi_segs_in;	/* RFC4898 tcpEStatsPerfSegsIn */

	uint32_t tcpi_notsent_bytes;
	uint32_t tcpi_min_rtt;
	uint32_t tcpi_data_segs_in;  /* RFC4898 tcpEStatsDataSegsIn */
	uint32_t tcpi_data_segs_out; /* RFC4898 tcpEStatsDataSegsOut */

	uint64_t tcpi_delivery_rate;

	uint64_t tcpi_busy_time;    /* Time (usec) busy sending data */
	uint64_t tcpi_rwnd_limited; /* Time (usec) limited by receive window */
	uint64_t tcpi_sndbuf_limited; /* Time (usec) limited by send buffer */

	uint32_t tcpi_delivered;
	uint32_t tcpi_delivered_ce;

	uint64_t tcpi_bytes_sent;    /* RFC4898 tcpEStatsPerfHCDataOctetsOut */
	uint64_t tcpi_bytes_retrans; /* RFC4898 tcpEStatsPerfOctetsRetrans */
	uint32_t tcpi_dsack_dups;    /* RFC4898 tcpEStatsStackDSACKDups */
	uint32_t tcpi_reord_seen;    /* reordering events seen */
};
