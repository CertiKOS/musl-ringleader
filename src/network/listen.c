#include <sys/socket.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#include <string.h>
#endif

int listen(int fd, int backlog)
{
	#ifndef _CERTIKOS_
	return socketcall(listen, fd, backlog, 0, 0, 0, 0);
	#else
    struct ringleader *rl    = get_ringleader();
    int                id = ringleader_prep_listen(rl, fd, backlog);
    ringleader_set_user_data(rl, id, (void *)LISTEN_COOKIE);
    ringleader_submit(rl);

    struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
    if (cqe->user_data == LISTEN_COOKIE)
    {
        int ret = cqe->res;
        ringleader_consume_cqe(rl, cqe);
        return __syscall_ret(ret);
    }
    else
    {
        ringleader_consume_cqe(rl, cqe);
        certikos_puts("Did not get expected ringleader listen cookie");
        return __syscall_ret(-EINVAL);
    }
#endif
}
