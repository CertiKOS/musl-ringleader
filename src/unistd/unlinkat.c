#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include <certikos.h>
#include <certikos/profile.h>
#include <string.h>
#include <limits.h>
#include <ringleader.h>
#include "certikos_impl.h"

int musl_ringleader_unlinkat(int fd, const char *path, int flag)
{
	struct ringleader *rl = get_ringleader();

	struct ringleader_arena * arena = musl_ringleader_get_arena(rl, PATH_MAX);
	char * shmem = ringleader_arena_push(arena, PATH_MAX);
	strcpy(shmem, path);

	int id = ringleader_prep_unlinkat(rl, fd, shmem, flag);
	void * cookie = musl_ringleader_set_cookie(rl, id);

	ringleader_submit(rl);
	int ret = musl_ringleader_wait_result(rl, cookie);

	ringleader_free_arena(rl, arena);
	return ret;
}
#endif


int unlinkat(int fd, const char *path, int flag)
{
	return syscall(SYS_unlinkat, fd, path, flag);
}
