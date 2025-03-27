#include <fcntl.h>
#include <stdarg.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <ringleader.h>
#include <certikos.h>
#include <certikos/profile.h>
#include <string.h>
#include <limits.h>

int musl_ringleader_openat(int fd, const char *filename, int flags, mode_t mode)
{
    int ret;
    int32_t open_sqe_id;//, stat_sqe_id;
    struct ringleader *rl = get_ringleader();


//    /* decode mode/flags & stat */
//    int regular_file = 0;
//    int do_stat = 1;
//
//    if(flags & O_TMPFILE)
//    {
//        regular_file = 1;
//        do_stat = 0;
//    }
//    else if((flags & O_DIRECTORY) || (flags & O_DSYNC) || (flags & O_PATH))
//    {
//        regular_file = 0;
//        do_stat = 0;
//    }
//    else if((flags & O_CREAT) || (flags & O_TRUNC))
//    {
//        /* TODO: can we assume regular file here? */
//        regular_file = 1;
//        do_stat = 0;
//    }
//    size_t arena_size = PATH_MAX + (do_stat ? sizeof(struct statx) : 0);
    size_t arena_size = PATH_MAX;


    struct ringleader_arena * arena = musl_ringleader_get_arena(rl, arena_size);
    void * shmem = ringleader_arena_push(arena, PATH_MAX);
    strncpy(shmem, filename, PATH_MAX);

    open_sqe_id = ringleader_prep_openat(rl, fd, shmem, flags, mode);
    void * cookie = musl_ringleader_set_cookie(rl, open_sqe_id);
    ringleader_sqe_set_flags(rl, open_sqe_id, IOSQE_IO_LINK);


#ifdef MUSL_RINGLEADER_PROFILE
	overhead_declare();
	overheads_start(&musl_overheads, track_ringleader_musl_openat);
#endif

    ringleader_submit(rl);

    ret = musl_ringleader_wait_result(rl, cookie);

#ifdef MUSL_RINGLEADER_PROFILE
	overheads_end(&musl_overheads, track_ringleader_musl_openat);
#endif

    ringleader_free_arena(rl, arena);

    return __syscall_ret(ret);
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
