#include <sys/socket.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include <string.h>
#include "certikos_impl.h"
#include "ringleader.h"
#endif


//TODO currently not workings
int bind(int fd, const struct sockaddr *addr, socklen_t len)
{
	#ifndef _CERTIKOS_
	return socketcall(bind, fd, addr, len, 0, 0, 0);
	#else
	struct ringleader *rl = get_ringleader();
	void *shmem = get_rl_shmem_singleton();
	memcpy(shmem, addr, len);

	int id = ringleader_prep_bind(rl, fd, shmem, len);
	ringleader_set_user_data(rl, id, (void *) BIND_COOKIE);
	ringleader_submit(rl);

	struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
    if (cqe->user_data == BIND_COOKIE)
    {
        int ret = cqe->res;
		ringleader_consume_cqe(rl, cqe);
		return __syscall_ret(ret);
    }
    else
    {
        ringleader_consume_cqe(rl, cqe);
		certikos_puts("Did not get expected ringleader bind cookie");
		return __syscall_ret(-EINVAL);
    }
#endif
}
