#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <ringleader.h>
#include <certikos.h>
#include <string.h>

int musl_ringleader_fsync(int fd)
{
	struct ringleader *rl = get_ringleader();
	int id = ringleader_prep_fsync(rl, fd);
	void * cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);
	return musl_ringleader_wait_result(rl, cookie);
}
#endif




int fsync(int fd)
{
	return syscall_cp(SYS_fsync, fd);
}
