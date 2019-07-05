#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <unistd.h>

#include "common.h"

struct forksrv {
	const char *afl_no_forksrv;
	uint8_t *afl_area_ptr;
};

struct forksrv *forksrv_new(void)
{
	const char *afl_no_forksrv = getenv("AFL_NO_FORKSRV");

	uint8_t *afl_area_ptr = NULL;
	const char *afl_shm_id_str = getenv("__AFL_SHM_ID");
	if (afl_shm_id_str != NULL) {
		int afl_shm_id = atoi(afl_shm_id_str);
		// shm_id 0 is fine
		afl_area_ptr = shmat(afl_shm_id, NULL, 0);
	}

	if (afl_area_ptr == NULL) {
		fprintf(stderr, "[-] Running outside of AFL\n");
		afl_area_ptr = calloc(1, 1 << 16);
		afl_no_forksrv = "yes";
	}

	struct forksrv *forksrv = calloc(1, sizeof(struct forksrv));
	forksrv->afl_no_forksrv = afl_no_forksrv;
	forksrv->afl_area_ptr = afl_area_ptr;
	return forksrv;
}

int forksrv_on(struct forksrv *forksrv)
{
	return forksrv->afl_no_forksrv == NULL;
}

void forksrv_welcome(struct forksrv *forksrv)
{
	if (forksrv_on(forksrv)) {
		/* __afl_forkserver  */
		/* Phone home and tell the parent that we're OK. If
		 * parent isn't there, assume we're not running in
		 * forkserver mode and just execute program. */
		uint32_t tmp = 0;
		int r = write(198 + 1, &tmp, 4);
		if (r != 4) {
			PFATAL("write(fsrv_st_fd)");
		}
	}
}

int32_t forksrv_cycle(struct forksrv *forksrv, uint32_t child_pid)
{
	uint32_t was_killed = -1;
	if (forksrv_on(forksrv)) {
		/* __afl_fork_wait_loop  */
		/* Wait for parent by reading from the pipe. Abort if
		 * read fails. */
		int r = read(198, &was_killed, 4);
		if (r != 4) {
			// __afl_die
			PFATAL("read(fsrv_ctl_fd)");
		}

		/* Do fork here */

		/* In parent process: write PID to pipe, then wait for child. */
		r = write(198 + 1, &child_pid, 4);
		if (r != 4) {
			PFATAL("read(fsrv_st_fd)");
		}
	}
	return was_killed;
}

void forksrv_status(struct forksrv *forksrv, uint32_t status)
{
	if (forksrv_on(forksrv)) {
		/* Relay wait status to pipe, then loop back. */
		int r = write(198 + 1, &status, 4);
		if (r != 4) {
			PFATAL("write(fsrv_st_fd)");
		}
	}
}

uint8_t *forksrv_area_ptr(struct forksrv *forksrv)
{
	return forksrv->afl_area_ptr;
}

void forksrv_free(struct forksrv *forksrv)
{
	shmdt(forksrv->afl_area_ptr);
	forksrv->afl_area_ptr = MAP_FAILED;
}
