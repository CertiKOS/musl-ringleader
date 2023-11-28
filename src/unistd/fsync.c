#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <ringleader.h>
#include <certikos.h>
#include <string.h>

int musl_ringleader_fsync(int fd)
{
    int ret;
    int32_t id;
    struct ringleader *rl = get_ringleader();

    id = ringleader_prep_fsync(rl, fd);
    ringleader_set_user_data(rl, id, (void*)FSYNC_COOKIE);
    ringleader_submit(rl);

    struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
    if((uint64_t) cqe->user_data != FSYNC_COOKIE)
    {
        ringleader_consume_cqe(rl, cqe);
        certikos_puts("fsync: unxpected ringleader cookie.");
        return -EINVAL;
    }

    ret = cqe->res;
    ringleader_consume_cqe(rl, cqe);
    return ret;
}
#endif




int fsync(int fd)
{
	return syscall_cp(SYS_fsync, fd);
}
