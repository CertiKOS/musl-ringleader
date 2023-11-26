#include <unistd.h>
#include <fcntl.h>
#include "syscall.h"

int unlink(const char *path)
{
#ifndef _CERTIKOS_
#ifdef SYS_unlink
	return syscall(SYS_unlink, path);
#else
	return syscall(SYS_unlinkat, AT_FDCWD, path, 0);
#endif
#else
//shuttle to unlinkat which makes the ringleader call
return unlinkat(AT_FDCWD, path, 0);
#endif
}
