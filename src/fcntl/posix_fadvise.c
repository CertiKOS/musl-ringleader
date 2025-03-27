#include <fcntl.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#endif

int posix_fadvise(int fd, off_t base, off_t len, int advice)
{
	#ifndef _CERTIKOS_
	#if defined(SYSCALL_FADVISE_6_ARG)
		/* Some archs, at least arm and powerpc, have the syscall
		* arguments reordered to avoid needing 7 argument registers
		* due to 64-bit argument alignment. */
		return -__syscall(SYS_fadvise, fd, advice,
			__SYSCALL_LL_E(base), __SYSCALL_LL_E(len));
	#else
		return -__syscall(SYS_fadvise, fd, __SYSCALL_LL_O(base),
			__SYSCALL_LL_E(len), advice);
	#endif
	#else
	struct ringleader *rl = get_ringleader();

	uint32_t id = ringleader_prep_fadvise(rl, fd, base, len, advice);
	void * cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);

	return __syscall_ret(musl_ringleader_wait_result(rl, cookie));
	#endif
}
