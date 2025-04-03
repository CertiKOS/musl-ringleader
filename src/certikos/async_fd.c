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
	struct musl_rl_async_pwrite_ctx *pwrite_ctxs;


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

	if(!file->pwrite_ctxs)
	{
		file->pwrite_ctxs = malloc(sizeof(*file->pwrite_ctxs) * 16);
		if(!file->pwrite_ctxs)
		{
			fprintf(stdenclave, "musl_ringleader_config_async: malloc failed\n");
			exit(1);
		}
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
			(void) ringleader_peek_cqe(rl);
		}
		fprintf(stdenclave, "musl_rl_async_fd_finish_all: closed fd=%d off=%zd sz=%zu\n", i, musl_rl_async_fds[i].offset, musl_rl_async_fds[i].file_size);
	}
}


err_t
musl_rl_async_fd_close_done(
		struct ringleader *rl,
		struct io_uring_cqe *cqe)
{
	struct musl_rl_async_fd *file = &musl_rl_async_fds[cqe->user_data];
	file->file_open = 0;
	ringleader_consume_cqe(rl, cqe);
	return ERR_OK_CONSUMED;
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
		ringleader_set_callback(rl, close_sqe, musl_rl_async_fd_close_done, (void*)(intptr_t)fd);
		ringleader_submit(rl);
		return 0;
	}

	file->do_close = 1;
	return 0;
}

err_t
musl_rl_async_pwrite_done(
		struct ringleader *rl,
		struct io_uring_cqe *cqe)
{
	struct musl_rl_async_pwrite_ctx *ctx = (void*)cqe->user_data;
	struct musl_rl_async_fd *file = &musl_rl_async_fds[ctx->fd];

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

	if(cqe->res < 0 && file->errcode == 0)
	{
		file->errcode = -cqe->res;
	}

done:
	ringleader_free_arena(rl, ctx->arena);
	free(ctx);
	ringleader_consume_cqe(rl, cqe);
	return ERR_OK_CONSUMED;
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

	int id = ringleader_prep_write(rl, fd, shmem, count, final_offset);

	//TODO non-malloc implementation
	struct musl_rl_async_pwrite_ctx *ctx = malloc(sizeof(*ctx));
	if(!ctx)
		return 0;

	ctx->arena = arena;
	ctx->fd = fd;

	if(ringleader_set_callback(rl, id, musl_rl_async_pwrite_done, ctx) != ERR_OK)
	{
		free(ctx);
		return 0;
	}

	musl_rl_async_fds[fd].n_writes_pending++;

	//TODO overflow
	musl_rl_async_fds[fd].file_size =
		MAX(musl_rl_async_fds[fd].file_size, final_offset + count);

	if(offset < 0)
	{
		musl_rl_async_fds[fd].offset += count;
	}

	//TODO check for full SQ
	ringleader_submit(rl);

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
