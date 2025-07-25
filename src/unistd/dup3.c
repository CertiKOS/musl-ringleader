#define _GNU_SOURCE
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"

int musl_ringleader_dup3(int old, int new, int flags)
{
	struct ringleader *rl = get_ringleader();

	musl_rl_async_fd_deasync(rl, new);

	int32_t id = ringleader_sqe_dup3_await(rl, old, new, flags);
	void * cookie = musl_ringleader_set_cookie(rl, id);

	ringleader_submit(rl);
	pid_t ret = musl_ringleader_wait_result(rl, cookie);

	return ret;
}

#endif


int __dup3(int old, int new, int flags)
{
	int r;
#ifdef SYS_dup2
	if (old==new) return __syscall_ret(-EINVAL);
	if (flags) {
		while ((r=__syscall(SYS_dup3, old, new, flags))==-EBUSY);
		if (r!=-ENOSYS) return __syscall_ret(r);
		if (flags & ~O_CLOEXEC) return __syscall_ret(-EINVAL);
	}
	while ((r=__syscall(SYS_dup2, old, new))==-EBUSY);
	if (r >= 0 && (flags & O_CLOEXEC))
		__syscall(SYS_fcntl, new, F_SETFD, FD_CLOEXEC);
#else
	while ((r=__syscall(SYS_dup3, old, new, flags))==-EBUSY);
#endif
	return __syscall_ret(r);
}

weak_alias(__dup3, dup3);
