#ifndef _CERTIKOS_H_
#define _CERTIKOS_H_
#include <stdlib.h>
#include <stdint.h>
#include <ringleader.h>

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

#endif /* _CERTIKOS_H_ */
