#include <unistd.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"

void musl_ringleader_sync()
{
	struct ringleader *rl = get_ringleader();
	uint32_t id = ringleader_prep_sync(rl);

	/* drain all sqes before this one, prevent future sqes from being
	 * submitted until this one is completed */
	ringleader_sqe_set_flags(rl, id, IOSQE_IO_DRAIN | IOSQE_CQE_SKIP_SUCCESS);
	ringleader_submit(rl);

	/* don't wait */
}

#endif

void sync(void)
{
#ifdef _CERTIKOS_
	musl_ringleader_sync();
#else
	__syscall(SYS_sync);
#endif
}
