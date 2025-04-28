#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <ringleader.h>
#include <string.h>


off_t musl_ringleader_do_lseek(
		struct ringleader *rl,
		int fd,
		off_t offset,
		int whence)
{
	int32_t id = ringleader_prep_lseek(rl, fd, offset, whence);
	void * cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);
	return musl_ringleader_wait_result(rl, cookie);
}

off_t musl_ringleader_lseek(int fd, off_t offset, int whence)
{
	struct ringleader *rl = get_ringleader();

	if(musl_rl_async_fd_check(fd))
	{
		return musl_rl_async_fd_lseek(rl, fd, offset, whence);
	}

	return musl_ringleader_do_lseek(rl, fd, offset, whence);
}
#endif

off_t __lseek(int fd, off_t offset, int whence)
{
#ifndef _CERTIKOS_
#ifdef SYS__llseek
	off_t result;
	return syscall(SYS__llseek, fd, offset>>32, offset, &result, whence) ? -1 : result;
#else
	return syscall(SYS_lseek, fd, offset, whence);
#endif
#else
	return __syscall_ret(musl_ringleader_lseek(fd, offset, whence));
#endif
}

weak_alias(__lseek, lseek);
