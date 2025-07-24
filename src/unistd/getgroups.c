#include <unistd.h>
#include "syscall.h"


#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#include <string.h>

static int musl_ringleader_getgroups(int count, gid_t list[])
{
	//TODO: optimization: if count is 0, we can prefetch once the system call
	//returns, on the next call, if the sizes match, we can just wait for that
	//one to return.
	struct ringleader *rl = get_ringleader();
	struct ringleader_arena * arena = NULL;
	void * shmem = NULL;

	if(count > 0 && list != NULL)
	{
		arena = musl_ringleader_get_arena(rl, sizeof(gid_t) * count);
		if(arena == NULL)
		{
			return __syscall_ret(-ENOMEM);
		}

		shmem = ringleader_arena_push(arena, sizeof(gid_t) * count);
		if(shmem == NULL)
		{
			ringleader_free_arena(rl, arena);
			return __syscall_ret(-ENOMEM);
		}
	}

	uint32_t id = ringleader_sqe_getgroups_await(rl, count, shmem);
	void * cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);
	pid_t ret = musl_ringleader_wait_result(rl, cookie);

	if(count > 0 && list != NULL && ret > 0)
	{
		if(ret > count)
		{
			ringleader_free_arena(rl, arena);
			return __syscall_ret(-EINVAL);
		}

		memcpy(list, shmem, sizeof(gid_t) * ret);
		ringleader_free_arena(rl, arena);
	}

	return __syscall_ret(ret);
}

#endif


int getgroups(int count, gid_t list[])
{
#ifdef _CERTIKOS_
	return musl_ringleader_getgroups(count, list);
#else
	return syscall(SYS_getgroups, count, list);
#endif
}
