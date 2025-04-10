#include "certikos_impl.h"
#include "ringleader.h"
#include <certikos/macros.h>
#include <string.h>

struct musl_rl_async_pwrite_ctx
{
	struct ringleader_arena *arena;
	int fd;
};

struct musl_rl_async_fd
{
	size_t offset;
	unsigned int flags;
	mode_t mode;
	size_t block_size;
	size_t file_size;
	size_t n_writes_pending;


	size_t n_reads_pending;

	int errcode;
	int file_open : 1;
	int do_close : 1;
};
struct musl_rl_async_fd musl_rl_async_fds[64] = {0};


void
musl_rl_async_fd_init(
		int fd,
		unsigned int flags,
		mode_t mode,
		size_t block_size,
		size_t block_count,
		size_t file_size)
{
	if(fd >= 64)
	{
		fprintf(stdenclave, "musl_ringleader_config_async: fd too large\n");
		return;
	}

	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];
	file->offset = 0;
	file->flags = flags;
	file->mode = mode;
	file->block_size = block_size;
	file->n_writes_pending = 0;
	file->n_reads_pending = 0;
	file->errcode = 0;
	file->file_open = 1;
	file->do_close = 0;
	file->file_size = file_size;

	if(file->flags & O_APPEND)
	{
		file->offset = file_size;
	}

	/* shmem is io_uring registered, so we can use O_DIRECT */
	if(!(file->flags & O_DIRECT))
	{
		//TODO async
		fcntl(fd, F_SETFL, O_DIRECT);
	}
}

bool
musl_rl_async_fd_check(int fd)
{
	return  fd < SIZEOF_ARRAY(musl_rl_async_fds) &&
			fd >= 0 &&
			musl_rl_async_fds[fd].file_open;
}

void
musl_rl_async_fd_finish_all(struct ringleader *rl)
{
	for(int i = 0; i < SIZEOF_ARRAY(musl_rl_async_fds); i++)
	{
		if(musl_rl_async_fds[i].file_open)
		{
			if(!musl_rl_async_fds[i].do_close)
			{
				(void) musl_rl_async_fd_close(rl, i);
			}
		}
	}

	for(int i = 0; i < SIZEOF_ARRAY(musl_rl_async_fds); i++)
	{
		while(musl_rl_async_fds[i].file_open)
		{
			/* Ignore any cookies since this should be called at exit time */
			struct io_uring_cqe cqe;
			(void) ringleader_peek_cqe(rl, &cqe);
		}
	}
}




void
musl_rl_async_fd_close_done(
		struct ringleader *rl,
		ringleader_promise_t promise,
		void *data)
{
	struct io_uring_cqe *cqe = data;
	struct musl_rl_async_fd *file = (void*)cqe->user_data;

	file->file_open = 0;
	ringleader_promise_set_result(rl, promise, (void*)(uintptr_t)cqe->res);
}


int
musl_rl_async_fd_close(
		struct ringleader *rl,
		int fd)
{
	if(!musl_rl_async_fd_check(fd))
	{
		fprintf(stdenclave, "musl_rl_async_fd_close: file not async\n");
		return -1;
	}

	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];
	if(!file->file_open)
	{
		fprintf(stdenclave, "musl_rl_async_fd_close: file not open\n");
		return -1;
	}

	if(file->n_writes_pending == 0 && file->n_reads_pending == 0)
	{
		int close_sqe = ringleader_prep_close(rl, fd);
		ringleader_sqe_set_data(rl, close_sqe, (void*)file);
		ringleader_promise_t promise =
				ringleader_sqe_then(
						rl, close_sqe, (void*)musl_rl_async_fd_close_done);
		ringleader_promise_free_on_fulfill(rl, promise);

		ringleader_submit(rl);
		return 0;
	}

	file->do_close = 1;
	return 0;
}

