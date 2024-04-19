#define _GNU_SOURCE
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#include <string.h>

int musl_ringleader_fcntl(int fd, int cmd, long arg)
{
    int ret;
    struct ringleader *rl = get_ringleader();
    void * shmem;
    size_t cp_in_sz = 0;
    size_t cp_out_sz = 0;
    int32_t id;

    switch(cmd)
    {
        case F_SETLK:
        case F_SETLKW:      cp_in_sz  = sizeof(struct flock); break;
        case F_GETLK:       cp_out_sz = sizeof(struct flock); break;
        case F_SETOWN_EX:   cp_in_sz  = sizeof(struct f_owner_ex); break;
        case F_GETOWN_EX:   cp_out_sz = sizeof(struct f_owner_ex); break;

        /* no shmem copy */
        case F_DUPFD:
        case F_GETFD:
        case F_SETFD:
        case F_GETFL:
        case F_SETFL:
        case F_SETOWN:
        case F_GETOWN:
        case F_SETSIG:
        case F_GETSIG:
        case F_SETLEASE:
        case F_GETLEASE:
        case F_NOTIFY:
        case F_SETPIPE_SZ:
        case F_GETPIPE_SZ:
        case F_ADD_SEALS:
        case F_GET_SEALS:
        default:
            break;
    }

    struct ringleader_arena * arena = NULL;

    if(cp_in_sz || cp_out_sz)
    {
        size_t count = cp_in_sz > cp_out_sz ? cp_in_sz : cp_out_sz;

        arena = musl_ringleader_get_arena(rl, count);
        void *shmem = ringleader_arena_push(arena, count);
        if(shmem == NULL)
        {
            return __syscall_ret(-ENOMEM);
        }

        if(cp_in_sz)
        {
            memcpy(shmem, (void*)arg, cp_in_sz);
        }

        id = ringleader_prep_fcntl(rl, fd, cmd,
                ringleader_calc_proxy_addr(rl, shmem));
    }
    else
    {
        id = ringleader_prep_fcntl(rl, fd, cmd, arg);
    }

    ringleader_set_user_data(rl, id, (void*)FCNTL_COOKIE);
    ringleader_submit(rl);

    struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
    if((uint64_t) cqe->user_data != FCNTL_COOKIE)
    {
        ringleader_consume_cqe(rl, cqe);
        ringleader_free_arena(rl, arena);
        certikos_puts("Did not get expected ringleader fcntl completion token");
        return -EINVAL;
    }

    ret = cqe->res;
    ringleader_consume_cqe(rl, cqe);

    if(cp_out_sz)
    {
        memcpy((void*)arg, shmem, cp_out_sz);
    }

    ringleader_free_arena(rl, arena);
    return ret;
}

#endif


int fcntl(int fd, int cmd, ...)
{
	unsigned long arg;
	va_list ap;
	va_start(ap, cmd);
	arg = va_arg(ap, unsigned long);
	va_end(ap);
	if (cmd == F_SETFL) arg |= O_LARGEFILE;
	if (cmd == F_SETLKW){
			return syscall_cp(SYS_fcntl, fd, cmd, (void *)arg);
	}
	if (cmd == F_GETOWN) {
		struct f_owner_ex ex;
		int ret = __syscall(SYS_fcntl, fd, F_GETOWN_EX, &ex);
		if (ret == -EINVAL) return __syscall(SYS_fcntl, fd, cmd, (void *)arg);
		if (ret) return __syscall_ret(ret);
		return ex.type == F_OWNER_PGRP ? -ex.pid : ex.pid;
	}
	if (cmd == F_DUPFD_CLOEXEC) {
		int ret = __syscall(SYS_fcntl, fd, F_DUPFD_CLOEXEC, arg);
		if (ret != -EINVAL) {
			if (ret >= 0)
				__syscall(SYS_fcntl, ret, F_SETFD, FD_CLOEXEC);
			return __syscall_ret(ret);
		}
		ret = __syscall(SYS_fcntl, fd, F_DUPFD_CLOEXEC, 0);
		if (ret != -EINVAL) {
			if (ret >= 0) __syscall(SYS_close, ret);
			return __syscall_ret(-EINVAL);
		}
		ret = __syscall(SYS_fcntl, fd, F_DUPFD, arg);
		if (ret >= 0) __syscall(SYS_fcntl, ret, F_SETFD, FD_CLOEXEC);
		return __syscall_ret(ret);
	}
	switch (cmd) {
	case F_SETLK:
	case F_GETLK:
	case F_GETOWN_EX:
	case F_SETOWN_EX:
		return syscall(SYS_fcntl, fd, cmd, (void *)arg);
	default:
		return syscall(SYS_fcntl, fd, cmd, arg);
	}
}
