#include "certikos_impl.h"
#include "ringleader.h"

int prep_musl_ringleader_fcntl(int link, uint64_t cookie, int fd, int cmd, int arg){
    struct ringleader *rl = get_ringleader();
    int32_t id = ringleader_prep_fcntl(rl, fd, cmd, arg);
    ringleader_set_user_data(rl, id, (void *) cookie);

    if(link) ringleader_sqe_set_flags(rl, id, IOSQE_IO_LINK);

    return id;
}

//return number reaped
int reap_musl_ringleader_fcntl(int count, uint64_t *cookies){
    struct ringleader *rl = get_ringleader();

    struct io_uring_cqe *cqe;
    int ret;
    for(int i = 0; i < count; i++){
        cqe = ringleader_get_cqe(rl);
        if((uint64_t) cqe->user_data == cookies[i]){
            ret = cqe->res;
            ringleader_consume_cqe(rl, cqe);
        } else {
            ringleader_consume_cqe(rl, cqe);
            certikos_puts("Did not get expected ringleader fcntl completion token");
            return -1;
        }
    }
    return count;
}