#include <fcntl.h>
#include <stdarg.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <ringleader.h>
#include <certikos.h>
#include <certikos/profile.h>
#include <string.h>

int musl_ringleader_openat(int fd, const char *filename, int flags, mode_t mode)
{
    int ret;
    int32_t id;
    struct ringleader *rl = get_ringleader();
    void * shmem = get_rl_shmem_singleton();


    strncpy(shmem, filename, SHMEM_SIZE - 0x100);
    id = ringleader_prep_openat(rl, fd, shmem, flags, mode);
    ringleader_set_user_data(rl, id, (void*)OPENAT_COOKIE);

#ifdef MUSL_RINGLEADER_PROFILE
	overhead_declare();
	overheads_start(&musl_overheads, track_ringleader_musl_openat);
#endif

    ringleader_submit(rl);

    struct io_uring_cqe *cqe = ringleader_get_cqe(rl);

#ifdef MUSL_RINGLEADER_PROFILE
	overheads_end(&musl_overheads, track_ringleader_musl_openat);
#endif

    if((uint64_t) cqe->user_data != OPENAT_COOKIE)
    {
        ringleader_consume_cqe(rl, cqe);
        certikos_puts("openat: unxpected ringleader cookie.");
        return -EINVAL;
    }

    ret = cqe->res;
    ringleader_consume_cqe(rl, cqe);

    return ret;
}
#endif

int openat(int fd, const char *filename, int flags, ...)
{
	mode_t mode = 0;

	if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}

	return syscall_cp(SYS_openat, fd, filename, flags|O_LARGEFILE, mode);
}
