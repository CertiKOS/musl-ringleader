#include "syscall.h"
#include "certikos_impl.h"
#include <string.h>

//TODO: replace magic 0
int certikos_puts(const char* buf){
    int len = strlen(buf);
    return syscall(SYS_certikos_puts, DEFAULT_CONSOLE_ID, buf, len);
}

int certikos_puts_length(const char* buf, int len){
    return syscall(SYS_certikos_puts, DEFAULT_CONSOLE_ID, buf, len);
}