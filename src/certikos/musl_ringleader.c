#include <ringleader.h>
#include <certikos.h>
#include <certikos/profile.h>
#include <stdatomic.h>
#include <limits.h>
#include "libc.h"
#include "certikos_impl.h"
#include "syscall.h"

struct overheads_t musl_overheads;

#define MIN_ENTRIES 16

uint64_t shmem_cookie =      0x7f7f7f7f00000000;
uint64_t shmem_cookie_mask = 0xffffffff00000000;


struct ringleader *static_rl = NULL;

err_t
musl_ringleader_arena_callback(struct ringleader *rl, struct io_uring_cqe *cqe)
{
    struct ringleader_arena *arena;

    if(ringleader_init_arena(rl, cqe, &arena) != ERR_OK)
    {
        certikos_printf("Failed to init arena\n");
    } else {
        ringleader_free_arena(rl, arena);
    }

    ringleader_consume_cqe(rl, cqe);
    return ERR_OK_CONSUMED;
}


void
musl_ringleader_init(void)
{
    static_rl = ringleader_factory(MIN_ENTRIES);
    if(!static_rl)
    {
        certikos_printf("Fatal: failed to allocate static RingLeader\n");
        return;
    }

    err_t err;
    while ((err = ringleader_init_finished(static_rl)) == ERR_NOT_READY)
    {
    }

    if (err != ERR_OK)
    {
        certikos_printf("Failed to start ringleader, error %d\n", err);
        return;
    }

    /* prerequest shmem for singleton, stdout, stdin, and stderr */
    if(ringleader_request_arena_callback(static_rl, SHMEM_SIZE + 0x100,
                musl_ringleader_arena_callback, NULL) != ERR_OK)
    {
        certikos_printf("Failed to request shmem\n");
    }

    if(ringleader_request_arena_callback(static_rl, BUFSIZ + UNGET + 0x100,
                musl_ringleader_arena_callback, NULL) != ERR_OK)
    {
        certikos_printf("Failed to request shmem\n");
    }

    if(ringleader_request_arena_callback(static_rl, BUFSIZ + UNGET + 0x100,
                musl_ringleader_arena_callback, NULL) != ERR_OK)
    {
        certikos_printf("Failed to request shmem\n");
    }

    if(ringleader_request_arena_callback(static_rl, BUFSIZ + UNGET + 0x100,
                musl_ringleader_arena_callback, NULL) != ERR_OK)
    {
        certikos_printf("Failed to request shmem\n");
    }


#ifdef MUSL_RINGLEADER_PROFILE
    overheads_init(&musl_overheads);
    overheads_track(&musl_overheads, track_ringleader_musl_base_overhead, prf_style_void);
    overheads_track(&musl_overheads, track_ringleader_musl_unlinkat, prf_style_void);
    overheads_track(&musl_overheads, track_ringleader_musl_openat, prf_style_void);
    overheads_track(&musl_overheads, track_ringleader_musl_getppid, prf_style_void);
    overheads_track(&musl_overheads, track_ringleader_musl_write, prf_style_void);
    overheads_track(&musl_overheads, track_ringleader_musl_write_apush, prf_style_void);
#endif

}


void*
get_rl_shmem_singleton(void)
{
    static void* shmem = NULL;
    static struct ringleader_arena * singleton_arena = NULL;

    if(!shmem)
    {
        struct ringleader * rl = get_ringleader();
        singleton_arena = musl_ringleader_get_arena(rl, SHMEM_SIZE);
        shmem = ringleader_arena_push(singleton_arena, SHMEM_SIZE);
    }

    return shmem;
}



struct ringleader_arena *
musl_ringleader_get_arena(struct ringleader *rl, size_t size)
{
    struct io_uring_cqe* cqe;

    size += 0x100;

    /* fast path */
    struct ringleader_arena * ret = ringleader_get_free_arena(rl, size);
    if(ret)
    {
        return ret;
    }

    shmem_cookie += 1;
    if(ringleader_request_arena(rl, size, (void*)shmem_cookie) != ERR_OK)
    {
        fprintf(stdenclave, "Failed to request arena.\n");
        return NULL;
    }

    while(1)
    {
        cqe = ringleader_get_cqe(rl);
        if(!cqe)
        {
            fprintf(stdenclave, "Failed to get cqe.\n");
            return NULL;
        }

        if((uint64_t)(cqe->user_data & shmem_cookie_mask) ==
                (shmem_cookie & shmem_cookie_mask))
        {
            break;
        }

        fprintf(stdenclave, "get_rl_arena: unexpected cookie\n");
        ringleader_consume_cqe(rl, cqe);
    }


    if (ringleader_init_arena(rl, cqe, &ret) != ERR_OK)
    {
        fprintf(stdenclave, "Failed to alloc shmem.\n");
    }

    ringleader_consume_cqe(rl, cqe);
    return ret;
}



int musl_ringleader_wait_result(struct ringleader *rl, uintptr_t cookie)
{
    int ret;
    struct io_uring_cqe *cqe = ringleader_get_cqe(rl);

    while((uintptr_t)cqe->user_data != cookie)
    {
        ringleader_consume_cqe(rl, cqe);
        cqe = ringleader_get_cqe(rl);
    }

    ret = cqe->res;
    ringleader_consume_cqe(rl, cqe);
    return ret;
}

