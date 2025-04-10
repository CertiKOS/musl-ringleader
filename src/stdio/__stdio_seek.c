#include "stdio_impl.h"
#include <unistd.h>
#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#endif

off_t __stdio_seek(FILE *f, off_t off, int whence)
{
#ifdef _CERTIKOS_
    struct ringleader *rl = get_ringleader();

    /* finish in-flight reads */
    rl_stdio_wait_pending_read(f);

    /* wait for outstanding write to complete */
    while(f->rl.in_flight_arenas[0] || f->rl.in_flight_arenas[1])
    {
        musl_ringleader_flush_cqes(rl);
    }
#endif
	return __lseek(f->fd, off, whence);
}
