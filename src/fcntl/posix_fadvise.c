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
	ringleader_set_user_data(rl, id, (void *) ADVISE_COOKIE);
	ringleader_submit(rl);

	syscall(SYS_sched_yield);

	struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
	if((uint64_t) cqe->user_data == ADVISE_COOKIE){
		int ret = cqe->res;
		ringleader_consume_cqe(rl, cqe);
		return ret;
	} else {
		ringleader_consume_cqe(rl, cqe);
		certikos_puts("Did not get expected ringleader fadvise completion token");
		return EINVAL;
	}
	#endif
}
