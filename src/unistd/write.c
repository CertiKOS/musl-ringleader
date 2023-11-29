#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#include <string.h>
#endif

ssize_t write(int fd, const void *buf, size_t count)
{
	#ifndef _CERTIKOS_
	return syscall_cp(SYS_write, fd, buf, count);
	#else
	struct ringleader *rl = get_ringleader();

	struct ringleader_arena *arena =
		musl_ringleader_get_arena(rl, count + 0x100);
	if(!arena)
		return __syscall_ret(-ENOMEM);

	void *shmem = ringleader_arena_apush(arena, buf, count);
	if(!shmem)
		return __syscall_ret(-ENOMEM);

	int32_t id = ringleader_prep_write(rl, fd, shmem, count, -1);
	ringleader_set_user_data(rl, id, (void *) WRITE_COOKIE);
	ringleader_submit(rl);

	struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
	if((uint64_t) cqe->user_data == WRITE_COOKIE){
		__s32 ret = cqe->res;
		ringleader_consume_cqe(rl, cqe);
		ringleader_free_arena(rl, arena);
		return __syscall_ret(ret);
	} else {
		ringleader_consume_cqe(rl, cqe);
		ringleader_free_arena(rl, arena);
		certikos_puts("Did not get expected ringleader write completion token");
		return __syscall_ret(-EIO);
	}
	#endif
}
