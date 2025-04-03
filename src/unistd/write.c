#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#include <string.h>



ssize_t musl_ringleader_write(int fd, const void *buf, size_t count)
{
	int ret;
	struct ringleader *rl = get_ringleader();
	struct ringleader_arena *arena =
		musl_ringleader_get_arena(rl, count);
	if(!arena)
		return -ENOMEM;

	if(musl_rl_async_fd_check(fd))
	{
		//TODO cache instead of waiting for arena above
		//TODO check status of older writes
		ret = musl_rl_async_pwrite(rl, arena, fd, buf, count, -1);
		if(ret != count)
		{
			ringleader_free_arena(rl, arena);
		}
		return ret;
	}


	void *shmem = ringleader_arena_apush(arena, buf, count);
	if(!shmem)
		return -ENOMEM;

	int32_t id = ringleader_prep_write(rl, fd, shmem, count, -1);
	void * cookie = musl_ringleader_set_cookie(rl, id);

	ringleader_submit(rl);

	ret = musl_ringleader_wait_result(rl, cookie);
	ringleader_free_arena(rl, arena);
	return ret;
}

#endif

ssize_t write(int fd, const void *buf, size_t count)
{
	return syscall_cp(SYS_write, fd, buf, count);
}
