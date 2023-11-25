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
	ingleader_exit_proxy(rl, ec);
	#endif
	__syscall(SYS_exit_group, ec);
	for (;;) __syscall(SYS_exit, ec);
}
