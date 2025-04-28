#ifndef _CERTIKOS_IMPL_H
#define _CERTIKOS_IMPL_H

#define DEFAULT_CONSOLE_ID 0u

#include <stdbool.h>
#include <stdint.h>
#include <certikos.h>
#include "stdio_impl.h"



void *  musl_ringleader_set_cookie(struct ringleader *rl, int32_t sqe_id);
struct  ringleader_arena * musl_ringleader_get_arena(
			struct ringleader * rl,
			size_t size);
int     musl_ringleader_wait_result(struct ringleader *rl, void * cookie);
void    musl_ringleader_flush_cqes(struct ringleader *rl);
void    musl_ringleader_init(void);
void *  musl_ringleader_statx_async(
			struct ringleader *rl,
			struct ringleader_arena * arena,
			int dirfd,
			char * shmem_path,
			int flag,
			unsigned int mask,
			void ** out_shmem_statxbuff);



void     musl_rl_async_fd_try_open(
			struct ringleader *rl,
			int fd,
			unsigned int flags,
			mode_t mode);

void    musl_rl_async_fd_finish_all(struct ringleader *rl);

int     musl_rl_async_fd_close(
			struct ringleader *rl,
			int fd);

ssize_t musl_rl_async_write(
			struct ringleader *rl,
			int fd,
			const void *buf,
			size_t count);

off_t   musl_rl_async_fd_lseek(
			struct ringleader *rl,
			int fd,
			off_t offset,
			int whence);

off_t musl_ringleader_do_lseek(
		struct ringleader *rl,
		int fd,
		off_t offset,
		int whence);

ssize_t musl_rl_async_fd_read(
			struct ringleader *rl,
			int fd,
			void *buf,
			size_t count);

struct musl_rl_async_fd*
musl_rl_async_fd_try_ensure(
		struct ringleader *rl,
		int fd);



static inline struct ringleader*
get_ringleader(void)
{
	extern struct ringleader *musl_rl;
	return musl_rl;
}

static inline void *
musl_ringleader_cookie(void)
{
	extern uintptr_t musl_rl_cookie;
	//TODO atomic for signals?
	musl_rl_cookie += 1;
	return (void *)musl_rl_cookie;
}

static inline bool
musl_ringleader_check_cookie(void *cookie, struct io_uring_cqe *cqe)
{
    return ((uintptr_t)(cqe->user_data) == ((uintptr_t)cookie));
}


static inline bool
musl_rl_async_fd_check(int fd)
{
	return musl_rl_async_fd_try_ensure(get_ringleader(), fd) != NULL;
}


#endif
