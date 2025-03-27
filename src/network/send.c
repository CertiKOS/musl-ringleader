#include <sys/socket.h>

#ifdef _CERTIKOS_
#include <string.h>
#include "certikos_impl.h"
#include "ringleader.h"
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
	void *cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);

	return __syscall_ret(musl_ringleader_wait_result(rl, cookie));
	#endif
}
