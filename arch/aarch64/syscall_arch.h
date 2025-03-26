#define __SYSCALL_LL_E(x) (x)
#define __SYSCALL_LL_O(x) (x)


#ifndef _CERTIKOS_

#define __asm_syscall(...) do { \
	__asm__ __volatile__ ( "svc 0" \
	: "=r"(x0) : __VA_ARGS__ : "memory", "cc"); \
	return x0; \
	} while (0)

static inline long __syscall0(long n)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0");
	__asm_syscall("r"(x8));
}

static inline long __syscall1(long n, long a)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	__asm_syscall("r"(x8), "0"(x0));
}

static inline long __syscall2(long n, long a, long b)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	__asm_syscall("r"(x8), "0"(x0), "r"(x1));
}

static inline long __syscall3(long n, long a, long b, long c)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	__asm_syscall("r"(x8), "0"(x0), "r"(x1), "r"(x2));
}

static inline long __syscall4(long n, long a, long b, long c, long d)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	__asm_syscall("r"(x8), "0"(x0), "r"(x1), "r"(x2), "r"(x3));
}

static inline long __syscall5(long n, long a, long b, long c, long d, long e)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	register long x4 __asm__("x4") = e;
	__asm_syscall("r"(x8), "0"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4));
}

static inline long __syscall6(long n, long a, long b, long c, long d, long e, long f)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	register long x4 __asm__("x4") = e;
	register long x5 __asm__("x5") = f;
	__asm_syscall("r"(x8), "0"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5));
}

#else
#include <certikos.h>

#define __asm_syscall(...) do { \
	__asm__ __volatile__ ( "svc 0" \
	: "=r"(x1) : __VA_ARGS__ : "memory", "cc"); \
	return x1; \
	} while (0)


static inline long __syscall0(long n)
{
#ifdef _CERTIKOS_
	/* MUSL RINGLEADER OVERRIDES */
	switch(n)
	{
        //use certikos pid
		//case SYS_getpid: return musl_ringleader_getpid();
		case SYS_getppid: return musl_ringleader_getppid();
		case SYS_sync: musl_ringleader_sync(); return 0;
		default: break;
	}
#endif
	register long x0 __asm__("x0") = n;
	register long x1 __asm__("x1");
	__asm_syscall("r"(x0));
}

static inline long __syscall1(long n, long a)
{
#ifdef _CERTIKOS_
	/* MUSL RINGLEADER OVERRIDES */
	switch(n)
	{
		case SYS_fsync: return musl_ringleader_fsync(a);
		case SYS_fdatasync: return musl_ringleader_fdatasync(a);
        case SYS_chdir: return musl_ringleader_chdir((void*)a);
		default: break;
	}
#endif

	register long x0 __asm__("x0") = n;
	register long x1 __asm__("x1") = a;
	__asm_syscall("r"(x0), "0"(x1));
}

static inline long __syscall2(long n, long a, long b)
{
#ifdef _CERTIKOS_
	/* MUSL RINGLEADER OVERRIDES */
	switch(n)
	{
		case SYS_pipe2: return musl_ringleader_pipe2((void*)a, b);
		default: break;
	}
#endif

	register long x0 __asm__("x0") = n;
	register long x1 __asm__("x1") = a;
	register long x2 __asm__("x2") = b;
	__asm_syscall("r"(x0), "0"(x1), "r"(x2));
}

static inline long __syscall3(long n, long a, long b, long c)
{
#ifdef _CERTIKOS_
	/* MUSL RINGLEADER OVERRIDES */
	switch(n)
	{
		case SYS_ioctl: return musl_ringleader_ioctl(a, b, (void*)c);
		case SYS_getdents64: return musl_ringleader_getdents(a, (void*)b, c);
		case SYS_fcntl: return musl_ringleader_fcntl(a, b, c);
		case SYS_openat: return musl_ringleader_openat(a, (void*)b, c, 0);
		case SYS_lseek: return musl_ringleader_lseek(a, b, c);
		case SYS_mkdirat: return musl_ringleader_mkdirat(a, (void*)b, c);
		case SYS_unlinkat: return musl_ringleader_unlinkat(a, (void*)b, c);
		case SYS_read: return musl_ringleader_read(a, (void*)b, c);
		case SYS_write: return musl_ringleader_write(a, (void*)b, c);
		default: break;
	}
#endif

	register long x0 __asm__("x0") = n;
	register long x1 __asm__("x1") = a;
	register long x2 __asm__("x2") = b;
	register long x3 __asm__("x3") = c;
	__asm_syscall("r"(x0), "r"(x1), "r"(x2), "r"(x3));
}

static inline long __syscall4(long n, long a, long b, long c, long d)
{
#ifdef _CERTIKOS_
	/* MUSL RINGLEADER OVERRIDES */
	switch(n)
	{
		case SYS_openat: return musl_ringleader_openat(a, (void*)b, c, d);
		default: break;
	}
#endif

	register long x0 __asm__("x0") = n;
	register long x1 __asm__("x1") = a;
	register long x2 __asm__("x2") = b;
	register long x3 __asm__("x3") = c;
	register long x4 __asm__("x4") = d;
	__asm_syscall("r"(x0), "0"(x1), "r"(x2), "r"(x3), "r"(x4));
}

static inline long __syscall5(long n, long a, long b, long c, long d, long e)
{
	register long x0 __asm__("x0") = n;
	register long x1 __asm__("x1") = a;
	register long x2 __asm__("x2") = b;
	register long x3 __asm__("x3") = c;
	register long x4 __asm__("x4") = d;
	register long x5 __asm__("x5") = e;
	__asm_syscall("r"(x0), "0"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5));
}

static inline long __syscall6(long n, long a, long b, long c, long d, long e, long f)
{
#ifdef _CERTIKOS_
	/* MUSL RINGLEADER OVERRIDES */
	switch(n)
	{
		case SYS_shutdown: return musl_ringleader_shutdown(a, b);
		default: break;
	}
#endif

	register long x0 __asm__("x0") = n;
	register long x1 __asm__("x1") = a;
	register long x2 __asm__("x2") = b;
	register long x3 __asm__("x3") = c;
	register long x4 __asm__("x4") = d;
	register long x5 __asm__("x5") = e;
	register long x6 __asm__("x6") = f;
	__asm_syscall("r"(x0), "0"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x6));
}

#endif /* _CERTIKOS_ */


#define VDSO_USEFUL
#define VDSO_CGT_SYM "__kernel_clock_gettime"
#define VDSO_CGT_VER "LINUX_2.6.39"

#define IPC_64 0
