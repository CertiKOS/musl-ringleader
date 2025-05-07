#define _GNU_SOURCE
#include <certikos/macros.h>
#include <string.h>
#include <ringleader.h>
#include <ringleader/reader.h>
#include <ringleader/writer.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "certikos_impl.h"
#include "statx.h"


struct musl_rl_async_fd
{
	struct ringleader_reader *reader;
	struct ringleader_writer *writer;

	struct
	{
		size_t block_size;
		size_t n_blocks;
		size_t file_size;
		size_t offset;
		size_t pipe_size;
	}; /* file */

	int errcode;
	unsigned int file_open : 1;
	unsigned int prepared : 1;
	unsigned int is_async : 1;
	unsigned int is_fifo : 1;
	unsigned int is_socket : 1;
	unsigned int do_close : 1;
};
struct musl_rl_async_fd musl_rl_async_fds[64] = {0};




/*
 * Determine the following:
 * 1. Named fifo, socket, pipe, or regular file?
 * 2. Block size
 * 3. File size
 * 4. Number of blocks
 * 5. Current offset
 * 6. Pipe size if pipe/fifo
 */
static void
musl_rl_async_fd_init_prepare_io(
	struct ringleader *rl,
	int fd)
{
	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];

	DEBUG_ASSERT(file->file_open == 1);
	if(file->prepared)
	{
		return;
	}

	file->prepared = 1;

	struct statx stx;
	int statx_flags = AT_EMPTY_PATH;
	unsigned int statx_mask = (
			STATX_BLOCKS |
			STATX_SIZE |
			STATX_TYPE |
			STATX_MODE);

	int statx_ret = musl_ringleader_statx(fd,
			"",
			statx_flags,
			statx_mask,
			&stx);

	if(statx_ret != 0)
	{
		fprintf(stdenclave,
				"musl_rl_async_fd_init_prepare_io: statx failed: %d\n",
				statx_ret);
		return;
	}

	file->block_size = stx.stx_blksize;
	file->file_size = stx.stx_size;
	file->n_blocks = stx.stx_blocks;

	if(S_ISFIFO(stx.stx_mode) || S_ISSOCK(stx.stx_mode) || S_ISREG(stx.stx_mode))
	{
		file->is_async = 1;
	}

	if(S_ISSOCK(stx.stx_mode))
	{
		file->is_socket = 1;
	}

	if(S_ISFIFO(stx.stx_mode))
	{
		file->is_fifo = 1;
	}

	if(S_ISREG(stx.stx_mode))
	{
		off_t offset = musl_ringleader_do_lseek(rl, fd, 0, SEEK_CUR);
		if(offset == -ESPIPE)
		{
			file->is_fifo = 1;
		}
		else if(offset >= 0)
		{
			file->offset = offset;
		}
		else
		{
			fprintf(stdenclave,
					"musl_rl_async_fd_init_prepare_io: lseek failed: %d\n",
					(int)offset);
		}
	}

	if(file->is_fifo)
	{
		file->pipe_size = musl_ringleader_fcntl(fd, F_GETPIPE_SZ, 0);
	}
}



static void
musl_rl_async_fd_init(
		struct ringleader *rl,
		int fd)
{
	/* discovered an open file */
	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];
	DEBUG_ASSERT(file->file_open == 0);

	memset(file, 0, sizeof(*file));

	file->file_open = 1;
}


struct musl_rl_async_fd*
musl_rl_async_fd_try_ensure(
		struct ringleader *rl,
		int fd)
{
	if(fd >= SIZEOF_ARRAY(musl_rl_async_fds) || fd < 0)
	{
		return NULL;
	}

	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];
	if(!file->file_open)
	{
		musl_rl_async_fd_init(rl, fd);
	}

	if(!file->prepared)
	{
		musl_rl_async_fd_init_prepare_io(rl, fd);
	}

	return file->is_async ? file : NULL;
}









void
musl_rl_async_fd_finish_all(struct ringleader *rl)
{
	for(int i = 0; i < SIZEOF_ARRAY(musl_rl_async_fds); i++)
	{
		if(musl_rl_async_fds[i].file_open && musl_rl_async_fds[i].is_async)
		{
			if(!musl_rl_async_fds[i].do_close)
			{
				(void) musl_rl_async_fd_close(rl, i);
			}
		}
	}

	for(int i = 0; i < SIZEOF_ARRAY(musl_rl_async_fds); i++)
	{
		while(musl_rl_async_fds[i].file_open && musl_rl_async_fds[i].is_async)
		{
			/* Ignore any cookies since this should be called at exit time */
			struct io_uring_cqe cqe;
			(void) ringleader_peek_cqe(rl, &cqe);
		}
	}
}





static void
musl_rl_async_fd_do_seek_set(
		struct ringleader *rl,
		int fd,
		off_t offset)
{
	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];

	file->offset = offset;

	if(file->writer)
	{
		ringleader_promise_t p_drain = ringleader_writer_drain(
			rl, file->writer);

		ringleader_promise_await(rl, p_drain, NULL);
	}

	if(file->reader)
	{
		ringleader_reader_reset(rl, &file->reader);
	}


	//TODO do lseek
}


