#ifndef _CERTIKOS_IMPL_H
#define _CERTIKOS_IMPL_H

#define DEFAULT_CONSOLE_ID 0u

#include "stdio_impl.h"

static FILE certikos_stdout = {.flags = F_ENCLAVE_TERMINAL};

#endif