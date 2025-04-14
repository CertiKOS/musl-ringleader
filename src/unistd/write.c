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

	if(musl_rl_async_fd_check(fd))
	{
		return musl_rl_async_write(rl, fd, buf, count);
	}

	struct ringleader_arena *arena =
		musl_ringleader_get_arena(rl, count);
	if(!arena)
		return -ENOMEM;

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
#ifdef _CERTIKOS_
	return __syscall_ret(musl_ringleader_write(fd, buf, count));
#else
	return syscall_cp(SYS_write, fd, buf, count);
#endif
}
