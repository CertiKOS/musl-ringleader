#define _BSD_SOURCE
#include <dirent.h>
#include <limits.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <ringleader.h>
#include <string.h>

/* override in __syscall3 */
int musl_ringleader_getdents(int fd, struct dirent *buf, unsigned int len)
{
	struct ringleader *rl = get_ringleader();
	struct ringleader_arena * arena = musl_ringleader_get_arena(rl, len);
	if(arena == NULL) return -ENOMEM;

	void *shmem  = ringleader_arena_apush(arena, buf, len);


	uint32_t id = ringleader_prep_getdents(rl, fd, shmem, len);
	void *cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);

	int ret = musl_ringleader_wait_result(rl, cookie);

	memcpy(buf, shmem, len);
	ringleader_free_arena(rl, arena);
	return ret;
}
#endif

int getdents(int fd, struct dirent *buf, size_t len)
{
	if (len>INT_MAX) len = INT_MAX;
	return syscall(SYS_getdents, fd, buf, len);
}
