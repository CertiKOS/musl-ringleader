#include <sys/stat.h>
#include <fcntl.h>
#include "syscall.h"


#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <ringleader.h>
#include <certikos.h>
#include <string.h>

int musl_ringleader_mkdirat(int fd, const char *filename, mode_t mode)
{
	void * shmem = get_rl_shmem_singleton();
	strncpy(shmem, filename, SHMEM_SIZE - 0x100);

	struct ringleader *rl = get_ringleader();
	int32_t id = ringleader_prep_mkdirat(rl, fd, shmem, mode);
	void * cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);
	return musl_ringleader_wait_result(rl, cookie);
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
