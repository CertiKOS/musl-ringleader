#ifndef _CERTIKOS_IMPL_H
#define _CERTIKOS_IMPL_H

#define DEFAULT_CONSOLE_ID 0u

#include <certikos.h>
#include "stdio_impl.h"

#define SHMEM_SIZE          (0x4000)

#define SOCKET_COOKIE       (2841)
#define READV_COOKIE        (3142)
#define MKDIRAT_COOKIE      (5181)
#define CONNECT_COOKIE      (48821)
#define CLOSE_COOKIE        (132413)
#define OPENAT_COOKIE       (350652)
#define OPEN_COOKIE         (350653)
#define UNLINK_COOKIE       (1607102)
#define FCNTL_COOKIE        (1644911)
#define BIND_COOKIE         (1927814)
#define WRITE_COOKIE        (2020860)
#define STATX_COOKIE        (2150650)
#define ADVISE_COOKIE       (2241254)
#define FSYNC_COOKIE        (2324231)
#define SEND_COOKIE         (2445150)
#define RECV_COOKIE         (2740787)
#define ACCEPT_COOKIE       (3129646)
#define LISTEN_COOKIE       (3424060)
#define READ_COOKIE         (3705175)
#define IOCTL_COOKIE        (123897897)
#define LSEEK_COOKIE        (1234010010)
#define GETDENTS_COOKIE     (1238000297)


struct ringleader* get_ringleader(void);
int ringleader_allocated(void);
void *get_rl_shmem_singleton(void);
void *alloc_new_rl_shmem(int size);

struct ringleader_arena * musl_ringleader_get_arena(struct ringleader * rl, size_t size);


#endif
