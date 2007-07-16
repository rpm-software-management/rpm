/*-
 * $Id: win_db.in,v 12.19 2006/06/19 15:56:39 bostic Exp $
 *
 * The following provides the information necessary to build Berkeley
 * DB on native Windows, and other Windows environments such as MinGW.
 */

/*
 * Avoid warnings with Visual Studio 8.
 */
#define	_CRT_SECURE_NO_DEPRECATE 1

/*
 * Windows NT 4.0 and later required for the replication manager.
 */
#ifdef HAVE_REPLICATION_THREADS
#define	_WIN32_WINNT 0x0400
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/timeb.h>

#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
#include <memory.h>
#include <process.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <time.h>

/*
 * To build Tcl interface libraries, the include path must be configured to
 * use the directory containing <tcl.h>, usually the include directory in
 * the Tcl distribution.
 */
#ifdef DB_TCL_SUPPORT
#include <tcl.h>
#endif

#define	WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>

#ifdef HAVE_GETADDRINFO
/*
 * Need explicit includes for IPv6 support on Windows.  Both are necessary to
 * ensure that pre WinXP versions have an implementation of the getaddrinfo API.
 */
#include <ws2tcpip.h>
#include <wspiapi.h>
#endif

/*
 * All of the necessary includes have been included, ignore the #includes
 * in the Berkeley DB source files.
 */
#define	NO_SYSTEM_INCLUDES

/*
 * Microsoft's C runtime library has fsync, getcwd, getpid, snprintf and
 * vsnprintf, but under different names.
 */
#define	fsync			_commit
#define	getcwd(buf, size)	_getcwd(buf, size)
#define	getpid			_getpid
#define	snprintf		_snprintf
#define	vsnprintf		_vsnprintf

#define	h_errno			WSAGetLastError()

/*
 * Windows defines off_t to long (i.e., 32 bits).  We need to pass 64-bit
 * file offsets, so we declare our own.
 */
#define	off_t	__db_off_t
typedef __int64 off_t;

/*
 * Win32 does not have getopt.
 *
 * The externs are here (instead of using db_config.h and clib_port.h),
 * because Berkeley DB example programs use getopt and they can't #include
 * those files.
 */
#if defined(__cplusplus)
extern "C" {
#endif
extern int getopt(int, char * const *, const char *);
#if defined(__cplusplus)
}
#endif

#ifdef _UNICODE
#define	TO_TSTRING(dbenv, s, ts, ret) do {				\
		int __len = (int)strlen(s) + 1;				\
		ts = NULL;						\
		if ((ret = __os_malloc((dbenv),				\
		    __len * sizeof(_TCHAR), &(ts))) == 0 &&		\
		    MultiByteToWideChar(CP_UTF8, 0,			\
		    (s), -1, (ts), __len) == 0)				\
			ret = __os_posix_err(__os_get_syserr());	\
	} while (0)

#define	FROM_TSTRING(dbenv, ts, s, ret) {				\
		int __len = WideCharToMultiByte(CP_UTF8, 0, ts, -1,	\
		    NULL, 0, NULL, NULL);				\
		s = NULL;						\
		if ((ret = __os_malloc((dbenv), __len, &(s))) == 0 &&	\
		    WideCharToMultiByte(CP_UTF8, 0,			\
		    (ts), -1, (s), __len, NULL, NULL) == 0)		\
			ret = __os_posix_err(__os_get_syserr());	\
	} while (0)

#define	FREE_STRING(dbenv, s) do {					\
		if ((s) != NULL) {					\
			__os_free((dbenv), (s));			\
			(s) = NULL;					\
		}							\
	} while (0)

#else
#define	TO_TSTRING(dbenv, s, ts, ret) (ret) = 0, (ts) = (_TCHAR *)(s)
#define	FROM_TSTRING(dbenv, ts, s, ret) (ret) = 0, (s) = (char *)(ts)
#define	FREE_STRING(dbenv, ts)
#endif

#ifndef INVALID_HANDLE_VALUE
#define	INVALID_HANDLE_VALUE ((HANDLE)-1)
#endif

#ifndef INVALID_FILE_ATTRIBUTES
#define	INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

#ifndef INVALID_SET_FILE_POINTER
#define	INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif
