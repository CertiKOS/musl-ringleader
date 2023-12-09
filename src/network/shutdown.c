#include <sys/socket.h>
#include "syscall.h"

#ifdef _CERTIKOS_

#include "certikos_impl.h"
#include <ringleader.h>
#include <certikos.h>

int musl_ringleader_shutdown(int fd, int how){
	int ret;
	int32_t id;
	struct ringleader *rl = get_ringleader();

	id = ringleader_prep_shutdown(rl, fd, how);
	ringleader_set_user_data(rl, id, (void *) SHUTDOWN_COOKIE);
	ringleader_submit(rl);

	struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
	if((uint64_t) cqe->user_data != SHUTDOWN_COOKIE){
		ringleader_consume_cqe(rl, cqe);
		certikos_puts("shutdown: unexpected ringleader cookie.");
		return -EINVAL;
	}

	ret = cqe->res;
	ringleader_consume_cqe(rl, cqe);

	return ret;
}

#endif

int shutdown(int fd, int how)
{
	return socketcall(shutdown, fd, how, 0, 0, 0, 0);
}
