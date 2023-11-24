#ifndef _CERTIKOS_IMPL_H
#define _CERTIKOS_IMPL_H

#define DEFAULT_CONSOLE_ID 0u

#include <certikos.h>
#include "stdio_impl.h"

struct ringleader* get_ringleader(void);
int ringleader_allocated(void);
void *get_rl_shmem_singleton(void);
void *alloc_new_rl_shmem(int size);

//these do not support passing structs or more complex arguments at the moment.
int prep_musl_ringleader_fcntl(int link, uint64_t cookie, int fd, int cmd, int arg);
int reap_musl_ringleader_fcntl(int count, uint64_t *cookies);

#endif