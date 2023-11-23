#include "stdio_impl.h"

#undef stdout

#ifndef _CERTIKOS_
static unsigned char buf[BUFSIZ+UNGET];
#endif

hidden FILE __stdout_FILE = {
	#ifndef _CERTIKOS_
	.buf = buf+UNGET,
	.buf_size = sizeof buf-UNGET,
	#else
	.buf = NULL,
	.buf_size = 0,
	#endif
	.fd = 1,
	.flags = F_PERM | F_NORD,
	.lbf = '\n',
	.write = __stdout_write,
	.seek = __stdio_seek,
	.close = __stdio_close,
	.lock = -1,
};
FILE *const stdout = &__stdout_FILE;
FILE *volatile __stdout_used = &__stdout_FILE;
