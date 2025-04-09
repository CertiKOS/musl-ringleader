#include "stdio_impl.h"
#include <sys/uio.h>
#include <string.h>

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#include <certikos/debug.h>

void
rl_stdio_write_done(
		struct ringleader *rl,
		ringleader_promise_t handle,
		void *data)
{
	struct io_uring_cqe * cqe = (void *)data;
    FILE *f = (void*)cqe->user_data;

    struct iovec * iov = f->rl.in_flight_iovs;
    size_t iov_count = !!iov[0].iov_len + !!iov[1].iov_len;
    size_t rem = iov[0].iov_len + iov[1].iov_len;

    //fprintf(stdenclave, "rl_stdio_write_done: (file=%p) %i/(%zu)\n",
    //        f, cqe->res, rem);
    //for(size_t i = 0; i < iov_count; i++)
    //{
    //    fprintf(stdenclave, "--iov[%zu]: %p (%zu) -> %p (%zu)\n",
    //            i,
    //            iov[i].iov_base,
    //            iov[i].iov_len,
    //            f->rl.iov_shmem[i].iov_base,
    //            f->rl.iov_shmem[i].iov_len);
    //}

    if((size_t)cqe->res == rem)
    {
        goto free_arenas;
    }

    if(cqe->res <= 0 || cqe->res > rem)
    {
        f->rl.err_res = cqe->res;
        f->flags |= F_ERR;
        goto free_arenas;
    }

    if((size_t)cqe->res >= iov[0].iov_len)
    {
        cqe->res -= iov[0].iov_len;
        iov[0].iov_base = (void*)((uintptr_t)iov[1].iov_base + cqe->res);
        iov[0].iov_len = iov[1].iov_len - (cqe->res);
        iov_count--;
    }

    iov[0].iov_base = (char *)iov[0].iov_base + cqe->res;
    iov[0].iov_len -= cqe->res;

    memcpy(f->rl.iov_shmem, iov, sizeof(*iov)*iov_count);

    uint32_t sqe = ringleader_prep_writev(rl, f->fd, f->rl.iov_shmem, iov_count, -1);
    ringleader_sqe_set_data(rl, sqe, f);
    ringleader_promise_t p = ringleader_sqe_then(rl, sqe, rl_stdio_write_done);
    ringleader_promise_free_on_fulfill(rl, p);
    ringleader_submit(rl);
    goto done;

free_arenas:
    ringleader_free_arena(rl, f->rl.in_flight_arenas[0]);
    ringleader_free_arena(rl, f->rl.in_flight_arenas[1]);
    f->rl.in_flight_arenas[0] = NULL;
    f->rl.in_flight_arenas[1] = NULL;
done:
    ringleader_consume_cqe(rl, cqe);
    ringleader_promise_set_result(rl, handle, (void*)(uintptr_t)cqe->res);
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
    if(f->wpos == f->wbase && (len == 0 || buf == NULL))
    {
        return 0;
    }

    struct ringleader *rl = get_ringleader();

    /* get buffer arena if not initialzied */
    if(!f->rl.buf_arena)
    {
        f->rl.buf_arena = musl_ringleader_get_arena(rl, BUFSIZ + UNGET);
        ringleader_arena_push(f->rl.buf_arena, UNGET);
    }

    /* migrate to arena buffer, if not in shmem */
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

    //fprintf(stdenclave, "rl_stdio_write: %p %zu %zu\n", f, f->wpos - f->wbase, len);


    /* get arena for io_vecs and buffer */
    struct ringleader_arena * new_arena = musl_ringleader_get_arena(
        rl,
        len + sizeof(struct iovec)*2);

    void * new_shmem = NULL;
    if(buf != NULL && len > 0)
    {
        new_shmem = ringleader_arena_apush(new_arena, buf, len);
    }

    struct ringleader_arena * new_stdio_arena = NULL;


    /* wait for current to finish */
    while(f->rl.in_flight_arenas[0] || f->rl.in_flight_arenas[1])
    {
        struct io_uring_cqe * cqe = ringleader_peek_cqe(rl);
        (void)cqe;

        if((f->rl.in_flight_arenas[0] || f->rl.in_flight_arenas[1]) &&
                (f->wpos != f->wbase) && !new_stdio_arena)
        {
            /* we are waiting on a write, let's request our new stdio buffer */
            new_stdio_arena = musl_ringleader_get_arena(rl, BUFSIZ + UNGET);
        }
    }

    if(f->flags & F_ERR)
    {
        ringleader_free_arena(rl, new_arena);
        ringleader_free_arena(rl, new_stdio_arena);
        return f->rl.err_res;
    }


    size_t index = 0;

    memset(f->rl.in_flight_iovs, 0, 2*sizeof(struct iovec));

    if(f->wpos != f->wbase)
    {
        f->rl.in_flight_arenas[index]           = f->rl.buf_arena;
        f->rl.in_flight_iovs[index].iov_base    = f->wbase;
        f->rl.in_flight_iovs[index++].iov_len   = f->wpos - f->wbase;
    }

    if(new_shmem)
    {
        f->rl.in_flight_arenas[index]           = new_arena;
        f->rl.in_flight_iovs[index].iov_base    = new_shmem;
        f->rl.in_flight_iovs[index++].iov_len   = len;
    }

    f->rl.iov_shmem = ringleader_arena_apush(new_arena,
            f->rl.in_flight_iovs,
            sizeof(struct iovec)*index);

    uint32_t sqe = ringleader_prep_writev(rl, f->fd, f->rl.iov_shmem, index, -1);
    ringleader_sqe_set_data(rl, sqe, f);
    ringleader_promise_t p = ringleader_sqe_then(rl, sqe, rl_stdio_write_done);
    ringleader_promise_free_on_fulfill(rl, p);
    ringleader_submit(rl);

    if(f->wpos != f->wbase)
    {
        /* we submitted our buffer, get a new one */
        f->rl.buf_arena = (new_stdio_arena) ? new_stdio_arena :
            musl_ringleader_get_arena(rl, BUFSIZ + UNGET);
        ringleader_arena_push(f->rl.buf_arena, UNGET);
        f->buf = ringleader_arena_push(f->rl.buf_arena, BUFSIZ);
        f->wend = f->buf + f->buf_size;
        f->wpos = f->wbase = f->buf;
    }


    return len;

#endif
}
