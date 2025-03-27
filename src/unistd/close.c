#include <unistd.h>
#include <errno.h>
#include "aio_impl.h"
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#endif

static int dummy(int fd)
{
	return fd;
}

weak_alias(dummy, __aio_close);

int close(int fd)
{
	int r;
#ifndef _CERTIKOS_
	fd = __aio_close(fd);
	r = __syscall_cp(SYS_close, fd);
#else
	struct ringleader* rl = get_ringleader();
	int id = ringleader_prep_close(rl, fd);
	void * cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);
	r = musl_ringleader_wait_result(rl, cookie);
#endif
	if (r == -EINTR) r = 0;
	return __syscall_ret(r);
}
