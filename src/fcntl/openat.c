#include <fcntl.h>
#include <stdarg.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <ringleader.h>
#include <certikos.h>
#include <certikos/profile.h>
#include <string.h>
#include <limits.h>


int
musl_ringleader_openat(
		int dirfd,
		const char *filename,
		int flags,
		mode_t mode)
{
	struct ringleader *rl = get_ringleader();
	size_t arena_size = PATH_MAX;

	struct ringleader_arena * arena = musl_ringleader_get_arena(rl,
			arena_size);
	void * shmem_filename = ringleader_arena_push(arena, PATH_MAX);
	strncpy(shmem_filename, filename, PATH_MAX);

	int open_sqe_id = ringleader_prep_openat(
			rl,
			dirfd,
			shmem_filename,
			flags,
			mode);

	void * cookie = musl_ringleader_set_cookie(rl, open_sqe_id);
	ringleader_submit(rl);


	int ret = musl_ringleader_wait_result(rl, cookie);


	ringleader_free_arena(rl, arena);

	return __syscall_ret(ret);
}
#endif

int openat(int fd, const char *filename, int flags, ...)
{
	mode_t mode = 0;

	if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}

	return syscall_cp(SYS_openat, fd, filename, flags|O_LARGEFILE, mode);
}
