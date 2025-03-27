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
	int id = ringleader_prep_listen(rl, fd, backlog);
	void *cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);

	return __syscall_ret(musl_ringleader_wait_result(rl, cookie));
#endif
}
