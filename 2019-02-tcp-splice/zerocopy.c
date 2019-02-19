#define _GNU_SOURCE /* splice */

#include <arpa/inet.h>
#include <linux/errqueue.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>

#include "common.h"

#include "list.h"

#define ZBUFS_SIZE 128

struct ztable {
	struct hlist_head zbufs[ZBUFS_SIZE];
	int items;

	struct list_head list_of_free;
};

#define ZBUF_BUF_SIZE (128 * 1024)

struct zbuf {
	struct hlist_node node;
	int cap;
	char buf[ZBUF_BUF_SIZE];
	int n;

	struct list_head in_list;
};

struct ztable *ztable_new(int prewarm)
{
	struct ztable *t = calloc(1, sizeof(struct ztable));
	int i;
	for (i = 0; i < ZBUFS_SIZE; i++) {
		INIT_HLIST_HEAD(&t->zbufs[i]);
	}
	INIT_LIST_HEAD(&t->list_of_free);

	for (i = 0; i < prewarm; i++) {
		struct zbuf *z = calloc(1, sizeof(struct zbuf));
		z->cap = ZBUF_BUF_SIZE;
		z->n = 0;
		list_add(&z->in_list, &t->list_of_free);
	}
	return t;
}

struct zbuf *zbuf_new(struct ztable *t)
{
	if (!list_empty(&t->list_of_free)) {
		struct zbuf *z = list_first_entry(&t->list_of_free, struct zbuf,
						  in_list);
		list_del(&z->in_list);
		return z;
	}

	struct zbuf *z = calloc(1, sizeof(struct zbuf));
	z->cap = ZBUF_BUF_SIZE;
	return z;
}

void zbuf_free(struct ztable *t, struct zbuf *z)
{
	z->n = 0;
	list_add(&z->in_list, &t->list_of_free);
}

void zbuf_schedule(struct ztable *t, struct zbuf *z, int n)
{
	z->n = n;
	t->items += 1;
	hlist_add_head(&z->node, &t->zbufs[n % ZBUFS_SIZE]);
}

void zbuf_deschedule_and_free(struct ztable *t, int n)
{
	int ctr = 0;
	int i;
	for (i = 0; i < ZBUFS_SIZE; i++) {
		struct hlist_node *pos, *tmp;
		hlist_for_each_safe(pos, tmp, &t->zbufs[i])
		{
			struct zbuf *z = hlist_entry(pos, struct zbuf, node);
			if (z->n == n) {
				hlist_del(&z->node);
				z->n = 0;
				list_add(&z->in_list, &t->list_of_free);
				t->items -= 1;
				ctr += 1;
			}
		}
	}
	if (ctr != 1) {
		FATAL("ctr=%d", ctr);
	}
}

void ztable_empty(struct ztable *t)
{
	int i;
	for (i = 0; i < ZBUFS_SIZE; i++) {
		struct hlist_node *pos, *tmp;
		hlist_for_each_safe(pos, tmp, &t->zbufs[i])
		{
			struct zbuf *z = hlist_entry(pos, struct zbuf, node);
			hlist_del(&z->node);
			free(z);
			t->items -= 1;
		}
	}
	if (t->items) {
		abort();
	}
}

int ztable_items(struct ztable *t) { return t->items; }

int zbuf_cap(struct zbuf *z) { return z->cap; }
char *zbuf_buf(struct zbuf *z) { return z->buf; }

int ztable_reap_completions(struct ztable *t, int fd, int block)
{
	int ctr = 0;

	/* Reap notifications */
#define ERR_ITEMS 4
	char ctl_buf[ERR_ITEMS][512];
	struct mmsghdr mmsghdr[ERR_ITEMS] = {};
	int i;
	for (i = 0; i < ERR_ITEMS; i++) {
		struct msghdr *m = &mmsghdr[i].msg_hdr;
		m->msg_control = &ctl_buf[i][0];
		m->msg_controllen = 512;
		m->msg_flags = MSG_ERRQUEUE;
	}

	if (block) {
		struct pollfd fds[1] = {{.fd = fd, .events = POLLERR}};
		poll(fds, 1, -1);
	}
	int err_msgs = recvmmsg(fd, mmsghdr, ERR_ITEMS, MSG_ERRQUEUE, NULL);
	for (i = 0; i < err_msgs; i++) {
		struct msghdr *m = &mmsghdr[i].msg_hdr;
		struct cmsghdr *cm = CMSG_FIRSTHDR(m);
		if (cm->cmsg_level != SOL_IP && cm->cmsg_type != IP_RECVERR) {
			PFATAL("expecting SOL_IP, IPRECVERR");
		}

		struct sock_extended_err *serr = (void *)CMSG_DATA(cm);
		if (serr->ee_errno != 0 ||
		    serr->ee_origin != SO_EE_ORIGIN_ZEROCOPY) {
			PFATAL("expecting errno=0, ee_origin "
			       "SO_EE_ORIGIN_ZEROCOPY");
		}

		uint32_t j;
		for (j = serr->ee_info; j <= serr->ee_data; j++) {
			zbuf_deschedule_and_free(t, j);
			ctr += 1;
		}
	}
	return ctr;
}
