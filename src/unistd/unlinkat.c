#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include <certikos.h>
#include <certikos/profile.h>
#include <string.h>
#include "certikos_impl.h"
#include "ringleader.h"

int musl_ringleader_unlinkat(int fd, const char *path, int flag)
{
	struct ringleader *rl = get_ringleader();

	void *shmem = get_rl_shmem_singleton();
	strcpy(shmem, path);

	int id = ringleader_prep_unlinkat(rl, fd, shmem, flag);
	void * cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);
	return musl_ringleader_wait_result(rl, cookie);
}
#endif


int unlinkat(int fd, const char *path, int flag)
{
	return syscall(SYS_unlinkat, fd, path, flag);
}
