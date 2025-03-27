#include <sys/socket.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include <string.h>
#include "certikos_impl.h"
#include "ringleader.h"
#endif

int connect(int fd, const struct sockaddr *addr, socklen_t len)
{
	#ifndef _CERTIKOS_
	return socketcall_cp(connect, fd, addr, len, 0, 0, 0);
	#else
    struct ringleader* rl = get_ringleader();
	struct ringleader_arena * arena = musl_ringleader_get_arena(rl, len);
	void * shmem = ringleader_arena_apush(arena, addr, len);

	int id = ringleader_prep_connect(rl, fd, shmem, len);
	void * cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);

	int ret = musl_ringleader_wait_result(rl, cookie);
	ringleader_free_arena(rl, arena);
	return __syscall_ret(ret);
	#endif
}
