#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include <string.h>
#include <stdio.h>
#include <ringleader.h>
#include <certikos.h>
#include "certikos_impl.h"
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
        uint64_t cookie = cqe->user_data;
		ringleader_consume_cqe(rl, cqe);
		certikos_printf("read: unexpected cookie %llu", cookie);
		return __syscall_ret(-EIO);
	}
	#endif
}
