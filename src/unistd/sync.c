#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"

void musl_ringleader_sync()
{
    struct ringleader *rl = get_ringleader();

    uint32_t id = ringleader_prep_sync(rl);
    ringleader_set_user_data(rl, id, (void*)SYNC_COOKIE);
    ringleader_submit(rl);

    struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
    while((uint64_t) cqe->user_data != SYNC_COOKIE)
    {
        ringleader_consume_cqe(rl, cqe);
        certikos_puts("Did not get expected getppid completion token");

        cqe = ringleader_get_cqe(rl);
    }

    ringleader_consume_cqe(rl, cqe);
}

#endif

void sync(void)
{
	__syscall(SYS_sync);
}
