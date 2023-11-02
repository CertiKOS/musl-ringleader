#include "certikos_impl.h"
#include "ringleader.h"
#include "syscall.h"
#include <stdatomic.h>

#define MIN_ENTRIES 16
#define SHMEM_SIZE  0x4000

uint64_t shmem_cookie = 0u;

// NULL RETURNS INDICATE ERRORS

struct ringleader*
get_ringleader(void)
{
    static struct ringleader* rl = NULL;

    if (rl != NULL)
    {
        return rl;
    }
    rl = ringleader_factory(MIN_ENTRIES);
    err_t err;
    while ((err = ringleader_init_finished(rl)) == ERR_NOT_READY)
    {
    }

    if (err != ERR_OK)
    {
        certikos_printf("Failed to start ringleader, error %d", err);
        return NULL;
    }

    return rl;
}

// TODO: modify this to be more dynamic and allow more than 4kb of memory using
// shmem malloc when that is completed

// TODO: user data reprsents waht type of io_uring mapping is done, for now use
// IORING_OFF_SQ_RING =0
// this is passed to the mmap offset

// allocate a new block of memory
// goes at the last block
int
create_rl_shmem(struct ringleader* rl, int size)
{
    if (ringleader_request_shmem(rl, size, (void*)shmem_cookie) != ERR_OK)
    {
        return -1;
    }
    struct io_uring_cqe* cqe = ringleader_get_cqe(rl);
    if ((uint64_t)cqe->user_data == shmem_cookie)
    {
        if (ringleader_add_shmem(rl, cqe, size) == ERR_OK)
        {
            ringleader_consume_cqe(rl, cqe);
            return rl->shmem_size - 1;
        }
        else
        {
            ringleader_consume_cqe(rl, cqe);
            return -1;
        }
    }
    else
    {
        certikos_puts("Did not get expected ringleader completion token");
        exit(-1);
        return -1;
    }
}

// always use first shmem for now, easier for deubgging, i.e. shmem singleton.
void*
get_rl_shmem_singleton(void)
{
    struct ringleader* rl = get_ringleader();

    if (rl->shmem_size == 0)
    {
        if (create_rl_shmem(rl, SHMEM_SIZE) == -1)
            return NULL;
        return rl->shmem[0].addr;
    }
    else
    {
        return rl->shmem[0].addr;
    }
}

void*
alloc_new_rl_shmem(int size)
{
    // ensure the singleton is already allocated

    if (get_rl_shmem_singleton() == -1)
        return NULL;

    struct ringleader* rl    = get_ringleader();
    int                index = create_rl_shmem(rl, size);
    if (index == -1)
    {
        return NULL;
    }
    else
    {
        return rl->shmem[index].addr;
    }
}