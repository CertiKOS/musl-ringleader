#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"

pid_t musl_ringleader_getpid()
{
    pid_t ret;
    struct ringleader *rl = get_ringleader();

    uint32_t id = ringleader_prep_getpid(rl);

    ringleader_set_user_data(rl, id, (void*)GETPID_COOKIE);
    ringleader_submit(rl);

    struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
    while((uint64_t) cqe->user_data != GETPID_COOKIE)
    {
        ringleader_consume_cqe(rl, cqe);
        certikos_puts("Did not get expected getpid completion token");

        cqe = ringleader_get_cqe(rl);
    }

    ret = cqe->res;
    ringleader_consume_cqe(rl, cqe);

    return ret;
}

#endif

pid_t getpid(void)
{
	return __syscall(SYS_getpid);
}
