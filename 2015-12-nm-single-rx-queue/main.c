#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <arpa/inet.h>

#define NETMAP_WITH_LIBS
#include <net/netmap_user.h>

static int
filter_packet(char *buf, int len)
{
	// Allow ARP
	if (len >= 14 &&
	    *((uint16_t *) (buf + 12)) == 0x0608) {
		return 1;
	}

	// Allow ICMP
	if (len >= 34 &&
	    *((uint16_t *) (buf + 12)) == 0x0008 &&
	    *((uint8_t *)  (buf + 23)) == 0x1) {
		return 1;
	}

	// Drop anything else
	return 0;
}

static void
receiver(struct nm_desc *d, unsigned int ring_id)
{
	struct pollfd fds;
	struct netmap_ring *ring;
	unsigned int i, len;
	char *buf;
	time_t now;
	int pps;

	now = time(NULL);
	pps = 0;

	while (1) {
		fds.fd     = d->fd;
		fds.events = POLLIN;

		int r = poll(&fds, 1, 1000);
		if (r < 0) {
			perror("poll()");
			exit(3);
		}

		if (time(NULL) > now) {
			printf("[+] receiving %d pps\n", pps);
			pps = 0;
			now = time(NULL);
		}

		ring = NETMAP_RXRING(d->nifp, ring_id);

		while (!nm_ring_empty(ring)) {
			i   = ring->cur;
			buf = NETMAP_BUF(ring, ring->slot[i].buf_idx);
			len = ring->slot[i].len;

			pps++;

			if (filter_packet(buf, len)) {
				// forward
				ring->flags         |= NR_FORWARD;
				ring->slot[i].flags |= NS_FORWARD;
			} else {
				// drop
			}

			ring->head = ring->cur = nm_ring_next(ring, i);
		}
	}
}

int
main(int argc, char *argv[])
{
	char netmap_ifname[IFNAMSIZ + 21];
	const char *interface;
	unsigned int ring_id;
	struct nm_desc *d;

	if (argc < 3) {
		fprintf(stderr, "Usage: %s [interface] [RX ring number]\n", argv[0]);
		exit(1);
	}

	interface = argv[1];
	ring_id   = atoi(argv[2]);

	snprintf(netmap_ifname, sizeof(netmap_ifname), "netmap:%s-%d/R", interface, ring_id);
	d = nm_open(netmap_ifname, NULL, 0, 0);

	if (!d) {
		perror("nm_open()");
		exit(2);
	}

	printf("[+] Receiving packets on interface %s, RX ring %d\n", interface, ring_id);
	receiver(d, ring_id);

	return 0;
}

