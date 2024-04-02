#ifndef _CERTIKOS_H_
#define _CERTIKOS_H_
#include <stdlib.h>
#include <stdint.h>
#include <ringleader.h>
#include <dirent.h>
#include <certikos/profile.h>

/* Return the offset of 'member' relative to the beginning of a struct type */
#define offsetof(type, member)  __builtin_offsetof(type, member)

#define container_of(ptr, type, member)                       \
    ({                                                        \
        const __typeof__(((type*)0)->member)* __mptr = (ptr); \
        (type*)((char*)__mptr - offsetof(type, member));      \
    })


int sys_io_uring_setup(uint32_t entries, struct io_uring_params *p);

int certikos_puts(const char* buf);
int certikos_puts_length(const char* buf, int len);

int certikos_printf(const char *restrict fmt, ...);

int musl_ringleader_ioctl(int fd, int req, void* arg);
int musl_ringleader_getdents(int fd, struct dirent *buf, unsigned int len);
int musl_ringleader_fcntl(int fd, int cmd, long arg);
int musl_ringleader_openat(int fd, const char *filename, int flags, mode_t mode);
int musl_ringleader_lseek(int fd, off_t offset, int whence);
int musl_ringleader_fsync(int fd);
int musl_ringleader_fdatasync(int fd);
int musl_ringleader_mkdirat(int fd, const char *filename, mode_t mode);
int musl_ringleader_pipe2(int fd[2], int flags);
int musl_ringleader_shutdown(int fd, int how);
pid_t musl_ringleader_getpid(); // use certikos pid
pid_t musl_ringleader_getppid();
int musl_ringleader_unlinkat(int fd, const char *path, int flag);
void musl_ringleader_sync();

extern struct overheads_t musl_overheads;

#endif /* _CERTIKOS_H_ */
