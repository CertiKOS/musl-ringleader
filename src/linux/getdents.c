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
	ringleader_set_user_data(rl, id, (void *) GETDENTS_COOKIE);
	ringleader_submit(rl);

	struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
	int ret;

	if((uint64_t) cqe->user_data == GETDENTS_COOKIE)
	{
		ret =  cqe->res;
		memcpy(buf, shmem, len);
	}
	else
	{
		ret = -EIO;
		certikos_puts("Unxpected ringleader getdents cookie\n");
	}

	ringleader_free_arena(rl, arena);
	ringleader_consume_cqe(rl, cqe);
	return ret;
}
#endif

int getdents(int fd, struct dirent *buf, size_t len)
{
	if (len>INT_MAX) len = INT_MAX;
	return syscall(SYS_getdents, fd, buf, len);
}
