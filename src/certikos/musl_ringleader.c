#include "certikos_impl.h"
#include "syscall.h"
#include "ringleader.h"
#include <stdatomic.h>

#define MIN_ENTRIES 16
#define SHMEM_SIZE 0x4000

uint64_t shmem_cookie = 0u;

//NULL RETURNS INDICATE ERRORS

struct ringleader* get_ringleader(void){
    static struct ringleader * rl = NULL;

    if(rl != NULL){
        return rl;
    }
    rl = ringleader_factory(MIN_ENTRIES);
    err_t err;
    while((err = ringleader_init_finished(rl)) == ERR_NOT_READY) {}

    if(err != ERR_OK){
        certikos_printf("Failed to start ringleader, error %d", err);
        return NULL;
    }

    return rl;
}

//TODO: modify this to be more dynamic and allow more than 4kb of memory using shmem malloc when that is completed

// TODO: user data reprsents waht type of io_uring mapping is done, for now use
// IORING_OFF_SQ_RING =0
// this is passed to the mmap offset
void *get_rl_shmem(void){
    struct ringleader* rl = get_ringleader();

    if(ringleader_request_shmem(rl, SHMEM_SIZE, (void*) shmem_cookie) != ERR_OK) {
        return NULL;
    }
    struct io_uring_cqe *cqe = ringleader_get_cqe(rl); 
    if((uint64_t) cqe->user_data == shmem_cookie){
        if(ringleader_add_shmem(rl, cqe, SHMEM_SIZE) == ERR_OK){
            ringleader_consume_cqe(rl, cqe);                   
            return rl->shmem[0].addr;
        } else {
            ringleader_consume_cqe(rl, cqe);                  
            return NULL;
        }
    } else {
        certikos_puts("Did not get expected ringleader completion token");
        exit(-1);
    }
}
