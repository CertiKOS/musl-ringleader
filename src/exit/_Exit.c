#include <stdlib.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "ringleader.h"
#include "certikos_impl.h"
#endif

_Noreturn void _Exit(int ec)
{
	#ifdef _CERTIKOS_
	struct ringleader *rl = get_ringleader();
	musl_rl_async_fd_finish_all(rl);

	struct ringleader_arena * arena = musl_ringleader_get_arena(rl, 64);
	int32_t sqe_id = ringleader_prep_exit_proxy_await(rl, ec, arena);
	void *cookie = musl_ringleader_set_cookie(rl, sqe_id);
	ringleader_submit(rl);
	(void) musl_ringleader_wait_result(rl, cookie);

	ringleader_free_arena(rl, arena);
	#endif
	__syscall(SYS_exit_group, ec);
	for (;;) __syscall(SYS_exit, ec);
}
