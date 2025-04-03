#include <fcntl.h>
#include <stdarg.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include <ringleader.h>
#include <certikos.h>
#include <certikos/profile.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>

#ifndef STATX_TYPE
//TODO toolchain must include our built linux's headers
#define STATX_TYPE		0x00000001U	/* Want/got stx_mode & S_IFMT */
#define STATX_MODE		0x00000002U	/* Want/got stx_mode & ~S_IFMT */
#define STATX_NLINK		0x00000004U	/* Want/got stx_nlink */
#define STATX_UID		0x00000008U	/* Want/got stx_uid */
#define STATX_GID		0x00000010U	/* Want/got stx_gid */
#define STATX_ATIME		0x00000020U	/* Want/got stx_atime */
#define STATX_MTIME		0x00000040U	/* Want/got stx_mtime */
#define STATX_CTIME		0x00000080U	/* Want/got stx_ctime */
#define STATX_INO		0x00000100U	/* Want/got stx_ino */
#define STATX_SIZE		0x00000200U	/* Want/got stx_size */
#define STATX_BLOCKS		0x00000400U	/* Want/got stx_blocks */
#define STATX_BASIC_STATS	0x000007ffU	/* The stuff in the normal stat struct */
#define STATX_BTIME		0x00000800U	/* Want/got stx_btime */
#define STATX_MNT_ID		0x00001000U	/* Got stx_mnt_id */
#define STATX_DIOALIGN		0x00002000U	/* Want/got direct I/O alignment info */

#define STATX__RESERVED		0x80000000U	/* Reserved for future struct statx expansion */
struct statx {
	uint32_t stx_mask;
	uint32_t stx_blksize;
	uint64_t stx_attributes;
	uint32_t stx_nlink;
	uint32_t stx_uid;
	uint32_t stx_gid;
	uint16_t stx_mode;
	uint16_t pad1;
	uint64_t stx_ino;
uint64_t stx_size;
	uint64_t stx_blocks;
	uint64_t stx_attributes_mask;
	struct {
		int64_t tv_sec;
		uint32_t tv_nsec;
		int32_t pad;
	} stx_atime, stx_btime, stx_ctime, stx_mtime;
	uint32_t stx_rdev_major;
	uint32_t stx_rdev_minor;
	uint32_t stx_dev_major;
	uint32_t stx_dev_minor;
	uint64_t spare[14];
};
#endif /* STATX_TYPE (no statx defs) */


/* optimize regular file, pipe, socket writes/reads by asynchronous IO */





int musl_ringleader_openat(int dirfd, const char *filename, int flags, mode_t mode)
{
	struct ringleader *rl = get_ringleader();
	size_t arena_size = PATH_MAX;

	int do_stat = 0;
	if ((flags & O_WRONLY) && !(flags & O_DIRECTORY) && !(flags & O_PATH))
	{
		do_stat = 1;
		arena_size += PATH_MAX + sizeof(struct statx);
	}

	struct ringleader_arena * arena = musl_ringleader_get_arena(rl,
			arena_size);
	void * shmem_filename = ringleader_arena_push(arena, PATH_MAX);
	strncpy(shmem_filename, filename, PATH_MAX);

	int open_sqe_id = ringleader_prep_openat(rl, dirfd, shmem_filename, flags,
			mode);
	void * cookie = musl_ringleader_set_cookie(rl, open_sqe_id);
	ringleader_sqe_set_flags(rl, open_sqe_id, IOSQE_IO_LINK);
    ringleader_submit(rl);

	void * stat_cookie = NULL;
	void * shmem_statx = NULL;
	if(do_stat)
	{
		int statx_flags = (flags & O_NOFOLLOW) ? AT_SYMLINK_NOFOLLOW : 0;
		unsigned int statx_mask = (
			STATX_BLOCKS |
			STATX_SIZE |
			STATX_TYPE |
			STATX_MODE);
		stat_cookie = musl_ringleader_statx_async(rl, arena, dirfd,
				shmem_filename, statx_flags, statx_mask, &shmem_statx);
	}

    int ret = musl_ringleader_wait_result(rl, cookie);

	if(do_stat)
	{
		//TODO async this
		struct statx * stx = shmem_statx;
		int statx_ret = musl_ringleader_wait_result(rl, stat_cookie);
		if(statx_ret == 0 && ret >= 0)
		{
			if(S_ISREG(stx->stx_mode))
			{
				musl_rl_async_fd_init(ret, flags, mode, stx->stx_blksize,
						stx->stx_blocks, stx->stx_size);
			}
		}
	}

    ringleader_free_arena(rl, arena);

    return __syscall_ret(ret);
}
#endif

int openat(int fd, const char *filename, int flags, ...)
{
	mode_t mode = 0;

	if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}

	return syscall_cp(SYS_openat, fd, filename, flags|O_LARGEFILE, mode);
}
