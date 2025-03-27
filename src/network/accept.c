#include <sys/socket.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#include <string.h>
#endif

int accept(int fd, struct sockaddr *restrict addr, socklen_t *restrict len)
{
#ifndef _CERTIKOS_
	return socketcall_cp(accept, fd, addr, len, 0, 0, 0);
#else
    struct ringleader *rl    = get_ringleader();
	struct ringleader_arena * arena = musl_ringleader_get_arena(rl,
			sizeof(socklen_t) + sizeof(struct sockaddr));
	socklen_t *socklen_ptr = ringleader_arena_push(arena, sizeof(socklen_t));
	struct sockaddr *sockaddr_ptr = ringleader_arena_push(arena, sizeof(struct sockaddr));
	*socklen_ptr = *len;
	int id = ringleader_prep_accept(rl, fd, sockaddr_ptr, socklen_ptr, 0);
	void *cookie = musl_ringleader_set_cookie(rl, id);

	ringleader_submit(rl);

	int ret = musl_ringleader_wait_result(rl, cookie);
	if(ret >= 0)
	{
		memcpy(addr, sockaddr_ptr, *socklen_ptr);
		*len = *socklen_ptr;
	}
	ringleader_free_arena(rl, arena);
	return __syscall_ret(ret);
#endif
}
