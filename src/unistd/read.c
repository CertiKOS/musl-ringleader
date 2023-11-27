#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"

#include <string.h>
#include <stdio.h>

#define READ_COOKIE (3705175)
#endif

static inline int min(const int a, const int b) {
    return a < b ? a : b;
}

ssize_t read(int fd, void *buf, size_t count)
{
	#ifndef _CERTIKOS_
	return syscall_cp(SYS_read, fd, buf, count);
	#else
	struct ringleader *rl = get_ringleader();
    //TODO how tight can this bound be
	void *shmem = (count <= SHMEM_SIZE) ? get_rl_shmem_singleton() :
		alloc_new_rl_shmem(count + 0x100);
	if(shmem == NULL) return __syscall_ret(-ENOMEM);

	int32_t id = ringleader_prep_read(rl, fd, shmem, count, -1);
	ringleader_set_user_data(rl, id, (void *) READ_COOKIE);
	ringleader_submit(rl);

	struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
	if((uint64_t) cqe->user_data == READ_COOKIE){
		__s32 ret = cqe->res;
		ringleader_consume_cqe(rl, cqe);
		if(ret > 0){
			memcpy(buf, shmem, ret);
		}
		return __syscall_ret(ret);
	} else {
		ringleader_consume_cqe(rl, cqe);
		certikos_puts("Did not get expected ringleader read completion token");
		return __syscall_ret(-EIO);
	}
	#endif
}
