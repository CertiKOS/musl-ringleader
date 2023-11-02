#ifndef _CERTIKOS_IMPL_H
#define _CERTIKOS_IMPL_H

#define DEFAULT_CONSOLE_ID 0u

#include <certikos.h>
#include "stdio_impl.h"

struct ringleader* get_ringleader(void);
void *get_rl_shmem_singleton(void);
void *alloc_new_rl_shmem(int size);

#endif