off_t
musl_rl_async_fd_lseek(
		struct ringleader *rl,
		int fd,
		off_t offset,
		int whence)
{
	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];

	if(file->offset < 0)
	{
		return -ESPIPE;
	}

	switch(whence)
	{
		case SEEK_SET:
			if(offset != file->offset)
			{
				musl_rl_async_fd_do_seek_set(rl, fd, offset);
			}
			break;
		case SEEK_CUR:
			if(offset != 0)
			{
				musl_rl_async_fd_do_seek_set(rl, fd, file->offset + offset);
			}
			break;
		case SEEK_END:
			musl_rl_async_fd_do_seek_set(rl, fd, file->file_size + offset);
			break;
#ifdef SEEK_DATA
		case SEEK_DATA:
		case SEEK_HOLE:
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


static void
musl_rl_async_fd_close_done(
		struct ringleader *rl,
		ringleader_promise_t promise,
		void *data)
{
	struct io_uring_cqe *cqe = data;
	struct musl_rl_async_fd *file = (void*)cqe->user_data;

	file->file_open = 0;
	file->is_async = 0;
	file->is_fifo = 0;
	file->is_socket = 0;
	file->prepared = 0;

	if(file->reader)
	{
		ringleader_reader_destroy(rl, &file->reader);
	}

	ringleader_promise_set_result(rl, promise, (void*)(uintptr_t)cqe->res);
}


static void
musl_rl_async_fd_do_close(
		struct ringleader *rl,
		int fd)
{
	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];

	int close_sqe = ringleader_sqe_close_await(rl, fd);
	ringleader_sqe_set_data(rl, close_sqe, (void*)file);

	ringleader_promise_t p_close =
			ringleader_sqe_then(
					rl, close_sqe, musl_rl_async_fd_close_done);

	ringleader_submit(rl);
	ringleader_promise_free_on_fulfill(rl, p_close);
}


static void
musl_rl_async_fd_writer_drained(
		struct ringleader *rl,
		ringleader_promise_t p_drain,
		void *dontcare)
{
	int * ctx = ringleader_promise_get_ctx_fast(
			rl, p_drain, int);
	int fd = *ctx;

	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];
	ringleader_writer_destroy(rl, &file->writer);

	musl_rl_async_fd_do_close(rl, fd);

	ringleader_promise_set_result_dontcare(rl, p_drain);
}


int
musl_rl_async_fd_close(
		struct ringleader *rl,
		int fd)
{
	if(fd >= SIZEOF_ARRAY(musl_rl_async_fds) || fd < 0)
	{
		return -1;
	}

	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];
	if(!file->file_open)
	{
		return -1;
	}

	if(!file->is_async)
	{
		/* not an async file */
		file->file_open = 0;
		return -1;
	}

	file->do_close = 1;

	if(file->writer)
	{
		ringleader_promise_t p_drain = ringleader_writer_drain(
				rl, file->writer);

		int * ctx = ringleader_promise_get_ctx_fast(
				rl, p_drain, int);
		*ctx = fd;

		ringleader_promise_then(rl, p_drain, musl_rl_async_fd_writer_drained);
		ringleader_promise_free_on_fulfill(rl, p_drain);
	}
	else
	{
		musl_rl_async_fd_do_close(rl, fd);
	}

	return 0;
}



ssize_t
musl_rl_async_fd_read(
		struct ringleader *rl,
		int fd,
		void *buf,
		size_t count)
{
	/* ensure has been called before this */
	DEBUG_ASSERT(0 <= fd && fd < SIZEOF_ARRAY(musl_rl_async_fds));

	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];

	DEBUG_ASSERT(file->file_open);
	DEBUG_ASSERT(file->is_async);

	/* we don't support READ/WRITE files */
	ASSERT(!file->writer);

	if(file->reader == NULL)
	{
		size_t max_write = SIZE_MAX;

		if(file->is_fifo)
		{
			/* we want to grow the pipe size to allow for better asynchrony.
			 * TODO: strategic way to do this for multiple files? */
			int ret = musl_ringleader_fcntl(fd, F_SETPIPE_SZ, 1024*512);
			if(ret > 0)
			{
				file->pipe_size = ret;
			}
		}
		struct ringleader_arena * arena = musl_ringleader_get_arena(
				rl, NEXT_POW2(file->block_size) * 16);

		musl_rl_async_fds[fd].reader = ringleader_reader_factory(
				rl,
				arena,
				fd,
				file->block_size,
				musl_rl_async_fds[fd].file_size,
				512*1024 /* max buffer size, magic for Pi4b's L2 */);
	}

	ringleader_promise_t p_read = ringleader_reader_read(
			rl,
			file->reader,
			buf,
			count);

	ssize_t ret = (uintptr_t)ringleader_promise_await(rl, p_read, NULL);

	if(file->offset >= 0)
	{
		file->offset += ret;
	}

	return ret;
}

ssize_t
musl_rl_async_write(
		struct ringleader *rl,
		int fd,
		const void *buf,
		size_t count)
{
	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];

	/* we don't support READ/WRITE files */
	ASSERT(!file->reader);

	if(!file->writer)
	{
		size_t max_write = SIZE_MAX;

		if(file->is_fifo)
		{
			/* we want to grow the pipe size to allow for better asynchrony.
			 * TODO: strategic way to do this for multiple files? */
			int ret = musl_ringleader_fcntl(fd, F_SETPIPE_SZ, 1024*512);
			if(ret > 0)
			{
				file->pipe_size = ret;
			}

			max_write = file->pipe_size;
		}

		file->writer = ringleader_writer_factory(
				rl,
				fd,
				file->block_size,
				1024*512, /* max buffer size, magic for Pi4b's L2 */
				max_write);
	}

	ringleader_promise_t p_write = ringleader_writer_write(
			rl,
			file->writer,
			buf,
			count);

	return (ssize_t)ringleader_promise_await(rl, p_write, NULL);
}
