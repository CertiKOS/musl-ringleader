#include "stdio_impl.h"

#undef stdin

#ifdef _CERTIKOS_
static unsigned char buf[UNGET];
#else
static unsigned char buf[BUFSIZ+UNGET];
#endif

hidden FILE __stdin_FILE = {
	.buf = buf+UNGET,
	.buf_size = sizeof buf-UNGET,
	.fd = 0,
	.flags = F_PERM | F_NOWR,
	.read = __stdio_read,
	.seek = __stdio_seek,
	.close = __stdio_close,
	.lock = -1,
};
FILE *const stdin = &__stdin_FILE;
FILE *volatile __stdin_used = &__stdin_FILE;
