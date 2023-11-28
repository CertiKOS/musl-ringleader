#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "ringleader.h"
#include "certikos_impl.h"
#endif

#ifdef _CERTIKOS_
int ringleader_socket_helper(int domain, int type, int protocol){
    struct ringleader* rl = get_ringleader();
    int index             = ringleader_prep_socket(rl, domain, type, protocol);
    ringleader_set_user_data(rl, index, (void *) SOCKET_COOKIE);
    ringleader_submit(rl);

    syscall(SYS_sched_yield);

    struct io_uring_cqe* cqe = ringleader_get_cqe(rl);
    if (cqe->user_data == SOCKET_COOKIE)
    {
		int socket = cqe->res;
		ringleader_consume_cqe(rl, cqe);
		return socket;
    }
    else
    {
        ringleader_consume_cqe(rl, cqe);
        certikos_puts("Did not get expected ringleader socket creation "
                      "completion token");
        return __syscall_ret(-EINVAL);
    }
}
#endif

int socket(int domain, int type, int protocol)
{
	#ifndef _CERTIKOS_
	int s = __socketcall(socket, domain, type, protocol, 0, 0, 0);
	if ((s==-EINVAL || s==-EPROTONOSUPPORT)
	    && (type&(SOCK_CLOEXEC|SOCK_NONBLOCK))) {
		s = __socketcall(socket, domain,
			type & ~(SOCK_CLOEXEC|SOCK_NONBLOCK),
			protocol, 0, 0, 0);
		if (s < 0) return __syscall_ret(s);
		if (type & SOCK_CLOEXEC)
			__syscall(SYS_fcntl, s, F_SETFD, FD_CLOEXEC);
		if (type & SOCK_NONBLOCK)
			__syscall(SYS_fcntl, s, F_SETFL, O_NONBLOCK);
	}
	return __syscall_ret(s);
	#else

	//repeat the logic from above of retrying without CLOEXEC and NONBLOCK, however we do not then set those up with fcntl because those are not present
	//TODO
	int s = ringleader_socket_helper(domain, type, protocol);
	if((s == -EINVAL || s == -EPROTONOSUPPORT) && type & (SOCK_CLOEXEC | SOCK_NONBLOCK)){
        s = ringleader_socket_helper(domain,
                                     type & ~(SOCK_CLOEXEC | SOCK_NONBLOCK), protocol);
    }
	return __syscall_ret(s);

	#endif
}
