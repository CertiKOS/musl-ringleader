

//struct musl_rl_io {
//	int fd;
//	const char *filename;
//	int flags;
//	mode_t mode;
//
//	void *shmem;
//	size_t shmem_size;
//	struct ringleader_arena * arena;
//};
//
//
//err_t
//musl_ringleader_arena_callback(struct ringleader *rl, struct io_uring_cqe *cqe)
//{
//    struct ringleader_arena *arena;
//
//    if(ringleader_init_arena(rl, cqe, &arena) != ERR_OK)
//    {
//        certikos_printf("Failed to init arena\n");
//    } else {
//        ringleader_free_arena(rl, arena);
//    }
//
//    ringleader_consume_cqe(rl, cqe);
//    return ERR_OK_CONSUMED;
//}
//
//
//
//void
//musl_rl_io_stat(
//		struct ringleader *rl,
//		const char *shmem_path,
//		ringleader_arena * arena)
//{
//
//		int32_t id = ringleader_prep_statx(rl, fd, shmem, flag, 0x7ff, shmem_st_start);
//		ringleader_set_user_data(rl, id, (void *) STATX_COOKIE);
//		ringleader_submit(rl);
//
//}
//
//
//void
//musl_rl_io_openat() {
//}
