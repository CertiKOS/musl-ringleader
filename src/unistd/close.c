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
	ringleader_set_user_data(rl, id, (void*) CLOSE_COOKIE);
	ringleader_submit(rl);

	syscall(SYS_sched_yield);
    struct io_uring_cqe* cqe = ringleader_get_cqe(rl);
    if (cqe->user_data == CLOSE_COOKIE)
    {
        r = cqe->res;
        ringleader_consume_cqe(rl, cqe);
    }
    else
    {
        ringleader_consume_cqe(rl, cqe);
        certikos_puts("Did not get expected ringleader close cookie");
        r = -EINVAL;
	    }
	#endif
	if (r == -EINTR) r = 0;
	return __syscall_ret(r);
}
