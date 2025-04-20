#define _GNU_SOURCE
#include "certikos_impl.h"
#include <certikos/macros.h>
#include <string.h>
#include <ringleader.h>
#include <ringleader/reader.h>
#include <sys/stat.h>
#include <fcntl.h>


#ifndef STATX_TYPE
//TODO toolchain must include our built linux's headers
#define STATX_TYPE		0x00000001U	/* Want/got stx_mode & S_IFMT */
#define STATX_MODE		0x00000002U	/* Want/got stx_mode & ~S_IFMT */
#define STATX_NLINK		0x00000004U	/* Want/got stx_nlink */
#define STATX_UID		0x00000008U	/* Want/got stx_uid */
#define STATX_GID		0x00000010U	/* Want/got stx_gid */
#define STATX_ATIME		0x00000020U	/* Want/got stx_atime */
#define STATX_MTIME		0x00000040U	/* Want/got stx_mtime */
#define STATX_CTIME		0x00000080U	/* Want/got stx_ctime */
#define STATX_INO		0x00000100U	/* Want/got stx_ino */
#define STATX_SIZE		0x00000200U	/* Want/got stx_size */
#define STATX_BLOCKS		0x00000400U	/* Want/got stx_blocks */
#define STATX_BASIC_STATS	0x000007ffU	/* The stuff in the normal stat struct */
#define STATX_BTIME		0x00000800U	/* Want/got stx_btime */
#define STATX_MNT_ID		0x00001000U	/* Got stx_mnt_id */
#define STATX_DIOALIGN		0x00002000U	/* Want/got direct I/O alignment info */

#define STATX__RESERVED		0x80000000U	/* Reserved for future struct statx expansion */
struct statx {
	uint32_t stx_mask;
	uint32_t stx_blksize;
	uint64_t stx_attributes;
	uint32_t stx_nlink;
	uint32_t stx_uid;
	uint32_t stx_gid;
	uint16_t stx_mode;
	uint16_t pad1;
	uint64_t stx_ino;
uint64_t stx_size;
	uint64_t stx_blocks;
	uint64_t stx_attributes_mask;
	struct {
		int64_t tv_sec;
		uint32_t tv_nsec;
		int32_t pad;
	} stx_atime, stx_btime, stx_ctime, stx_mtime;
	uint32_t stx_rdev_major;
	uint32_t stx_rdev_minor;
	uint32_t stx_dev_major;
	uint32_t stx_dev_minor;
	uint64_t spare[14];
};
#endif /* STATX_TYPE (no statx defs) */


/* optimize regular file, pipe, socket writes/reads by asynchronous IO */
struct musl_rl_async_fd
{
	struct
	{
		size_t file_size;
		ssize_t offset;
	}; /* file */

	ringleader_promise_t p_fstat;

	int errcode;
	unsigned int file_open : 1;
	unsigned int statx_done : 1;
	unsigned int is_async : 1;
	unsigned int do_close : 1;

	struct
	{
		struct ringleader_arena *arena;
		void * block;
		size_t block_size;
		size_t block_off;
		size_t n_pending;
		size_t estimated_max_write_size;
	} writer;

	struct ringleader_reader *reader;
};

struct musl_rl_async_write_ctx
{
	struct ringleader_arena *arena;
	int fd;
	size_t total;
	size_t remaining;
};


struct musl_rl_async_fd musl_rl_async_fds[64] = {0};


static void
musl_rl_async_fd_do_statx(
		struct ringleader *rl,
		int fd)
{
	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];

	//TODO async statx
	struct statx stx;
	//int statx_flags = (flags & O_NOFOLLOW) ? AT_SYMLINK_NOFOLLOW : 0;
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

	if(statx_ret == 0)
	{
		file->file_size = stx.stx_size;

		file->writer.block_size = stx.stx_blksize;
		file->file_size = stx.stx_size;

		/* Buffering a FIFO or Socket is not supported.
		 * It will break some of the expected behaviors */
		if(S_ISFIFO(stx.stx_mode) || S_ISSOCK(stx.stx_mode))
		{
			file->offset = -1;
			file->is_async = 1;
		}

		if(S_ISREG(stx.stx_mode))
		{
			file->is_async = 1;
		}
	}

	file->statx_done = 1;
}


void
musl_rl_async_fd_init(
		struct ringleader *rl,
		int fd)
{
	if(fd >= 64 || fd < 0)
	{
		/* we only async files 0-63 */
		return;
	}

	/* discovered an open file */
	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];

	if(file->file_open && file->do_close)
	{
		ringleader_reader_destroy(rl, &file->reader);

		/* wait until close is done */
		while(file->file_open)
		{
			musl_ringleader_flush_cqes(rl);
		}
	}

	file->file_open = 1;
	file->is_async = 0;
	file->statx_done = 0;
	file->errcode = 0;
	file->do_close = 0;

	file->offset = 0;

	file->writer.arena = NULL;
	file->writer.block = NULL;
	file->writer.block_off = 0;
	file->writer.block_size = 4096;
	file->writer.n_pending = 0;
	file->writer.estimated_max_write_size = 128*1024;

	musl_rl_async_fd_do_statx(rl, fd);
}


