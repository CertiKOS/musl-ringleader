#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"

pid_t musl_ringleader_getpgid(pid_t pid)
{
	struct ringleader *rl = get_ringleader();
	int32_t id = ringleader_sqe_getpgid_await(rl, pid);
	void * cookie = musl_ringleader_set_cookie(rl, id);

	ringleader_submit(rl);
	pid_t ret = musl_ringleader_wait_result(rl, cookie);

	return ret;
}

#endif


pid_t getpgid(pid_t pid)
{
	return syscall(SYS_getpgid, pid);
}
