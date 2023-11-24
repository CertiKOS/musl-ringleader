#define _GNU_SOURCE
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#include <string.h>

#define FCNTL_COOKIE (1644911)
#endif

//TODO there are probably a lot of bugs in this file that need fixing for all the edge casing

int fcntl(int fd, int cmd, ...)
{
	unsigned long arg;
	va_list ap;
	va_start(ap, cmd);
	arg = va_arg(ap, unsigned long);
	va_end(ap);
	struct ringleader *rl;
	uint64_t cookie;
	if (cmd == F_SETFL) arg |= O_LARGEFILE;
	if (cmd == F_SETLKW){
		#ifndef _CERTIKOS_
			return syscall_cp(SYS_fcntl, fd, cmd, (void *)arg);
		#else 
			cookie = FCNTL_COOKIE;
			prep_musl_ringleader_fcntl(0, cookie, fd, cmd, arg);
			rl = get_ringleader();
			ringleader_submit(rl);
			syscall(SYS_sched_yield);
			return __syscall_ret(reap_musl_ringleader_fcntl(1, &cookie));
		#endif
	} 
	if (cmd == F_GETOWN) {
		#ifndef _CERTIKOS_
		struct f_owner_ex ex;
		int ret = __syscall(SYS_fcntl, fd, F_GETOWN_EX, &ex);
		if (ret == -EINVAL) return __syscall(SYS_fcntl, fd, cmd, (void *)arg);
		if (ret) return __syscall_ret(ret);
		return ex.type == F_OWNER_PGRP ? -ex.pid : ex.pid;
		#else
		rl = get_ringleader();
		void *shmem = get_rl_shmem_singleton();
		struct f_owner_ex* ex = shmem;
		int32_t id = ringleader_prep_fcntl(rl, fd, F_GETOWN_EX, (uint64_t) ex);
		ringleader_set_user_data(rl, id, (void *)FCNTL_COOKIE);

		ringleader_submit(rl);
		syscall(SYS_sched_yield);

		struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
		int ret;
		if((uint64_t) cqe->user_data == FCNTL_COOKIE){
			ret = cqe->res;
            ringleader_consume_cqe(rl, cqe);
		} else {
            ringleader_consume_cqe(rl, cqe);
            certikos_puts("Did not get expected ringleader fcntl completion token");
            return -EINVAL;
        }

		if (ret == -EINVAL){
			uint64_t cookie = FCNTL_COOKIE;
			prep_musl_ringleader_fcntl(0, cookie, fd, cmd, arg);
			ringleader_submit(rl);
			syscall(SYS_sched_yield);
			return reap_musl_ringleader_fcntl(1, &cookie);
		}

		if(ret){
			return __syscall_ret(ret);
		}  

		return ex->type == F_OWNER_PGRP ? -ex->pid : ex->pid;
		#endif
	}
	if (cmd == F_DUPFD_CLOEXEC) {
		#ifndef _CERTIKOS_
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
		#else

		cookie = FCNTL_COOKIE;
		prep_musl_ringleader_fcntl(0, cookie, fd, F_DUPFD_CLOEXEC, arg);
		rl = get_ringleader();
		ringleader_submit(rl);
		syscall(SYS_sched_yield);
		int ret = reap_musl_ringleader_fcntl(1, &cookie);

		if (ret != -EINVAL){
			if (ret >= 0){
				prep_musl_ringleader_fcntl(0, ++cookie, ret, F_SETFD, FD_CLOEXEC);
				ringleader_submit(rl);
				syscall(SYS_sched_yield);
				reap_musl_ringleader_fcntl(1, &cookie);
			}
			return __syscall_ret(ret);
		}

		prep_musl_ringleader_fcntl(0, ++cookie, fd, F_DUPFD_CLOEXEC, 0);
		ringleader_submit(rl);
		syscall(SYS_sched_yield);
		ret = reap_musl_ringleader_fcntl(1, &cookie);

		if(ret != -EINVAL){
			if (ret >= 0){
				//TODO add a close here
				// close(ret);
			}
			return __syscall_ret(-EINVAL);
		}

		prep_musl_ringleader_fcntl(0, ++cookie, fd, F_DUPFD, arg);
		ringleader_submit(rl);
		syscall(SYS_sched_yield);
		ret = reap_musl_ringleader_fcntl(1, &cookie);

		if(ret >= 0){
			prep_musl_ringleader_fcntl(0, ++cookie, ret, F_SETFD, FD_CLOEXEC);
			ringleader_submit(rl);
			syscall(SYS_sched_yield);
			reap_musl_ringleader_fcntl(1, &cookie);
		}

		return __syscall_ret(ret);
		#endif
	}
	switch (cmd) {
	case F_SETLK:
	case F_GETLK:
	case F_GETOWN_EX:
	case F_SETOWN_EX:
		#ifndef _CERTIKOS_
		return syscall(SYS_fcntl, fd, cmd, (void *)arg);
		#else
		cookie = FCNTL_COOKIE;
		prep_musl_ringleader_fcntl(0, cookie, fd, cmd, arg);
		rl = get_ringleader();
		ringleader_submit(rl);
		syscall(SYS_sched_yield);
		return reap_musl_ringleader_fcntl(1, &cookie);
		#endif
	default:
		#ifndef _CERTIKOS_
		return syscall(SYS_fcntl, fd, cmd, arg);
		#else
		cookie = FCNTL_COOKIE;
		prep_musl_ringleader_fcntl(0, cookie, fd, cmd, arg);
		rl = get_ringleader();
		ringleader_submit(rl);
		syscall(SYS_sched_yield);
		return reap_musl_ringleader_fcntl(1, &cookie);
		#endif
	}
}
