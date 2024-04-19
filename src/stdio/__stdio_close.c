#include "stdio_impl.h"
#include "aio_impl.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#endif

static int dummy(int fd)
{
	return fd;
}

weak_alias(dummy, __aio_close);

int __stdio_close(FILE *f)
{
#ifndef _CERTIKOS_
	return syscall(SYS_close, __aio_close(f->fd));
#else

    struct ringleader* rl = get_ringleader();
    int32_t id = ringleader_prep_close(rl, f->fd);
    ringleader_set_user_data(rl, id, (void *)CLOSE_COOKIE);

    ringleader_submit(rl);

    struct io_uring_cqe* cqe = ringleader_get_cqe(rl);
    int ret;
    if((uint64_t) cqe->user_data == CLOSE_COOKIE){
        ret = cqe->res;
        ringleader_consume_cqe(rl, cqe);
        return ret;
    } else {
        ringleader_consume_cqe(rl, cqe);
        certikos_puts("Did not get expected ringleader close completion token");
        ret = -1;
    }
    return ret;
#endif

}
