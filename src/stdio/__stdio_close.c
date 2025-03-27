#include "stdio_impl.h"
#include "aio_impl.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#endif

static int dummy(int fd)
{
	return fd;
}

weak_alias(dummy, __aio_close);

int __stdio_close(FILE *f)
{
#ifndef _CERTIKOS_
	return syscall(SYS_close, __aio_close(f->fd));
#else

	/* wait for outstanding write to complete */
	struct ringleader *rl = get_ringleader();
	while(f->rl.in_flight_arenas[0] || f->rl.in_flight_arenas[1])
	{
		struct io_uring_cqe * cqe = ringleader_peek_cqe(rl);
		(void)cqe;
	}

	int32_t id = ringleader_prep_close(rl, f->fd);
	void *cookie = musl_ringleader_set_cookie(rl, id);

	ringleader_submit(rl);

	int ret = musl_ringleader_wait_result(rl, cookie);

	/* finish in-flight reads */
	rl_stdio_wait_pending_read(f);


	if(f->rl.buf_arena)
	{
		ringleader_free_arena(rl, f->rl.buf_arena);
		f->rl.buf_arena = NULL;
		f->buf = NULL;
	}
	return ret;
#endif

}
