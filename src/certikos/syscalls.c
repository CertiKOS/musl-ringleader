#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <ringleader.h>
#include <certikos.h>
#include "syscall.h"


int
sys_io_uring_setup(uint32_t entries, struct io_uring_params *p)
{
	ret = __syscall(SYS_io_uring_setup, entries, p);
    return __syscall_ret(ret);
}
