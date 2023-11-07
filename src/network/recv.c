#include <sys/socket.h>

#ifdef _CERTIKOS_
#include <string.h>
#include "certikos_impl.h"
#include "ringleader.h"

#define RECV_COOKIE (2740787)
#endif

ssize_t recv(int fd, void *buf, size_t len, int flags)
{
	//TODO add in recvfrom for udp
	#ifndef _CERTIKOS_
	return recvfrom(fd, buf, len, flags, 0, 0);
	#else
	struct ringleader *rl = get_ringleader();
	void *shmem = get_rl_shmem_singleton();

	int id = ringleader_prep_recv(rl, fd, shmem, len, flags);
	ringleader_set_user_data(rl, id, (void *)RECV_COOKIE);
	ringleader_submit(rl);

	struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
	if(cqe->user_data == RECV_COOKIE){
		if(cqe->res >= 0){
			memcpy(buf, shmem, cqe->res);	
		}
		int ret = cqe->res;
		ringleader_consume_cqe(rl, cqe);
		return __syscall_ret(ret);
	} else {
        ringleader_consume_cqe(rl, cqe);
        certikos_puts("Did not get expected ringleader recv cookie");
        __syscall_ret(-EINVAL);
    }
	#endif
}
