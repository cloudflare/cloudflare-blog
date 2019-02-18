
inline int io_setup(unsigned nr, aio_context_t *ctxp)
{
	return syscall(__NR_io_setup, nr, ctxp);
}

inline int io_destroy(aio_context_t ctx)
{
	return syscall(__NR_io_destroy, ctx);
}

inline int io_submit(aio_context_t ctx, long nr, struct iocb **iocbpp)
{
	return syscall(__NR_io_submit, ctx, nr, iocbpp);
}

#define read_barrier() __asm__ __volatile__("lfence" ::: "memory")

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

inline int io_getevents(aio_context_t ctx, long min_nr, long max_nr,
			struct io_event *events, struct timespec *timeout)
{
	struct aio_ring *ring = (struct aio_ring *)ctx;
	if (ring == NULL || ring->magic != AIO_RING_MAGIC) {
		goto do_syscall;
	}

	int i = 0;
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
	    timeout->tv_nsec == 0 && ring->head == ring->tail) {
		return 0;
	}

	if (i) {
		return i;
	}

do_syscall:;
	return syscall(__NR_io_getevents, ctx, min_nr, max_nr, events, timeout);
}
