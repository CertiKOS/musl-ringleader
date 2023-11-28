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
    int ret;
    int32_t id;
    struct ringleader *rl = get_ringleader();
    void * shmem = get_rl_shmem_singleton();

    strncpy(shmem, filename, SHMEM_SIZE - 0x100);
    id = ringleader_prep_mkdirat(rl, fd, shmem, mode);
    ringleader_set_user_data(rl, id, (void*)MKDIRAT_COOKIE);
    ringleader_submit(rl);

    struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
    if((uint64_t) cqe->user_data != MKDIRAT_COOKIE)
    {
        ringleader_consume_cqe(rl, cqe);
        certikos_puts("mkdirat: unxpected ringleader cookie.");
        return -EINVAL;
    }

    ret = cqe->res;
    ringleader_consume_cqe(rl, cqe);

    return ret;
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
