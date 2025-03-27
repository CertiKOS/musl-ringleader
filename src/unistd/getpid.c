#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"

pid_t musl_ringleader_getpid()
{
	struct ringleader *rl = get_ringleader();

	uint32_t id = ringleader_prep_getpid(rl);
	void * cookie = musl_ringleader_set_cookie(rl, id);

	ringleader_submit(rl);

	return musl_ringleader_wait_result(rl, cookie);
}

#endif

pid_t getpid(void)
{
	return __syscall(SYS_getpid);
}
