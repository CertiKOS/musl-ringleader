#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"

pid_t musl_ringleader_getppid()
{
	struct ringleader *rl = get_ringleader();
	int32_t id = ringleader_prep_getppid(rl);
	void * cookie = musl_ringleader_set_cookie(rl, id);

#ifdef MUSL_RINGLEADER_PROFILE
	overhead_declare();
	overheads_start(&musl_overheads, track_ringleader_musl_getppid);
#endif

	ringleader_submit(rl);
	pid_t ret = musl_ringleader_wait_result(rl, cookie);

#ifdef MUSL_RINGLEADER_PROFILE
	overheads_end(&musl_overheads, track_ringleader_musl_getppid);
#endif

	return ret;
}

#endif


pid_t getppid(void)
{
	return __syscall(SYS_getppid);
}
