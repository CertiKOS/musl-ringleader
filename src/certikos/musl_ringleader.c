#include <ringleader.h>
#include <certikos.h>
#include <certikos/profile.h>
#include <stdatomic.h>
#include <limits.h>
#include "libc.h"
#include "certikos_impl.h"
#include "syscall.h"

#define MUSL_RINGLEADER_COOKIE_START    (0x7f7f7f7f00000000)
#define MIN_ENTRIES                     16
#define SAFE_OFFSET                     (0x100)

struct ringleader *musl_rl = NULL;
uintptr_t musl_rl_cookie = MUSL_RINGLEADER_COOKIE_START;
struct overheads_t musl_overheads;


void *
musl_ringleader_set_cookie(struct ringleader *rl, int32_t sqe_id)
{
    void *cookie = musl_ringleader_cookie();
    ringleader_set_user_data(rl, sqe_id, cookie);
    return cookie;
}

void
musl_ringleader_arena_callback(
        struct ringleader *rl,
        ringleader_promise_t promise,
        void *data)
{
    struct ringleader_arena *arena = data;

    /* this was prefetched memory */
    (void) ringleader_arena_free(rl, arena);

    ringleader_promise_set_result_and_free(rl, promise, data);
}


void
musl_ringleader_init(void)
{
    musl_rl = ringleader_factory(MIN_ENTRIES);
    if(!musl_rl)
    {
        certikos_printf("Fatal: failed to allocate static RingLeader\n");
        return;
    }

    err_t err;
    while ((err = ringleader_init_finished(musl_rl)) == ERR_NOT_READY)
    {
    }

    if (err != ERR_OK)
    {
        certikos_printf("Failed to start ringleader, error %d\n", err);
        return;
    }


    /* prerequest shmem for paths, stdout, stdin, and stderr */
    size_t size = (PATH_MAX + SAFE_OFFSET)*8 + (BUFSIZ + UNGET + SAFE_OFFSET)*3;
    ringleader_promise_t p = ringleader_arena_request(musl_rl, size);
    if(p == RINGLEADER_PROMISE_INVALID)
    {
        fprintf(stdenclave, "Failed to prefetch arena\n");
    }
    ringleader_promise_t p2 =
        ringleader_promise_then(musl_rl, p, musl_ringleader_arena_callback);

    ringleader_promise_free_on_fulfill(musl_rl, p);
    ringleader_promise_free_on_fulfill(musl_rl, p2);



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


int
musl_ringleader_wait_result(struct ringleader *rl, void * cookie)
{
	static struct {
		struct {
			void * cookie;
			int result;
		} * data;
		size_t capacity;
		size_t size;
	} results = {NULL, 8, 0};

	int ret = -1;

	for(size_t i = 0; i < results.size; i++)
	{
		if(results.data[i].cookie == cookie)
		{
			ret = results.data[i].result;
			results.data[i] = results.data[--results.size];
			return ret;
		}
	}

	while(1)
	{
		struct io_uring_cqe *cqe = ringleader_peek_cqe(rl);

		/* allocate results array in spare time */
		if(cqe == NULL || cqe->user_data != (uintptr_t)cookie)
		{
			if(results.data == NULL || results.size == results.capacity)
			{
				results.capacity *= 2;
				results.data = realloc(results.data,
						results.capacity * sizeof(*results.data));
				if(!results.data)
				{
					fprintf(stdenclave, "Failed to reallocate results array.\n");
					return -1;
				}
			}

			if(cqe && cqe->user_data != (uintptr_t)cookie)
			{
				results.data[results.size].cookie = (void *)cqe->user_data;
				results.data[results.size].result = cqe->res;
				results.size++;
				ringleader_consume_cqe(rl, cqe);
			}
		}
		else
		{
			ret = cqe->res;
			ringleader_consume_cqe(rl, cqe);
			return ret;
		}
	}

	fprintf(stdenclave, "Failed to wait for result.\n");
	return ret;
}


struct ringleader_arena *
musl_ringleader_get_arena(struct ringleader *rl, size_t size)
{
	ringleader_promise_t promise = ringleader_arena_request(rl, size);
	if(promise == RINGLEADER_PROMISE_INVALID)
	{
		return NULL;
	}

	return ringleader_promise_await(rl, promise, NULL);
}




