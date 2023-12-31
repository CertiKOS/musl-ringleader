#include "syscall.h"
#include "certikos_impl.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

FILE certikos_stdout = {.flags = F_ENCLAVE_TERMINAL};
FILE *const stdenclave = &certikos_stdout;

int certikos_printf(const char *restrict fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = vfprintf(stdenclave, fmt, ap);
    va_end(ap);
    return ret;
}
