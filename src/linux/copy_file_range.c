#define _GNU_SOURCE
#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <ringleader.h>

ssize_t musl_ringleader_copy_file_range(
		int fd_in,
		off_t *off_in,
		int fd_out,
		off_t *off_out,
		size_t len,
		unsigned flags)
{
	struct ringleader *rl = get_ringleader();
	int32_t id = ringleader_sqe_copy_file_range_await(rl, fd_in, off_in, fd_out, off_out, len, flags);
	void * cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);
	return musl_ringleader_wait_result(rl, cookie);
}
#endif /* _CERTIKOS_ */


ssize_t copy_file_range(int fd_in, off_t *off_in, int fd_out, off_t *off_out, size_t len, unsigned flags)
{
#ifndef _CERTIKOS_
	return syscall(SYS_copy_file_range, fd_in, off_in, fd_out, off_out, len, flags);
#else
	return __syscall_ret(musl_ringleader_copy_file_range(fd_in, off_in, fd_out, off_out, len, flags));
#endif /* _CERTIKOS_ */
}
