/*
 * $Id: bench.h,v 1.13 2007/06/15 18:44:23 bostic Exp $
 */
#include "db_config.h"

#include <sys/types.h>
#include <sys/stat.h>

#ifndef DB_WIN32
#include <sys/time.h>

#include <unistd.h>
#include <stdint.h>
#else
#define	WIN32_LEAN_AND_MEAN	1
#include <windows.h>
#include <direct.h>
#include <sys/timeb.h>
extern int getopt(int, char * const *, const char *);
extern char *optarg;
extern int optind;
#define	snprintf _snprintf
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"

/*
 * Implement a custom assert to allow consistent behavior across builds and
 * platforms.
 *
 * The BDB library DB_ASSERT implementation is only enabled in diagnostic
 * builds -- so is not suitable here.
 */
#define	DB_BENCH_ASSERT(e) do {						\
	(e) ? 0 : (fprintf(stderr, "ASSERT FAILED: %s\n", #e), abort());\
} while (0)

#ifndef	NS_PER_SEC
#define	NS_PER_SEC	1000000000	/* Nanoseconds in a second */
#endif
#ifndef	NS_PER_US
#define	NS_PER_US	1000		/* Nanoseconds in a microsecond */
#endif
#ifndef	MS_PER_NS
#define	MS_PER_NS	1000000		/* Milliseconds in a nanosecond */
#endif

/*
 * Use the timer routines in the Berkeley DB library after their conversion
 * to POSIX timespec interfaces.
 */
#ifdef DB_TIMEOUT_TO_TIMESPEC
#define	TIMER_START	__os_gettime(NULL, &__start_time)
#define	TIMER_STOP	__os_gettime(NULL, &__end_time)
#else
typedef struct {
	time_t	tv_sec;				/* seconds */
	long	tv_nsec;			/* nanoseconds */
} db_timespec;

#define	timespecadd(vvp, uvp)						\
	do {								\
		(vvp)->tv_sec += (uvp)->tv_sec;				\
		(vvp)->tv_nsec += (uvp)->tv_nsec;			\
		if ((vvp)->tv_nsec >= NS_PER_SEC) {			\
			(vvp)->tv_sec++;				\
			(vvp)->tv_nsec -= NS_PER_SEC;			\
		}							\
	} while (0)
#define	timespecsub(vvp, uvp)						\
	do {								\
		(vvp)->tv_sec -= (uvp)->tv_sec;				\
		(vvp)->tv_nsec -= (uvp)->tv_nsec;			\
		if ((vvp)->tv_nsec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_nsec += NS_PER_SEC;			\
		}							\
	} while (0)

#define	TIMER_START	CLOCK(__start_time)
#define	TIMER_STOP	CLOCK(__end_time)

#if defined(HAVE_CLOCK_GETTIME)
#define	CLOCK(t) do {							\
	DB_BENCH_ASSERT(						\
	    clock_gettime(CLOCK_REALTIME, (struct timespec *)&t) == 0);	\
} while (0)
#elif defined(DB_WIN32)
#define	CLOCK(t) do {							\
	struct _timeb __now;						\
	_ftime(&__now);							\
	t.tv_sec = __now.time;						\
	t.tv_nsec = __now.millitm * MS_PER_NS;				\
} while (0)
#else
#define	CLOCK(t) do {							\
	struct timeval __tp;						\
	DB_BENCH_ASSERT(gettimeofday(&__tp, NULL) == 0);		\
	t.tv_sec = __tp.tv_sec;						\
	t.tv_nsec = __tp.tv_usec * NS_PER_US;				\
} while (0)
#endif
#endif /* DB_TIMEOUT_TO_TIMESPEC */

db_timespec __start_time, __end_time;

#define	TIMER_GET(t) do {						\
	t = __end_time;							\
	timespecsub(&t, &__start_time);					\
} while (0)
#define	TIMER_DISPLAY(ops) do {						\
	db_timespec __tmp_time;						\
	__tmp_time = __end_time;					\
	timespecsub(&__tmp_time, &__start_time);			\
	TIME_DISPLAY(ops, __tmp_time);					\
} while (0)
#define	TIME_DISPLAY(ops, t) do {					\
	double __secs;							\
	int __major, __minor, __patch;					\
	__secs = t.tv_sec + (double)t.tv_nsec / NS_PER_SEC;		\
	(void)db_version(&__major, &__minor, &__patch);			\
	printf("%d.%d.%d\t%.2f\n", __major, __minor, __patch,		\
	    (__secs == 0) ? 0.0 : (ops) / __secs);			\
} while (0)

/*
 * Cleanup the test directory.
 */
void
cleanup_test_dir()
{
#ifndef DB_WIN32
	(void)system("rm -rf a TESTDIR; mkdir TESTDIR");
#else
	(void)system("RMDIR TESTDIR /s /q");
	/*
	 * Windows outputs a message if I delete this via the system
	 * DEL function, so use unlink.
	 */
	_unlink("a");
	_mkdir("TESTDIR");
	SetLastError(0);
#endif
}
