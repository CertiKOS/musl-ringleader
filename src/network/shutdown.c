#include <sys/socket.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <ringleader.h>
#include <certikos.h>

int musl_ringleader_shutdown(int fd, int how){
	struct ringleader *rl = get_ringleader();
	int32_t id = ringleader_prep_shutdown(rl, fd, how);
	void * cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);
	return musl_ringleader_wait_result(rl, cookie);
}
#endif

int shutdown(int fd, int how)
{
	return socketcall(shutdown, fd, how, 0, 0, 0, 0);
}
