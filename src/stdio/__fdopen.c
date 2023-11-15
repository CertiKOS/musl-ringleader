#include "stdio_impl.h"
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "libc.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#endif

FILE *__fdopen(int fd, const char *mode)
{
	FILE *f;
	struct winsize wsz;

	/* Check for valid initial mode character */
	if (!strchr("rwa", *mode)) {
		errno = EINVAL;
		return 0;
	}

	/* Allocate FILE+buffer or fail */
	//TODO I am worried that there may be some assumptions somehwere that it is contiguous
	#ifndef _CERTIKOS_
	if (!(f=malloc(sizeof *f + UNGET + BUFSIZ))) return 0;
	#else
	if (!(f=malloc(sizeof *f + UNGET))) return 0;
	#endif

	/* Zero-fill only the struct, not the buffer */
	memset(f, 0, sizeof *f);

	/* Impose mode restrictions */
	if (!strchr(mode, '+')) f->flags = (*mode == 'r') ? F_NOWR : F_NORD;

	/* Apply close-on-exec flag */
	if (strchr(mode, 'e')) __syscall(SYS_fcntl, fd, F_SETFD, FD_CLOEXEC);

	/* Set append mode on fd if opened for append */
	if (*mode == 'a') {
		int flags = __syscall(SYS_fcntl, fd, F_GETFL);
		if (!(flags & O_APPEND))
			__syscall(SYS_fcntl, fd, F_SETFL, flags | O_APPEND);
		f->flags |= F_APP;
	}

	f->fd = fd;

	#ifndef _CERTIKOS_
	f->buf = (unsigned char *)f + sizeof *f + UNGET;
	#else
	void *buf = alloc_new_rl_shmem(BUFSIZ);
	if(buf == NULL){
		free(f);
		return 0;
	}
	f->buf = buf;
	#endif

	f->buf_size = BUFSIZ;

	/* Activate line buffered mode for terminals */
	#ifndef _CERTIKOS_
	f->lbf = EOF;
	if (!(f->flags & F_NOWR) && !__syscall(SYS_ioctl, fd, TIOCGWINSZ, &wsz))
		f->lbf = '\n'; 
	#endif

	/* Initialize op ptrs. No problem if some are unneeded. */
	//TODO free shmem
	f->read = __stdio_read;
	f->write = __stdio_write;
	f->seek = __stdio_seek;
    f->close = __stdio_close;

	if (!libc.threaded) f->lock = -1;

	/* Add new FILE to open file list */
	return __ofl_add(f);
}

weak_alias(__fdopen, fdopen);
