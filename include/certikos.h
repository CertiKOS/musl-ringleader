#ifndef _CERITKOS_H_
#define _CERTIKOS_H_
#include <stdlib.h>
#include <ringleader.h>


int sys_io_uring_setup(uint32_t entries, struct io_uring_params *p);

int certikos_puts(const char* buf);
int certikos_puts_length(const char* buf, int len);

int certikos_printf(const char *restrict fmt, ...);

#endif /* _CERTIKOS_H_ */
