#include <sys/socket.h>

#ifdef _CERTIKOS_
#include <string.h>
#include "certikos_impl.h"
#include "ringleader.h"
#endif

ssize_t send(int fd, const void *buf, size_t len, int flags)
{
	#ifndef _CERTIKOS_
	return sendto(fd, buf, len, flags, 0, 0);
	#else
	struct ringleader *rl = get_ringleader();
	struct ringleader_arena *arena = musl_ringleader_get_arena(rl, len);
	void *shmem = ringleader_arena_apush(arena, buf, len);

	int id = ringleader_prep_send(rl, fd, shmem, len, flags);
	void *cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);

	ssize_t ret = musl_ringleader_wait_result(rl, cookie);
	ringleader_free_arena(rl, arena);
	return __syscall_ret(ret);
	#endif
}
