#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"

pid_t musl_ringleader_getppid()
{

    pid_t ret;
    struct ringleader *rl = get_ringleader();

    uint32_t id = ringleader_prep_getppid(rl);

    ringleader_set_user_data(rl, id, (void*)GETPPID_COOKIE);

#ifdef MUSL_RINGLEADER_PROFILE
	overhead_declare();
	overheads_start(&musl_overheads, track_ringleader_musl_getppid);
#endif

    ringleader_submit(rl);

    struct io_uring_cqe *cqe = ringleader_get_cqe(rl);

#ifdef MUSL_RINGLEADER_PROFILE
	overheads_end(&musl_overheads, track_ringleader_musl_getppid);
#endif

    while((uint64_t) cqe->user_data != GETPPID_COOKIE)
    {
        ringleader_consume_cqe(rl, cqe);
        certikos_puts("Did not get expected getppid completion token");

        cqe = ringleader_get_cqe(rl);
    }

    ret = cqe->res;
    ringleader_consume_cqe(rl, cqe);

    return ret;
}

#endif


pid_t getppid(void)
{
	return __syscall(SYS_getppid);
}
