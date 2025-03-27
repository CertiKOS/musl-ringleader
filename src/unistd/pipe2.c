#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "syscall.h"


#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <ringleader.h>
#include <certikos.h>
#include <string.h>

int musl_ringleader_pipe2(int fd[2], int flags)
{
	struct ringleader *rl = get_ringleader();

	struct ringleader_arena *arena =
		musl_ringleader_get_arena(rl, 2*sizeof(int));
	if(!arena)
		return -ENOMEM;

	void *shmem = ringleader_arena_push(arena, 2*sizeof(int));
	if(!shmem)
		return -ENOMEM;

	int id = ringleader_prep_pipe2(rl, shmem, flags);
	void * cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);
	int ret = musl_ringleader_wait_result(rl, cookie);

	ringleader_arena_apop(arena, shmem, fd, 2*sizeof(int));
	ringleader_free_arena(rl, arena);
	return ret;
}
#endif


int pipe2(int fd[2], int flag)
{
	if (!flag) return pipe(fd);
	int ret = __syscall(SYS_pipe2, fd, flag);
	if (ret != -ENOSYS) return __syscall_ret(ret);
	if (flag & ~(O_CLOEXEC|O_NONBLOCK)) return __syscall_ret(-EINVAL);
	ret = pipe(fd);
	if (ret) return ret;
	if (flag & O_CLOEXEC) {
		__syscall(SYS_fcntl, fd[0], F_SETFD, FD_CLOEXEC);
		__syscall(SYS_fcntl, fd[1], F_SETFD, FD_CLOEXEC);
	}
	if (flag & O_NONBLOCK) {
		__syscall(SYS_fcntl, fd[0], F_SETFL, O_NONBLOCK);
		__syscall(SYS_fcntl, fd[1], F_SETFL, O_NONBLOCK);
	}
	return 0;
}
