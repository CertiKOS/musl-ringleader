#include "stdio_impl.h"
#ifdef _CERTIKOS_
#include "certikos_impl.h"
#endif

static FILE *volatile dummy_file = 0;
weak_alias(dummy_file, __stdin_used);
weak_alias(dummy_file, __stdout_used);
weak_alias(dummy_file, __stderr_used);

static void close_file(FILE *f)
{
	if (!f) return;
	FFINALLOCK(f);
	if (f->wpos != f->wbase) f->write(f, 0, 0);
#ifdef _CERTIKOS_
    /* wait for outstanding write to complete */
    struct ringleader *rl = get_ringleader();
    while(f->rl.in_flight_arenas[0] || f->rl.in_flight_arenas[1])
    {
        struct io_uring_cqe * cqe = ringleader_peek_cqe(rl);
        (void)cqe;
    }
#endif
	if (f->rpos != f->rend) f->seek(f, f->rpos-f->rend, SEEK_CUR);
}

void __stdio_exit(void)
{
	FILE *f;
	for (f=*__ofl_lock(); f; f=f->next) close_file(f);
	close_file(__stdin_used);
	close_file(__stdout_used);
	close_file(__stderr_used);
}

weak_alias(__stdio_exit, __stdio_exit_needed);
