#include <sys/socket.h>

#ifdef _CERTIKOS_
#include <string.h>
#include "certikos_impl.h"
#include "ringleader.h"
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
	void * cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);

	int ret = musl_ringleader_wait_result(rl, cookie);
	if(ret >= 0){
		memcpy(buf, shmem, ret);
	}
	return __syscall_ret(ret);
	#endif
}
