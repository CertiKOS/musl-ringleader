#include <sys/socket.h>

#ifdef _CERTIKOS_
#include <string.h>
#include "certikos_impl.h"
#include "ringleader.h"

#define SEND_COOKIE (2445150)
#endif

ssize_t send(int fd, const void *buf, size_t len, int flags)
{
	//TODO add in sendto for udp
	#ifndef _CERTIKOS_
	return sendto(fd, buf, len, flags, 0, 0);
	#else
	struct ringleader *rl = get_ringleader();
	void *shmem = get_rl_shmem_singleton();

	memcpy(shmem, buf, len);

	int id = ringleader_prep_send(rl, fd, shmem, len, flags);
	ringleader_set_user_data(rl, id, (void *)SEND_COOKIE);
	ringleader_submit(rl);

    syscall(SYS_sched_yield);

    struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
	if(cqe->user_data == SEND_COOKIE){
		int ret = cqe->res;
		ringleader_consume_cqe(rl, cqe);
		return __syscall_ret(ret);
	} else {
		ringleader_consume_cqe(rl, cqe);
		certikos_puts("Did not get expected ringleader send cookie");
		return __syscall_ret(-EINVAL);
	}
	#endif
}
