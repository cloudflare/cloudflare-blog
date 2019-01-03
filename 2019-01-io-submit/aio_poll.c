#include <arpa/inet.h>
#include <linux/aio_abi.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#define PFATAL(x...)                                                           \
	do {                                                                   \
		fprintf(stderr, "[-] SYSTEM ERROR : " x);                      \
		fprintf(stderr, "\n\tLocation : %s(), %s:%u\n", __FUNCTION__,  \
			__FILE__, __LINE__);                                   \
		perror("      OS message ");                                   \
		fprintf(stderr, "\n");                                         \
		exit(EXIT_FAILURE);                                            \
	} while (0)

inline static int io_setup(unsigned nr, aio_context_t *ctxp)
{
	return syscall(__NR_io_setup, nr, ctxp);
}

inline static int io_destroy(aio_context_t ctx)
{
	return syscall(__NR_io_destroy, ctx);
}

inline static int io_submit(aio_context_t ctx, long nr, struct iocb **iocbpp)
{
	return syscall(__NR_io_submit, ctx, nr, iocbpp);
}

#define AIO_RING_MAGIC 0xa10a10a1
struct aio_ring {
	unsigned id; /** kernel internal index number */
	unsigned nr; /** number of io_events */
	unsigned head;
	unsigned tail;

	unsigned magic;
	unsigned compat_features;
	unsigned incompat_features;
	unsigned header_length; /** size of aio_ring */

	struct io_event events[0];
};

/* Stolen from kernel arch/x86_64.h */
#ifdef __x86_64__
#define read_barrier() __asm__ __volatile__("lfence" ::: "memory")
#else
#ifdef __i386__
#define read_barrier() __asm__ __volatile__("" : : : "memory")
#else
#define read_barrier() __sync_synchronize()
#endif
#endif

/* Code based on axboe/fio:
 * https://github.com/axboe/fio/blob/702906e9e3e03e9836421d5e5b5eaae3cd99d398/engines/libaio.c#L149-L172
 */
inline static int io_getevents(aio_context_t ctx, long min_nr, long max_nr,
			       struct io_event *events,
			       struct timespec *timeout)
{
	int i = 0;

	struct aio_ring *ring = (struct aio_ring *)ctx;
	if (ring == NULL || ring->magic != AIO_RING_MAGIC) {
		goto do_syscall;
	}

	while (i < max_nr) {
		unsigned head = ring->head;
		if (head == ring->tail) {
			/* There are no more completions */
			break;
		} else {
			/* There is another completion to reap */
			events[i] = ring->events[head];
			read_barrier();
			ring->head = (head + 1) % ring->nr;
			i++;
		}
	}

	if (i == 0 && timeout != NULL && timeout->tv_sec == 0 &&
	    timeout->tv_nsec == 0) {
		/* Requested non blocking operation. */
		return 0;
	}

	if (i && i >= min_nr) {
		return i;
	}

do_syscall:
	return syscall(__NR_io_getevents, ctx, min_nr - i, max_nr - i,
		       &events[i], timeout);
}

#ifndef IOCB_CMD_POLL
#define IOCB_CMD_POLL 5 /* from 4.18 */
#endif

char payload[] = {0xde, 0xad, 0x01, 0x20, 0x00, 0x01, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x0a, 0x63, 0x6c, 0x6f,
		  0x75, 0x64, 0x66, 0x6c, 0x61, 0x72, 0x65, 0x03,
		  0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01};

int main()
{
	struct sockaddr_in sin = {
		.sin_family = AF_INET,
		.sin_port = htons(53),
		.sin_addr = {0x01010101},
	};

	int sd = socket(sin.sin_family, SOCK_STREAM, IPPROTO_TCP);
	if (sd < 0) {
		PFATAL("socket()");
	}

	int r = connect(sd, (struct sockaddr *)&sin, sizeof(sin));
	if (r < 0) {
		PFATAL("connect()");
		return -1;
	}

	write(sd, payload, sizeof(payload));

	aio_context_t ctx = 0;
	r = io_setup(128, &ctx);
	if (r < 0) {
		PFATAL("io_setup()");
	}

	struct iocb cb = {.aio_fildes = sd,
			  .aio_lio_opcode = IOCB_CMD_POLL,
			  .aio_buf = POLLIN};
	struct iocb *list_of_iocb[1] = {&cb};

	r = io_submit(ctx, 1, list_of_iocb);
	if (r != 1) {
		PFATAL("io_submit()");
	}

	struct io_event events[1] = {};
	r = io_getevents(ctx, 1, 1, events, NULL);
	if (r != 1) {
		PFATAL("io_getevents()");
	}
	io_destroy(ctx);
	close(sd);
	return 0;
}
