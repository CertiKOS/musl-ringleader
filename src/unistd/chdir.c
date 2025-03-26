#include <unistd.h>
#include "syscall.h"
#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <ringleader.h>
#include <limits.h>

int musl_ringleader_chdir(const char *path)
{
    int ret;
    struct ringleader *rl = get_ringleader();

    struct ringleader_arena *arena =
        musl_ringleader_get_arena(rl, PATH_MAX);
    if(!arena)
    {
        return __syscall_ret(-ENOMEM);
    }

    void *shmem = ringleader_arena_spush(arena, path);
    if(!shmem)
    {
        ringleader_free_arena(rl, arena);
        return __syscall_ret(-ENOMEM);
    }

    int32_t id = ringleader_prep_chdir(rl, shmem);
    ringleader_set_user_data(rl, id, (void*)CHDIR_COOKIE);
    ringleader_submit(rl);

    struct io_uring_cqe *cqe = ringleader_get_cqe(rl);
    if((uint64_t) cqe->user_data != CHDIR_COOKIE)
    {
        certikos_printf("read: unexpected cookie %llu", cqe->user_data);
        ringleader_consume_cqe(rl, cqe);
        ringleader_free_arena(rl, arena);
        return __syscall_ret(-EINVAL);
    }

    ret = cqe->res;
    ringleader_consume_cqe(rl, cqe);
    ringleader_free_arena(rl, arena);
    return ret;
}
#endif

int chdir(const char *path)
{
	return syscall(SYS_chdir, path);
}
