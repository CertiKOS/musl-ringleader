#include "stdio_impl.h"
#include <sys/uio.h>
#include <string.h>

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <certikos.h>
#include <ringleader.h>

void
rl_stdio_wait_pending_read(FILE *f)
{
	struct ringleader *rl = get_ringleader();
	while(f && f->rl.pending_buf_read)
	{
		musl_ringleader_flush_cqes(rl);
	}
}

void
rl_stdio_read_done(
		struct ringleader *rl,
		ringleader_promise_t handle,
		void *data)
{
	struct io_uring_cqe * cqe =  (void*)data;
	FILE *f = (void*)cqe->user_data;
	int cnt = cqe->res;

	ringleader_promise_set_result(rl, handle, (void*)(uintptr_t)cnt);

	f->rl.pending_buf_read = 0;

	if (cnt <= 0)
	{
		f->flags |= cnt ? F_ERR : F_EOF;
		return;
	}

	f->rpos = f->buf;
	f->rend = f->buf + cnt;
}

#endif

size_t __stdio_read(FILE *f, unsigned char *buf, size_t len)
{
#ifndef _CERTIKOS_
	struct iovec iov[2] = {
		{ .iov_base = buf, .iov_len = len - !!f->buf_size },
		{ .iov_base = f->buf, .iov_len = f->buf_size }
	};

	ssize_t cnt;

	cnt = iov[0].iov_len ? syscall(SYS_readv, f->fd, iov, 2)
		: syscall(SYS_read, f->fd, iov[1].iov_base, iov[1].iov_len);
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
#else
    struct ringleader *rl = get_ringleader();

    if(musl_rl_async_fd_check(f->fd))
    {
        /* buffering is happening at lower level */
        return __syscall_ret(musl_rl_async_fd_read(rl, f->fd, buf, len));
    }

    ssize_t cnt = __syscall_ret(musl_ringleader_read(f->fd, buf, len));

    if (cnt <= 0) {
        f->flags |= cnt ? F_ERR : F_EOF;
        return 0;
    }

    if(!f->rl.buf_arena)
    {
        /* stdin first read */
        f->rl.buf_arena = musl_ringleader_get_arena(rl, BUFSIZ + UNGET);
        f->buf = ringleader_arena_push(f->rl.buf_arena, UNGET + BUFSIZ);
        f->buf += UNGET;
        f->buf_size = BUFSIZ;
    }

    uint32_t sqe = ringleader_prep_read(rl, f->fd, f->buf, f->buf_size, -1);

    ringleader_sqe_set_data(rl, sqe, f);
    ringleader_promise_t p = ringleader_sqe_then(rl, sqe, rl_stdio_read_done);
    ringleader_promise_free_on_fulfill(rl, p);

    ringleader_submit(rl);
    f->rl.pending_buf_read = 1;

    return cnt;

#endif
}
