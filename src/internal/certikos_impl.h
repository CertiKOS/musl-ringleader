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
void    musl_ringleader_init(void);
void *  musl_ringleader_statx_async(
			struct ringleader *rl,
			struct ringleader_arena *restrict arena,
			int dirfd,
			const char *restrict path,
			int flag,
			unsigned int mask,
			void ** out_shmem_statxbuff);

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

#endif
