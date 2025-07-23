#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"

pid_t musl_ringleader_getgid()
{
	struct ringleader *rl = get_ringleader();
	int32_t id = ringleader_sqe_getgid_await(rl);
	void * cookie = musl_ringleader_set_cookie(rl, id);

	ringleader_submit(rl);
	pid_t ret = musl_ringleader_wait_result(rl, cookie);

	return ret;
}

#endif

gid_t getgid(void)
{
	return __syscall(SYS_getgid);
}
