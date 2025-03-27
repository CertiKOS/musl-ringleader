#include <sys/stat.h>
#include <fcntl.h>
#include "syscall.h"


#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <ringleader.h>
#include <certikos.h>
#include <string.h>
#include <limits.h>

int musl_ringleader_mkdirat(int fd, const char *filename, mode_t mode)
{
	struct ringleader *rl = get_ringleader();
	struct ringleader_arena *arena = musl_ringleader_get_arena(rl, PATH_MAX);
	char *shmem = ringleader_arena_push(arena, PATH_MAX);
	strncpy(shmem, filename, PATH_MAX);

	int32_t id = ringleader_prep_mkdirat(rl, fd, shmem, mode);
	void * cookie = musl_ringleader_set_cookie(rl, id);

	ringleader_submit(rl);
	int ret = musl_ringleader_wait_result(rl, cookie);

	ringleader_free_arena(rl, arena);
	return ret;
}
#endif


int mkdir(const char *path, mode_t mode)
{
#ifdef SYS_mkdir
	return syscall(SYS_mkdirat, path, mode);
#else
	return syscall(SYS_mkdirat, AT_FDCWD, path, mode);
#endif
}
