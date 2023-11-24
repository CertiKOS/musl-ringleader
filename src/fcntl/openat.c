#include <fcntl.h>
#include <stdarg.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#include <string.h>

#define OPENAT_COOKIE (350652)
#endif
//syscall_cp has syscall ret

int openat(int fd, const char *filename, int flags, ...)
{
	mode_t mode = 0;

	if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}

	#ifndef _CERTIKOS_
	return syscall_cp(SYS_openat, fd, filename, flags|O_LARGEFILE, mode);
	#else 
	struct ringleader *rl = get_ringleader();
	void *shmem = get_rl_shmem_singleton();
	strcpy(shmem, filename);

	int32_t id = ringleader_prep_openat(rl, fd, shmem, flags, mode);
	ringleader_set_user_data(rl, id, (void *) OPENAT_COOKIE);
	ringleader_submit(rl);

	syscall(SYS_sched_yield);

	struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
	if((uint64_t) cqe->user_data == OPENAT_COOKIE){
		int fd = cqe->res;
		ringleader_consume_cqe(rl,cqe);
		return __syscall_ret(fd);
	} else {
		ringleader_consume_cqe(rl, cqe);
		certikos_puts("Did not get expected ringleader open completion token");
		return __syscall_ret(-EINVAL);
	}
	#endif
}
