#include "stdio_impl.h"
#include "certikos_impl.h"
#include "ringleader.h"
#include <sys/uio.h>
#include <string.h>

#define WRITE_COOKIE (2020859)

size_t __stdio_write(FILE *f, const unsigned char *buf, size_t len)
{
	#ifndef _CERTIKOS_
		struct iovec iovs[2] = {
		{ .iov_base = f->wbase, .iov_len = f->wpos-f->wbase },
		{ .iov_base = (void *)buf, .iov_len = len }
	};
	#else 
		struct ringleader* rl = get_ringleader();
		void* shmem = get_rl_shmem();
		struct iovec *iovs = (struct iovec *) shmem;

		size_t block_two_offset = len;

		iovs[0] = (struct iovec) {.iov_base = &iovs[2] + block_two_offset, .iov_len = f->wpos-f->wbase};
		iovs[1] = (struct iovec) {.iov_base = &iovs[2], len};

		//copy past where the iovecs live
		memcpy(&iovs[2], (void *) buf, len);
		memcpy(&iovs[2] + block_two_offset, f->wbase, f->wpos-f->wbase);
	#endif
	
	struct iovec *iov = iovs;
	size_t rem = iov[0].iov_len + iov[1].iov_len;
	int iovcnt = 2;
	ssize_t cnt;

	for (;;) {

		#ifndef _CERTIKOS_
			cnt = syscall(SYS_writev, f->fd, iov, iovcnt);
		#else
			int32_t id = ringleader_prep_writev(rl, f->fd, iov, iovcnt, 0, 1);
			ringleader_set_user_data(rl, id, (void *) WRITE_COOKIE);
			ringleader_submit(rl);
			syscall(SYS_sched_yield);

			struct io_uring_cqe *cqe;
			while(1) {
			cqe = ringleader_peek_cqe(rl);
			if(cqe != NULL){
				if((uint64_t) cqe->user_data == WRITE_COOKIE){
					__s32 ret = cqe->res;
					ringleader_consume_cqe(rl, cqe);
					cnt = ret;
					break;
				} else {
					certikos_puts("Did not get expected ringleader write completion token");
					cnt = 0;
					break;
				}
			}}
		#endif
		
		if (cnt == rem) {
			f->wend = f->buf + f->buf_size;
			f->wpos = f->wbase = f->buf;
			return len;
		}
		if (cnt < 0) {
			f->wpos = f->wbase = f->wend = 0;
			f->flags |= F_ERR;
			return iovcnt == 2 ? 0 : len-iov[0].iov_len;
		}
		rem -= cnt;
		if (cnt > iov[0].iov_len) {
			cnt -= iov[0].iov_len;
			iov++; iovcnt--;
		}

		iov[0].iov_base = (char *)iov[0].iov_base + cnt;
		iov[0].iov_len -= cnt;
	}
}
