#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include <string.h>
#include <stdio.h>
#include <ringleader.h>
#include <certikos.h>
#include "certikos_impl.h"

ssize_t musl_ringleader_read(int fd, void *buf, size_t count)
{
	struct ringleader *rl = get_ringleader();

	struct ringleader_arena *arena =
		musl_ringleader_get_arena(rl, count);
	if(!arena)
		return __syscall_ret(-ENOMEM);

	void *shmem = ringleader_arena_push(arena, count);
	if(!shmem)
		return __syscall_ret(-ENOMEM);

	int32_t id = ringleader_prep_read(rl, fd, shmem, count, -1);
	ringleader_set_user_data(rl, id, (void *) READ_COOKIE);
	ringleader_submit(rl);

	struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
	if((uint64_t) cqe->user_data == READ_COOKIE){
		__s32 ret = cqe->res;
		ringleader_consume_cqe(rl, cqe);
		if(ret > 0){
			ringleader_arena_apop(arena, shmem, buf);
		}
		ringleader_free_arena(rl, arena);
		return __syscall_ret(ret);
	} else {
		uint64_t cookie = cqe->user_data;
		ringleader_consume_cqe(rl, cqe);
		ringleader_free_arena(rl, arena);
		certikos_printf("read: unexpected cookie %llu", cookie);
		return __syscall_ret(-EIO);
	}
}


#endif

ssize_t read(int fd, void *buf, size_t count)
{
	return syscall_cp(SYS_read, fd, buf, count);
}
