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

    /* wait for outstanding write to complete */
    struct ringleader *rl = get_ringleader();
    while(f->rl.in_flight_arenas[0] || f->rl.in_flight_arenas[1])
    {
        struct io_uring_cqe * cqe = ringleader_peek_cqe(rl);
        (void)cqe;
    }

    int32_t id = ringleader_prep_close(rl, f->fd);
    ringleader_set_user_data(rl, id, (void *)CLOSE_COOKIE);

    ringleader_submit(rl);

    struct io_uring_cqe* cqe = ringleader_get_cqe(rl);
    int ret;
    if((uint64_t) cqe->user_data == CLOSE_COOKIE){
        ret = cqe->res;
        ringleader_consume_cqe(rl, cqe);
    } else {
        ringleader_consume_cqe(rl, cqe);
        certikos_puts("Did not get expected ringleader close completion token");
        ret = -1;
    }

    /* finish in-flight reads */
    rl_stdio_wait_pending_read(f);


    if(f->rl.buf_arena)
    {
        ringleader_free_arena(rl, f->rl.buf_arena);
        f->rl.buf_arena = NULL;
        f->buf = NULL;
    }
    return ret;
#endif

}
