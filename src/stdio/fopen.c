#include "stdio_impl.h"
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#ifdef _CERTIKOS_
#include "ringleader.h"
#include "certikos_impl.h"
#define OPENAT_COOKIE (134321)
#endif

FILE *fopen(const char *restrict filename, const char *restrict mode)
{
	FILE *f;
	int fd;
	int flags;

	/* Check for valid initial mode character */
	if (!strchr("rwa", *mode)) {
		errno = EINVAL;
		return 0;
	}

	/* Compute the flags to pass to open() */
	flags = __fmodeflags(mode);

	#ifndef _CERTIKOS_
	fd = sys_open(filename, flags, 0666);
	if (fd < 0) return 0;
	if (flags & O_CLOEXEC)
		__syscall(SYS_fcntl, fd, F_SETFD, FD_CLOEXEC);
	#else
	//TODO: Need to do this later fcntl in for fd_cloexec
		struct ringleader *rl = get_ringleader();
		void* shmem = get_rl_shmem();
		strcpy(shmem, filename);
		//uses flags in flags, and mode from mode
		// int32_t id = ringleader_prep_openat(rl, shmem, flags & (O_RDWR | O_RDONLY | O_WRONLY), flags);
		int32_t id = ringleader_prep_openat(rl, shmem, O_RDWR, 0);
		ringleader_set_user_data(rl, id, (void*) OPENAT_COOKIE);

		ringleader_submit(rl);
		syscall(SYS_sched_yield);

		certikos_puts("Prepped");

		struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
		certikos_puts("Got");
		if((uint64_t) cqe->user_data == OPENAT_COOKIE){
			fd = cqe->res;
			ringleader_consume_cqe(rl, cqe);
			if (fd < 0) return 0;
		} else {
			certikos_puts("Did not get expected RL openat completion token");
			return 0;
		}
	#endif

	f = __fdopen(fd, mode);
	if (f) return f;

	certikos_puts("Not valid");

	#ifndef _CERTIKOS_
		__syscall(SYS_close, fd);
	#else
		//todo add in close
	#endif

	return 0;
}
