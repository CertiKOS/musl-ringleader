#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <ringleader.h>
#include <string.h>

int musl_ringleader_lseek(int fd, off_t offset, int whence)
{
    int ret;
    struct ringleader *rl = get_ringleader();

    int32_t id = ringleader_prep_lseek(rl, fd, offset, whence);
    ringleader_set_user_data(rl, id, (void*)LSEEK_COOKIE);
    ringleader_submit(rl);

    struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
    if((uint64_t) cqe->user_data != LSEEK_COOKIE)
    {
        ringleader_consume_cqe(rl, cqe);
        certikos_puts("lseek: unxpected ringleader cookie.\n");
        return -EINVAL;
    }

    ret = cqe->res;
    ringleader_consume_cqe(rl, cqe);
    return ret;
}
#endif

off_t __lseek(int fd, off_t offset, int whence)
{
#ifdef SYS__llseek
	off_t result;
	return syscall(SYS__llseek, fd, offset>>32, offset, &result, whence) ? -1 : result;
#else
	return syscall(SYS_lseek, fd, offset, whence);
#endif
}

weak_alias(__lseek, lseek);
