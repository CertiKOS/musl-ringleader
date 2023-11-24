#include <fcntl.h>
#include <stdarg.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#include <string.h>

#define OPEN_COOKIE (350653)
#endif

//syscall_cp has syscall ret

int open(const char *filename, int flags, ...)
{
	mode_t mode = 0;

	if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}

	int fd;
	#ifndef _CERTIKOS_
	fd = __sys_open_cp(filename, flags, mode);
	#else
	struct ringleader *rl = get_ringleader();
	void *shmem = get_rl_shmem_singleton();
	strcpy(shmem, filename);

	int32_t id = ringleader_prep_open(rl, shmem, flags, mode);
	ringleader_set_user_data(rl, id, (void *) OPEN_COOKIE);
	ringleader_submit(rl);

	syscall(SYS_sched_yield);

	struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
	if((uint64_t) cqe->user_data == OPEN_COOKIE){
		fd = cqe->res;
		ringleader_consume_cqe(rl,cqe);
	} else {
		ringleader_consume_cqe(rl, cqe);
		certikos_puts("Did not get expected ringleader open completion token");
		fd = -EINVAL;
	}
	#endif

	if (fd>=0 && (flags & O_CLOEXEC)){
		#ifndef _CERTIKOS_
		__syscall(SYS_fcntl, fd, F_SETFD, FD_CLOEXEC);
		#else 
		uint64_t cookies[1];
		cookies[0] = OPEN_COOKIE + 1;
		prep_musl_ringleader_fcntl(0, cookies[0], fd, F_SETFD, FD_CLOEXEC);
		struct ringleader *rl = get_ringleader();
		ringleader_submit(rl);
		syscall(SYS_sched_yield);
		reap_musl_ringleader_fcntl(1, cookies);
		#endif
	}
		

	return __syscall_ret(fd);
}