void
musl_rl_async_pwrite_done(
		struct ringleader *rl,
		ringleader_promise_t promise,
		void *data)
{
	struct io_uring_cqe *cqe = data;

	struct musl_rl_async_pwrite_ctx *ctx =
		ringleader_promise_get_ctx_fast(rl, promise,
				struct musl_rl_async_pwrite_ctx);

	struct musl_rl_async_fd *file = &musl_rl_async_fds[ctx->fd];

	if(cqe->res < 0 && file->errcode == 0)
	{
		file->errcode = -cqe->res;
	}

	if(file->n_writes_pending == 0 || !file->file_open)
	{
		fprintf(stdenclave, "musl_rl_async_pwrite_done: invalid file state\n");
		goto done;
	}
	file->n_writes_pending--;

	if(file->do_close)
	{
		(void) musl_rl_async_fd_close(rl, ctx->fd);
	}

done:
	ringleader_promise_set_result(rl, promise, (void*)(uintptr_t)cqe->res);
	ringleader_arena_free(rl, ctx->arena);
}



void
musl_rl_async_pwrite_ready(
		struct ringleader *rl,
		ringleader_promise_t p_ready,
		void *data)
{
	uint32_t sqe_id = (uint32_t)(uintptr_t)data;

	ringleader_promise_t p_done = ringleader_sqe_then(rl, sqe_id,
			(void*)musl_rl_async_pwrite_done);

	ringleader_promise_dup_ctx(rl, p_done, p_ready,
			struct musl_rl_async_pwrite_ctx);

	ringleader_submit(rl);
	ringleader_promise_free_on_fulfill(rl, p_done);
	ringleader_promise_set_result(rl, p_ready, NULL);
}


ssize_t
musl_rl_async_pwrite(
		struct ringleader *rl,
		struct ringleader_arena *arena,
		int fd,
		const void *buf,
		size_t count,
		off_t offset)
{
	void *shmem = ringleader_arena_apush(arena, buf, count);
	if(!shmem)
		return 0;

	off_t final_offset = (offset < 0) ? musl_rl_async_fds[fd].offset : offset;

	ringleader_promise_t p1 = ringleader_sqe_alloc(
			rl, fd, IORING_OP_WRITE_FIXED, shmem, count, final_offset);

	ringleader_promise_t p2 = ringleader_promise_chain(rl, p1);

	struct musl_rl_async_pwrite_ctx *ctx =
		ringleader_promise_get_ctx_fast(rl, p2,
				struct musl_rl_async_pwrite_ctx);
	ctx->arena = arena;
	ctx->fd = fd;

	ringleader_promise_then(rl, p2, musl_rl_async_pwrite_ready);
	ringleader_promise_free_on_fulfill(rl, p1);
	ringleader_promise_free_on_fulfill(rl, p2);


	musl_rl_async_fds[fd].n_writes_pending++;

	//TODO overflow
	musl_rl_async_fds[fd].file_size =
		MAX(musl_rl_async_fds[fd].file_size, final_offset + count);

	if(offset < 0)
	{
		musl_rl_async_fds[fd].offset += count;
	}

	return count;
}


off_t
musl_rl_async_fd_lseek(
		int fd,
		off_t offset,
		int whence)
{
	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];
	switch(whence)
	{
		case SEEK_SET:
			file->offset = offset;
			break;
		case SEEK_CUR:
			//TODO overflow
			file->offset += offset;
			break;
		case SEEK_END:
			//TODO IMPLEMENT
			fprintf(stdenclave, "musl_rl_async_fd_lseek: SEEK_END not implemented\n");
			exit(1);
			break;
#ifdef SEEK_DATA
		case SEEK_DATA:
		case SEEK_HOLE:
			//TODO IMPLEMENT
			fprintf(stdenclave, "musl_rl_async_fd_lseek: SEEK_DATA/SEEK_HOLE not implemented\n");
			exit(1);
			break;
#endif /* SEEK_DATA */
		default:
			return -1;
	}

	//TODO do we need to sync the OS?
	return file->offset;
}
