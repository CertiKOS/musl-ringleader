#include <unistd.h>
#include "syscall.h"


#ifdef _CERTIKOS_
#include <string.h>
#include "certikos_impl.h"
#include "ringleader.h"
#endif


int unlinkat(int fd, const char *path, int flag)
{
	#ifndef _CERTIKOS_
	return syscall(SYS_unlinkat, fd, path, flag);
	#else
	struct ringleader *rl = get_ringleader();
	void *shmem = get_rl_shmem_singleton();

	strcpy(shmem, path);

	int id = ringleader_prep_unlinkat(rl, fd, shmem, flag);
	ringleader_set_user_data(rl, id, (void*) UNLINK_COOKIE);
	ringleader_submit(rl);

	syscall(SYS_sched_yield);

	struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
	if(cqe->user_data == UNLINK_COOKIE){
		int ret = cqe->res;
		ringleader_consume_cqe(rl, cqe);
		return __syscall_ret(ret);
	} else {
		ringleader_consume_cqe(rl, cqe);
        certikos_puts("Did not get expected ringleader unlinkat cookie");
        return __syscall_ret(-EINVAL);
	}
	#endif
}
