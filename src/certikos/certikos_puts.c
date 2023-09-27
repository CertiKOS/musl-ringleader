#include "syscall.h"
#include <string.h>

int certikos_puts(const char* buf){
    int len = strlen(buf);
    return syscall(SYS_certikos_puts, 0, buf, len);
}