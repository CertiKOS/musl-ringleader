#include "stdio_impl.h"

#undef stderr

//REMOVE CARVEOUT AND PUT UNGET INTO SHMEM
#ifndef _CERTIKOS_
static unsigned char buf[UNGET];
#endif

hidden FILE __stderr_FILE = {
	#ifndef _CERTIKOS_
	.buf = buf+UNGET,
	.buf_size = 0,
	#else
	.buf = NULL,
	.buf_size = 0,
	#endif
	.fd = 2,
	.flags = F_PERM | F_NORD,
	.lbf = -1,
	.write = __stdio_write,
	.seek = __stdio_seek,
	.close = __stdio_close,
	.lock = -1,
};
FILE *const stderr = &__stderr_FILE;
FILE *volatile __stderr_used = &__stderr_FILE;
