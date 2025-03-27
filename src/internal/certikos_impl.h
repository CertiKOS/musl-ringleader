#ifndef _CERTIKOS_IMPL_H
#define _CERTIKOS_IMPL_H

#define DEFAULT_CONSOLE_ID 0u

#include <stdbool.h>
#include <stdint.h>
#include <certikos.h>
#include "stdio_impl.h"

#define SHMEM_SIZE          (0x4000)

#define MUSL_RINGLEADER_COOKIE_START (0x7f7f7f7f00000000)
#define MUSL_RINGLEADER_COOKIE_MASK  (0xffffffff00000000)

void *  musl_ringleader_cookie(void);
bool    musl_ringleader_check_cookie(void *cookie, struct io_uring_cqe *cqe);
void *  musl_ringleader_set_cookie(struct ringleader *rl, int32_t sqe_id);
void    musl_ringleader_init(void);
void *  get_rl_shmem_singleton(void);
void *  alloc_new_rl_shmem(int size);
struct  ringleader_arena * musl_ringleader_get_arena(struct ringleader * rl, size_t size);
int     musl_ringleader_wait_result(struct ringleader *rl, void * cookie);

static inline struct ringleader*
get_ringleader(void)
{
    extern struct ringleader *static_rl;
    if (static_rl == NULL)
    {
        musl_ringleader_init();
    }

    return static_rl;
}



#endif
