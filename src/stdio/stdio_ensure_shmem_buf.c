#ifdef _CERTIKOS_
#include "stdio_impl.h"
#include "certikos_impl.h"
#include <errno.h>

int stdio_ensure_shmem_buf(FILE* f){
    if(f->buf == NULL && (f == stdin || f == stdout)){
        unsigned char *shmem = alloc_new_rl_shmem(BUFSIZ + UNGET);
        if(shmem == NULL){
            return -ENOMEM;
        }
        f->buf = shmem + UNGET;
        f->buf_size = BUFSIZ - UNGET;
    }
    return 0;
}

#endif 