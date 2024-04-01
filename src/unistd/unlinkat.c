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
	ringleader_set_user_data(rl, id, (void*) UNLINK_COOKIE);

#ifdef MUSL_RINGLEADER_PROFILE
	overhead_declare();
	overheads_start(&musl_overheads, track_ringleader_musl_unlinkat);
#endif

	ringleader_submit(rl);
	struct io_uring_cqe *cqe = ringleader_get_cqe(rl);

#ifdef MUSL_RINGLEADER_PROFILE
	overheads_end(&musl_overheads, track_ringleader_musl_unlinkat);
#endif

	if(cqe->user_data == UNLINK_COOKIE){
		int ret = cqe->res;
		ringleader_consume_cqe(rl, cqe);
		return ret;
	} else {
		ringleader_consume_cqe(rl, cqe);
		certikos_puts("Did not get expected ringleader unlinkat cookie");
		return -EINVAL;
	}
}

#endif


int unlinkat(int fd, const char *path, int flag)
{
	return syscall(SYS_unlinkat, fd, path, flag);
}