void
musl_rl_async_fd_init_reading(
		struct ringleader *rl,
		int fd,
		size_t hint)
{
	struct ringleader_arena * arena = musl_ringleader_get_arena(
			rl, NEXT_POW2(hint) * 16);

	musl_rl_async_fds[fd].reader = ringleader_reader_factory(
			rl,
			arena,
			fd,
			0,
			musl_rl_async_fds[fd].file_size);
}


void
musl_rl_async_fd_try_open(
		struct ringleader *rl,
		int fd,
		unsigned int flags,
		mode_t mode)
{
	if(fd >= 64 || fd < 0)
	{
		/* we only async files 0-63 */
		return;
	}

	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];

	if((flags & O_DIRECTORY) || (flags & O_PATH))
	{
		/* not an async file */
		file->file_open = 1;
		file->statx_done = 1;
		file->is_async = 0;
		return;
	}

	musl_rl_async_fd_init(rl, fd);

	if(flags & O_APPEND)
	{
		file->offset = file->file_size;
	}

	if(flags & O_RDONLY || flags & O_RDWR)
	{
		musl_rl_async_fd_init_reading(rl, fd, file->writer.block_size);
	}

	if(flags & O_WRONLY || flags & O_RDWR)
	{
		//NOP
	}
}

bool
musl_rl_async_fd_check(int fd)
{
	if(fd >= SIZEOF_ARRAY(musl_rl_async_fds) || fd < 0)
	{
		return false;
	}

	if(!musl_rl_async_fds[fd].file_open)
	{
		musl_rl_async_fd_init(get_ringleader(), fd);
	}


	if(!musl_rl_async_fds[fd].statx_done)
	{
		musl_rl_async_fd_do_statx(get_ringleader(), fd);
	}

	return musl_rl_async_fds[fd].is_async;
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
musl_rl_async_dispatch_block(
		struct ringleader *rl,
		int fd,
		size_t new_block_size);


static void
musl_rl_async_write_done(
		struct ringleader *rl,
		ringleader_promise_t promise,
		void *data)
{
	struct io_uring_cqe *cqe = data;

	struct musl_rl_async_write_ctx *ctx =
		ringleader_promise_get_ctx_fast(rl, promise,
				struct musl_rl_async_write_ctx);

	struct musl_rl_async_fd *file = &musl_rl_async_fds[ctx->fd];

	DEBUG_ASSERT(file->file_open);
	DEBUG_ASSERT(file->writer.n_pending > 0);

	ringleader_promise_set_result(rl, promise, (void*)(uintptr_t)cqe->res);
	ringleader_arena_free(rl, ctx->arena);

	if(cqe->res < 0 && file->errcode == 0)
	{
		file->errcode = -cqe->res;
	}

	file->writer.n_pending--;

	if(file->writer.block_off > 0 &&
			file->offset < 0 &&
			file->writer.n_pending == 0)
	{
		/* we are a FIFO or socket, and have not flushed all our data,
		 * and no other writes are occurring, then flush */
		musl_rl_async_dispatch_block(rl, ctx->fd, file->writer.block_size);
	}
	//TODO forward arena, instead of allocating a new one
	//else if(file->writer.arena == NULL)
	//{
	//}

	if(file->do_close && file->writer.n_pending == 0)
	{
		(void) musl_rl_async_fd_close(rl, ctx->fd);
	}
}


static void
musl_rl_async_write_ready(
		struct ringleader *rl,
		ringleader_promise_t p_ready,
		void *data)
{
	uint32_t sqe_id = (uint32_t)(uintptr_t)data;

	ringleader_sqe_set_flags(rl, sqe_id, IOSQE_ASYNC);

	ringleader_promise_t p_done = ringleader_sqe_then(rl, sqe_id,
			(void*)musl_rl_async_write_done);

	ringleader_promise_dup_ctx(rl, p_done, p_ready,
			struct musl_rl_async_write_ctx);

	ringleader_submit(rl);
	ringleader_promise_free_on_fulfill(rl, p_done);
	ringleader_promise_set_result(rl, p_ready, NULL);
}

static void
musl_rl_async_dispatch_block(
		struct ringleader *rl,
		int fd,
		size_t new_block_size)
{
	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];
	struct ringleader_arena *arena = file->writer.arena;
	void *shmem = file->writer.block;
	size_t count = file->writer.block_off;

	if(count == 0)
	{
		return;
	}

	off_t offset = (file->offset >= 0) ?
		(file->offset - file->writer.block_off) :
		-1;

	ringleader_promise_t p1 = ringleader_sqe_alloc(
			rl, fd, IORING_OP_WRITE_FIXED, shmem, count, offset);

	ringleader_promise_t p2 = ringleader_promise_chain(rl, p1);

	struct musl_rl_async_write_ctx *ctx =
		ringleader_promise_get_ctx_fast(rl, p2,
				struct musl_rl_async_write_ctx);
	ctx->arena = arena;
	ctx->fd = fd;

	ringleader_promise_then(rl, p2, musl_rl_async_write_ready);
	ringleader_promise_free_on_fulfill(rl, p1);
	ringleader_promise_free_on_fulfill(rl, p2);
	musl_rl_async_fds[fd].writer.n_pending++;

	file->writer.block_off = 0;

	if(new_block_size == 0)
	{
		file->writer.arena = NULL;
		file->writer.block = NULL;
		return;
	}

	file->writer.arena = musl_ringleader_get_arena(rl, new_block_size);
	file->writer.block = ringleader_arena_push(
		file->writer.arena, new_block_size);
	file->writer.block_size = new_block_size;
}



