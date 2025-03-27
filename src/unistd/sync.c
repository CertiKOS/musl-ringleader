#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"

void musl_ringleader_sync()
{
	struct ringleader *rl = get_ringleader();
	uint32_t id = ringleader_prep_sync(rl);
	void * cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);
	(void) musl_ringleader_wait_result(rl, cookie);
}

#endif

void sync(void)
{
	__syscall(SYS_sync);
}
