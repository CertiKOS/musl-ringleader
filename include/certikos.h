#ifndef _CERITKOS_H_
#define _CERTIKOS_H_
#include <stdlib.h>
#include <ringleader.h>


int sys_io_uring_setup(uint32_t entries, struct io_uring_params *p);

#endif /* _CERTIKOS_H_ */
