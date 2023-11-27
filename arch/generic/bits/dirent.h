#define _DIRENT_HAVE_D_RECLEN
#define _DIRENT_HAVE_D_OFF
#define _DIRENT_HAVE_D_TYPE

struct dirent {
	ino_t d_ino;
	off_t d_off;
#ifdef _CERTIKOS_
    // Our version of linux uses one byte for reclen
	unsigned char d_reclen;
#else
	unsigned short d_reclen;
#endif
	unsigned char d_type;
	char d_name[256];
};
