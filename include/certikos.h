#include <unistd.h>
#include <stdlib.h>
#include <io_uring_common.h>

int
sys_io_uring_setup(uint32_t entries, struct io_uring_params *p)
{
    long ret = __syscall(SYS_io_uring_setup, entries, p);
    return __syscall_ret(ret);
}
