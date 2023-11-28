#include "stdio_impl.h"
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#ifdef _CERTIKOS_
#include "ringleader.h"
#include "certikos_impl.h"
#endif

FILE *fopen(const char *restrict filename, const char *restrict mode)
{
	FILE *f;
	int fd;
	int flags;

	/* Check for valid initial mode character */
	if (!strchr("rwa", *mode)) {
		errno = EINVAL;
		return 0;
	}

	/* Compute the flags to pass to open() */
	flags = __fmodeflags(mode);

	fd = sys_open(filename, flags, 0666);
	if (fd < 0) return 0;
	if (flags & O_CLOEXEC)
		__syscall(SYS_fcntl, fd, F_SETFD, FD_CLOEXEC);

	f = __fdopen(fd, mode);
	if (f) return f;

	certikos_puts("Not valid");

	#ifndef _CERTIKOS_
		__syscall(SYS_close, fd);
	#else
		//TODO add in close
	#endif

	return 0;
}
