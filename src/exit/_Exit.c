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
	struct ringleader_arena * arena = musl_ringleader_get_arena(rl, 0x1000);
	ringleader_exit_proxy(rl, ec, ringleader_arena_push(arena, 0x1000));
	struct io_uring_cqe cqe;
	(void) ringleader_get_cqe(rl, &cqe);
	ringleader_free_arena(rl, arena);
	#endif
	__syscall(SYS_exit_group, ec);
	for (;;) __syscall(SYS_exit, ec);
}