ssize_t
musl_rl_async_write(
		struct ringleader *rl,
		int fd,
		const void *buf,
		size_t count)
{
	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];

	if(!file->writer.arena)
	{
		file->writer.arena = musl_ringleader_get_arena(rl, file->writer.block_size);
		file->writer.block = ringleader_arena_push(
				file->writer.arena, file->writer.block_size);
		file->writer.block_off = 0;
	}

	size_t rem = count;
	size_t blk_rem = file->writer.block_size - file->writer.block_off;
	size_t cpy_size = MIN(blk_rem, rem);

	memcpy((char*)file->writer.block + file->writer.block_off,
			buf,
			cpy_size);

	file->writer.block_off += cpy_size;
	rem -= cpy_size;
	if(file->offset >= 0)
	{
		file->offset += cpy_size;
	}

	/* if we have a full block, or
	 * if we are a FIFO or socket and we aren't currently writing data,
	 * then flush the block */
	if(file->writer.block_off == file->writer.block_size ||
			(file->offset < 0 && file->writer.n_pending == 0))
	{
		size_t new_block_size = file->writer.block_size;

		/* for non-FIFO/socket files, we can double the block size
		 * if we have a lot of pending writes */
		if(file->writer.n_pending > 8 && file->offset >= 0)
		{
			new_block_size *= 2;
		}

		/* Scale our block size up to allow for at least 16 copies */
		while(new_block_size < count * 16)
		{
			//TODO no overflow check
			new_block_size *= 2;
		}

		musl_rl_async_dispatch_block(rl, fd,
				new_block_size);

		if(rem)
		{
			memcpy((char*)file->writer.block,
					(const char *)buf + cpy_size,
					rem);
			file->writer.block_off = rem;
			if(file->offset >= 0)
			{
				file->offset += rem;
			}
		}
	}

	file->file_size = MAX(file->file_size, file->offset);

	return count;
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
				musl_rl_async_dispatch_block(rl, fd, file->writer.block_size);

				file->offset = offset;
				ringleader_reader_reset(
						rl,
						&file->reader,
						offset);
			}
			break;
		case SEEK_CUR:
			if(offset != 0)
			{
				musl_rl_async_dispatch_block(rl, fd, file->writer.block_size);

				file->offset += offset;
				ringleader_reader_reset(
						rl,
						&file->reader,
						file->offset);
			}
			break;
		case SEEK_END:
			if(offset != 0)
			{
				musl_rl_async_dispatch_block(rl, fd, file->writer.block_size);
				ringleader_reader_reset(
						rl,
						&file->reader,
						file->offset + file->file_size + offset);
			}
			file->offset = file->file_size + offset;
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


void
musl_rl_async_fd_close_done(
		struct ringleader *rl,
		ringleader_promise_t promise,
		void *data)
{
	struct io_uring_cqe *cqe = data;
	struct musl_rl_async_fd *file = (void*)cqe->user_data;

	file->file_open = 0;
	file->statx_done = 0;
	file->is_async = 0;

	if(file->reader)
	{
		ringleader_reader_destroy(rl, &file->reader);
	}

	ringleader_promise_set_result(rl, promise, (void*)(uintptr_t)cqe->res);
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

	musl_rl_async_dispatch_block(rl, fd, 0);

	if(file->writer.n_pending == 0)
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



ssize_t
musl_rl_async_fd_read(
		struct ringleader *rl,
		int fd,
		void *buf,
		size_t count)
{
	struct musl_rl_async_fd *file = &musl_rl_async_fds[fd];
	DEBUG_ASSERT(file->file_open);

	if(file->reader == NULL)
	{
		musl_rl_async_fd_init_reading(rl, fd, file->writer.block_size);
	}

	/* if have been writing, flush writes first */
	if(file->writer.n_pending > 0 || file->writer.block_off > 0)
	{
		musl_rl_async_dispatch_block(rl, fd, 0);

		/* don't read until writes are all done */
		while(file->writer.n_pending > 0)
		{
			musl_ringleader_flush_cqes(rl);
		}
	}



	ringleader_promise_t p_read = ringleader_reader_read(
			rl,
			file->reader,
			buf,
			count);

	ssize_t ret = (uintptr_t)ringleader_promise_await(rl, p_read, NULL);

	if(file->offset >= 0)
	{
		file->offset = file->reader->file.off;
	}

	return ret;
}

