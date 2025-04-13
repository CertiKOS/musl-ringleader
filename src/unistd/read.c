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

	if(musl_rl_async_fd_check(fd))
	{
		return musl_rl_async_fd_read(rl, fd, buf, count);
	}

	struct ringleader_arena *arena =
		musl_ringleader_get_arena(rl, count);
	if(!arena)
		return -ENOMEM;

	void *shmem = ringleader_arena_push(arena, count);
	if(!shmem)
	{
		ringleader_free_arena(rl, arena);
		return -ENOMEM;
	}

	int32_t id = ringleader_prep_read(rl, fd, shmem, count, -1);
	void *cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);

	int ret = musl_ringleader_wait_result(rl, cookie);

	if(ret > 0)
	{
		memcpy(buf, shmem, ret);
	}
	ringleader_free_arena(rl, arena);
	return ret;
}


#endif

ssize_t read(int fd, void *buf, size_t count)
{
#ifdef _CERTIKOS_
	return __syscall_ret(musl_ringleader_read(fd, buf, count));
#else
	return syscall_cp(SYS_read, fd, buf, count);
#endif
}
