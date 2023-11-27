#include "certikos_impl.h"
#include <ringleader.h>
#include <certikos.h>
#include "syscall.h"
#include <stdatomic.h>
#include <limits.h>

#define MIN_ENTRIES 16

uint64_t shmem_cookie =      0x7f7f7f7f00000000;
uint64_t shmem_cookie_mask = 0xffffffff00000000;

// NULL RETURNS INDICATE ERRORS

static struct ringleader *static_rl = NULL;

struct ringleader*
get_ringleader(void)
{
    if (static_rl != NULL)
    {
        return static_rl;
    }
    static_rl = ringleader_factory(MIN_ENTRIES);
    err_t err;
    while ((err = ringleader_init_finished(static_rl)) == ERR_NOT_READY)
    {
    }

    if (err != ERR_OK)
    {
        certikos_printf("Failed to start ringleader, error %d\n", err);
        return NULL;
    }

    return static_rl;
}

int ringleader_allocated(void){
    return static_rl != NULL;
}

// allocate a new block of memory
// goes at the last block
void *
create_rl_shmem(struct ringleader* rl, int size)
{
    void * ret;
    shmem_cookie += 1;

    if (ringleader_request_shmem(rl, size, (void*)shmem_cookie) != ERR_OK)
    {
        certikos_puts("Failed to request shmem\n");
        return NULL;
    }


    struct io_uring_cqe* cqe = ringleader_get_cqe(rl);
    if ((uint64_t)(cqe->user_data & shmem_cookie_mask) == (shmem_cookie & shmem_cookie_mask))
    {
        if (ringleader_add_shmem(rl, cqe, size, &ret) == ERR_OK)
        {
            ringleader_consume_cqe(rl, cqe);
            return ret;
        }
        else
        {
            certikos_puts("Failed to alloc shmem.\n");
            ringleader_consume_cqe(rl, cqe);
            return NULL;
        }
    }
    else
    {
        certikos_puts("Did not get expected ringleader completion token");
        exit(-1);
    }
    return NULL;
}

// always use first shmem for now, easier for deubgging, i.e. shmem singleton.
void*
get_rl_shmem_singleton(void)
{
    static void* shmem = NULL;

    if(!shmem)
    {
        // TODO: modify this to be more dynamic and allow more than 4 pages of
        // memory using shmem malloc when that is completed
        struct ringleader* rl = get_ringleader();
        shmem = create_rl_shmem(rl, SHMEM_SIZE);
        if(!shmem)
        {
            certikos_puts("Failed to get singleton.\n");
        }
    }

    return shmem;
}

void*
alloc_new_rl_shmem(int size)
{
    struct ringleader* rl    = get_ringleader();
    return create_rl_shmem(rl, size);
}

