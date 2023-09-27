#include "syscall.h"
#include "certikos_impl.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

int certikos_printf(const char *restrict fmt, ...){
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = vfprintf(&certikos_stdout, fmt, ap);
    va_end(ap);
    return ret;
}