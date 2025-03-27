#include <unistd.h>
#include "syscall.h"
#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <ringleader.h>
#include <limits.h>

int musl_ringleader_chdir(const char *path)
{
	struct ringleader *rl = get_ringleader();

	struct ringleader_arena *arena = musl_ringleader_get_arena(rl, PATH_MAX);
	if(!arena)
	{
		return -ENOMEM;
	}

	void *shmem = ringleader_arena_spush(arena, path);
	if(!shmem)
	{
		ringleader_free_arena(rl, arena);
		return -ENOMEM;
	}

	int32_t id = ringleader_prep_chdir(rl, shmem);
	void * cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);

	int ret = musl_ringleader_wait_result(rl, cookie);
	ringleader_free_arena(rl, arena);
	return ret;
}
#endif

int chdir(const char *path)
{
	return syscall(SYS_chdir, path);
}
