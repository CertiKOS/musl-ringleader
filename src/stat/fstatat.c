#define _BSD_SOURCE
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/sysmacros.h>
#include "syscall.h"

#ifdef _CERTIKOS_
#include "certikos_impl.h"
#include "ringleader.h"
#include <limits.h>
#endif

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

#ifdef _CERTIKOS_

#ifdef SYS_fstatat
/* force statx to be used */
#undef SYS_fstatat
#endif



void *
musl_ringleader_statx_async(
		struct ringleader * rl,
		struct ringleader_arena * arena,
		int dirfd,
		char * shmem_path,
		int flag,
		unsigned int mask,
		void ** out_shmem_statxbuff)
{
	*out_shmem_statxbuff = ringleader_arena_push(arena, sizeof(struct statx));

	int id = ringleader_prep_statx(rl, dirfd, shmem_path, flag, mask,
			*out_shmem_statxbuff);

	void *cookie = musl_ringleader_set_cookie(rl, id);
	ringleader_submit(rl);

	return cookie;
}

int
musl_ringleader_statx(
		int dirfd,
		const char *restrict path,
		int flag,
		unsigned int mask,
		void *restrict statxbuff)
{
	struct ringleader *rl = get_ringleader();

	size_t arena_size = sizeof(struct statx);

	if(path != NULL)
	{
		arena_size += PATH_MAX;
	}

	struct ringleader_arena *arena =
		musl_ringleader_get_arena(rl, arena_size);

	char *shmem_path = NULL;
	if(path != NULL)
	{
		shmem_path = ringleader_arena_push(arena, PATH_MAX);
		strncpy(shmem_path, path, PATH_MAX);
	}

	void *shmem_statxbuff;
	void *cookie = musl_ringleader_statx_async(rl, arena, dirfd, shmem_path,
			flag, mask, &shmem_statxbuff);

	int ret = musl_ringleader_wait_result(rl, cookie);
	if(!ret)
	{
		memcpy(statxbuff, shmem_statxbuff, sizeof(struct statx));
	}

	ringleader_free_arena(rl, arena);
	return ret;
}
#endif

static int fstatat_statx(int fd, const char *restrict path, struct stat *restrict st, int flag)
{
	struct statx stx;

	int ret = __syscall(SYS_statx, fd, path, flag, 0x7ff, &stx);
	if (ret) return ret;

	*st = (struct stat){
		.st_dev = makedev(stx.stx_dev_major, stx.stx_dev_minor),
		.st_ino = stx.stx_ino,
		.st_mode = stx.stx_mode,
		.st_nlink = stx.stx_nlink,
		.st_uid = stx.stx_uid,
		.st_gid = stx.stx_gid,
		.st_rdev = makedev(stx.stx_rdev_major, stx.stx_rdev_minor),
		.st_size = stx.stx_size,
		.st_blksize = stx.stx_blksize,
		.st_blocks = stx.stx_blocks,
		.st_atim.tv_sec = stx.stx_atime.tv_sec,
		.st_atim.tv_nsec = stx.stx_atime.tv_nsec,
		.st_mtim.tv_sec = stx.stx_mtime.tv_sec,
		.st_mtim.tv_nsec = stx.stx_mtime.tv_nsec,
		.st_ctim.tv_sec = stx.stx_ctime.tv_sec,
		.st_ctim.tv_nsec = stx.stx_ctime.tv_nsec,
#if _REDIR_TIME64
		.__st_atim32.tv_sec = stx.stx_atime.tv_sec,
		.__st_atim32.tv_nsec = stx.stx_atime.tv_nsec,
		.__st_mtim32.tv_sec = stx.stx_mtime.tv_sec,
		.__st_mtim32.tv_nsec = stx.stx_mtime.tv_nsec,
		.__st_ctim32.tv_sec = stx.stx_ctime.tv_sec,
		.__st_ctim32.tv_nsec = stx.stx_ctime.tv_nsec,
#endif
	};
	return 0;
}

#ifdef SYS_fstatat

#include "kstat.h"

static int fstatat_kstat(int fd, const char *restrict path, struct stat *restrict st, int flag)
{
	int ret;
	struct kstat kst;

	if (flag==AT_EMPTY_PATH && fd>=0 && !*path) {
		ret = __syscall(SYS_fstat, fd, &kst);
		if (ret==-EBADF && __syscall(SYS_fcntl, fd, F_GETFD)>=0) {
			ret = __syscall(SYS_fstatat, fd, path, &kst, flag);
			if (ret==-EINVAL) {
				char buf[15+3*sizeof(int)];
				__procfdname(buf, fd);
#ifdef SYS_stat
				ret = __syscall(SYS_stat, buf, &kst);
#else
				ret = __syscall(SYS_fstatat, AT_FDCWD, buf, &kst, 0);
#endif
			}
		}
	}
#ifdef SYS_lstat
	else if ((fd == AT_FDCWD || *path=='/') && flag==AT_SYMLINK_NOFOLLOW)
		ret = __syscall(SYS_lstat, path, &kst);
#endif
#ifdef SYS_stat
	else if ((fd == AT_FDCWD || *path=='/') && !flag)
		ret = __syscall(SYS_stat, path, &kst);
#endif
	else ret = __syscall(SYS_fstatat, fd, path, &kst, flag);

	if (ret) return ret;

	*st = (struct stat){
		.st_dev = kst.st_dev,
		.st_ino = kst.st_ino,
		.st_mode = kst.st_mode,
		.st_nlink = kst.st_nlink,
		.st_uid = kst.st_uid,
		.st_gid = kst.st_gid,
		.st_rdev = kst.st_rdev,
		.st_size = kst.st_size,
		.st_blksize = kst.st_blksize,
		.st_blocks = kst.st_blocks,
		.st_atim.tv_sec = kst.st_atime_sec,
		.st_atim.tv_nsec = kst.st_atime_nsec,
		.st_mtim.tv_sec = kst.st_mtime_sec,
		.st_mtim.tv_nsec = kst.st_mtime_nsec,
		.st_ctim.tv_sec = kst.st_ctime_sec,
		.st_ctim.tv_nsec = kst.st_ctime_nsec,
#if _REDIR_TIME64
		.__st_atim32.tv_sec = kst.st_atime_sec,
		.__st_atim32.tv_nsec = kst.st_atime_nsec,
		.__st_mtim32.tv_sec = kst.st_mtime_sec,
		.__st_mtim32.tv_nsec = kst.st_mtime_nsec,
		.__st_ctim32.tv_sec = kst.st_ctime_sec,
		.__st_ctim32.tv_nsec = kst.st_ctime_nsec,
#endif
	};

	return 0;
}
#endif

int __fstatat(int fd, const char *restrict path, struct stat *restrict st, int flag)
{
	int ret;
#ifdef SYS_fstatat
	if (sizeof((struct kstat){0}.st_atime_sec) < sizeof(time_t)) {
		ret = fstatat_statx(fd, path, st, flag);
		if (ret!=-ENOSYS) return __syscall_ret(ret);
	}
	ret = fstatat_kstat(fd, path, st, flag);
#else
	ret = fstatat_statx(fd, path, st, flag);
#endif /* SYS_fstatat */
	return __syscall_ret(ret);
}

weak_alias(__fstatat, fstatat);
