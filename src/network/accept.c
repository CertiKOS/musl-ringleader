#include <sys/socket.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#include <string.h>
#endif

int accept(int fd, struct sockaddr *restrict addr, socklen_t *restrict len)
{
	#ifndef _CERTIKOS_
	return socketcall_cp(accept, fd, addr, len, 0, 0, 0);
	#else
    struct ringleader *rl    = get_ringleader();
	void              *shmem = get_rl_shmem_singleton();
	struct sockaddr *sockaddr_ptr = (struct sockaddr *)((uintptr_t) shmem + (sizeof *len));
	socklen_t * socklen_ptr = shmem;
	*socklen_ptr = *len;
    int                id = ringleader_prep_accept(rl, fd, sockaddr_ptr, socklen_ptr, 0);
    ringleader_set_user_data(rl, id, (void *)ACCEPT_COOKIE);
    ringleader_submit(rl);

    struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
    if (cqe->user_data == ACCEPT_COOKIE)
    {
        if (cqe->res >= 0)
        {
            memcpy(addr, sockaddr_ptr, *socklen_ptr);
			*len = *socklen_ptr;
        }
        int ret = cqe->res;
        ringleader_consume_cqe(rl, cqe);
        return __syscall_ret(ret);
    }
    else
    {
        ringleader_consume_cqe(rl, cqe);
        certikos_puts("Did not get expected ringleader accept cookie");
        return __syscall_ret(-EINVAL);
    }
#endif
}
