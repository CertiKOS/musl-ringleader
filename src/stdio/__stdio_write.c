#include "stdio_impl.h"
#include <sys/uio.h>
#include <string.h>

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"



err_t
rl_stdio_write_done(struct ringleader *rl, struct io_uring_cqe *cqe)
{
    FILE *f = (void*)cqe->user_data;

    size_t iov_count = 2;
    struct iovec * iov = f->rl.in_flight_iovs;
    size_t rem = iov[0].iov_len + iov[1].iov_len;

    if(cqe->res < 0)
    {
        f->flags |= F_ERR;
        goto free_arenas;
    }

    if((size_t)cqe->res == rem)
    {
        goto free_arenas;
    }

    rem -= cqe->res;

    if((size_t)cqe->res > iov[0].iov_len)
    {
        cqe->res -= iov[0].iov_len;
        iov[0].iov_len = 0;
        iov++;
        iov_count--;
    }

    iov[0].iov_base = (char *)iov[0].iov_base + cqe->res;
    iov[0].iov_len -= cqe->res;

    memcpy(f->rl.iov_shmem, iov, sizeof(*iov)*iov_count);

    uint32_t sqe = ringleader_prep_writev(rl, f->fd, f->rl.iov_shmem, iov_count, -1);
    ringleader_set_callback(rl, sqe, rl_stdio_write_done, f);
    ringleader_submit(rl);
    goto done;

free_arenas:
    ringleader_free_arena(rl, f->rl.in_flight_arenas[0]);
    ringleader_free_arena(rl, f->rl.in_flight_arenas[1]);
    f->rl.in_flight_arenas[0] = NULL;
    f->rl.in_flight_arenas[1] = NULL;
done:
    ringleader_consume_cqe(rl, cqe);
    return ERR_OK_CONSUMED;
}

#endif




size_t __stdio_write(FILE *f, const unsigned char *buf, size_t len)
{
#ifndef _CERTIKOS_
		struct iovec iovs[2] = {
		{ .iov_base = f->wbase, .iov_len = f->wpos-f->wbase },
		{ .iov_base = (void *)buf, .iov_len = len }
	};

	struct iovec *iov = iovs;
	size_t rem = iov[0].iov_len + iov[1].iov_len;
	int iovcnt = 2;
	ssize_t cnt;

	for (;;) {

		cnt = syscall(SYS_writev, f->fd, iov, iovcnt);

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
#else

    struct ringleader *rl = get_ringleader();
    struct ringleader_arena * new_arena = musl_ringleader_get_arena(rl,
            len + sizeof(struct iovec)*2);
    void * new_shmem = ringleader_arena_apush(new_arena, buf, len);

    if(!f->rl.buf_arena)
    {
        f->rl.buf_arena = musl_ringleader_get_arena(rl, BUFSIZ + UNGET);
        ringleader_arena_push(f->rl.buf_arena, UNGET);
    }

    if(!ringleader_arena_contains(f->rl.buf_arena, f->buf))
    {
        /* first stdout, and handling for all stderr prints */
        /* copy current buffer into new shmem buffer */
        intptr_t diff = (intptr_t)f->buf;
        f->buf = ringleader_arena_apush(f->rl.buf_arena, f->buf, f->buf_size);
        diff = (intptr_t)f->buf - diff;
        f->wpos += diff;
        f->wbase += diff;
        f->wend += diff;
    }

    while(f->rl.in_flight_arenas[0] || f->rl.in_flight_arenas[1])
    {
        struct io_uring_cqe * cqe = ringleader_peek_cqe(rl);
        (void)cqe;
    }

    f->rl.in_flight_arenas[0] = f->rl.buf_arena;
    f->rl.in_flight_arenas[1] = new_arena;

    f->rl.in_flight_iovs[0] = (struct iovec){
        .iov_base = f->wbase,
        .iov_len = f->wpos-f->wbase,
    };
    f->rl.in_flight_iovs[1] = (struct iovec){
        .iov_base = new_shmem,
        .iov_len = len,
    };

    f->rl.iov_shmem = ringleader_arena_apush(new_arena,
            f->rl.in_flight_iovs,
            sizeof(struct iovec)*2);

    uint32_t sqe = ringleader_prep_writev(rl, f->fd, f->rl.iov_shmem, 2, -1);
    ringleader_set_callback(rl, sqe, rl_stdio_write_done, f);
    ringleader_submit(rl);

    f->rl.buf_arena = musl_ringleader_get_arena(rl, BUFSIZ + UNGET);
    ringleader_arena_push(f->rl.buf_arena, UNGET);
    f->buf = ringleader_arena_push(f->rl.buf_arena, BUFSIZ);
    f->wend = f->buf + f->buf_size;
    f->wpos = f->wbase = f->buf;

    return len;

#endif
}
