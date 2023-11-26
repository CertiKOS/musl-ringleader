#include "stdio_impl.h"
#include <sys/uio.h>
#include <string.h>

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#define READV_COOKIE (3142)
#define READ_COOKIE (3143)
#endif

size_t __stdio_read(FILE *f, unsigned char *buf, size_t len)
{
	#ifndef _CERTIKOS_
	struct iovec iov[2] = {
		{ .iov_base = buf, .iov_len = len - !!f->buf_size },
		{ .iov_base = f->buf, .iov_len = f->buf_size }
	};
	#else
	if(stdio_ensure_shmem_buf(f) < 0) return 0;
	//TODO should be buff size
	struct ringleader* rl = get_ringleader();
	void *shmem = get_rl_shmem_singleton();
	struct iovec *iov = (struct iovec *) shmem;
 
	iov[0] = (struct iovec) {.iov_base = iov + 2, .iov_len = len - !!f->buf_size};
	if(ringleader_addr_in_shmem(rl, f->buf)){
		iov[1] = (struct iovec) {.iov_base = f->buf, .iov_len = f->buf_size};
	} else {
		iov[1] = (struct iovec) {.iov_base = (char *)(iov + 2) + (len - !!f->buf_size), .iov_len = f->buf_size};
	}
	#endif

	ssize_t cnt;

	#ifndef _CERTIKOS_
	cnt = iov[0].iov_len ? syscall(SYS_readv, f->fd, iov, 2)
		: syscall(SYS_read, f->fd, iov[1].iov_base, iov[1].iov_len);
	#else 
	if(iov[0].iov_len){
		int32_t id = ringleader_prep_readv(rl, f->fd, iov, 2, -1);
		ringleader_set_user_data(rl, id, (void *) READV_COOKIE);		
	} else {
		//todo make sure there is sufficient size
		int32_t id = ringleader_prep_read(rl, f->fd, iov[1].iov_base, iov[1].iov_len, -1);
        ringleader_set_user_data(rl, id, (void*)READ_COOKIE);
    }
    ringleader_submit(rl);
    syscall(SYS_sched_yield);

    struct io_uring_cqe* cqe = ringleader_get_cqe(rl);
    if ((uint64_t)cqe->user_data == READV_COOKIE)
    {	
        cnt = cqe->res;
        ringleader_consume_cqe(rl, cqe);
        memcpy(buf, iov + 2, len - !!f->buf_size);
		if(!ringleader_addr_in_shmem(rl, f->buf)){
			int amnt = cnt - (len - !!f->buf_size);
			memcpy(f->buf, (char *)(iov + 2) + (len - !!f->buf_size), amnt > 0 ? amnt : 0);
		}
    } else if ((uint64_t) cqe->user_data == READ_COOKIE){
		cnt = cqe->res;
		ringleader_consume_cqe(rl, cqe);
		if(!ringleader_addr_in_shmem(rl, f->buf)){
			memcpy(f->buf, (char *)(iov + 2) + (len - !!f->buf_size), cnt);
		}
    } else {
        ringleader_consume_cqe(rl, cqe);
        certikos_puts("Did not get expected ringleader read completion token");
        cnt = 0;
    }
	#endif
	if (cnt <= 0) {
		f->flags |= cnt ? F_ERR : F_EOF;
		return 0;
	}
	if (cnt <= iov[0].iov_len) return cnt;
	cnt -= iov[0].iov_len;
	f->rpos = f->buf;
	f->rend = f->buf + cnt;
	if (f->buf_size) buf[len-1] = *f->rpos++;
	return len;
}
