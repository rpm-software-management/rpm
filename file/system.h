#ifndef	H_SYSTEM
#define	H_SYSTEM

#ifndef __linux__
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE 
#define _FILE_OFFSET_BITS 64
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <sys/stat.h>
#include <stdio.h>

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
#else
# include <strings.h>
char *memchr ();
#endif

#include <errno.h>
#ifndef errno
/*@-declundef @*/
extern int errno;
/*@=declundef @*/
#endif

#ifdef STDC_HEADERS
#include <stdlib.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdarg.h>
#else
#include <varargs.h>
#endif /* STDC_HEADERS */

#if defined (__GLIBC__) && defined(__LCLINT__)
/*@-declundef@*/
/*@unchecked@*/
extern __const __int32_t *__ctype_tolower;
/*@unchecked@*/
extern __const __int32_t *__ctype_toupper;
/*@=declundef@*/
#endif

#include <ctype.h>

#if defined (__GLIBC__) && defined(__LCLINT__)
/*@-exportlocal@*/
extern int isalnum(int) __THROW	/*@*/;
extern int iscntrl(int) __THROW	/*@*/;
extern int isgraph(int) __THROW	/*@*/;
extern int islower(int) __THROW	/*@*/;
extern int ispunct(int) __THROW	/*@*/;
extern int isxdigit(int) __THROW	/*@*/;
extern int isascii(int) __THROW	/*@*/;
extern int toascii(int) __THROW	/*@*/;
extern int _toupper(int) __THROW	/*@*/;
extern int _tolower(int) __THROW	/*@*/;
/*@=exportlocal@*/
#endif

/* XXX solaris2.5.1 has not */
#if !defined(EXIT_FAILURE)
#define	EXIT_FAILURE	1
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else
#include <sys/file.h>
#endif

#if HAVE_SYS_MMAN_H && !defined(__LCLINT__)
#include <sys/mman.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef RESTORE_TIME
# if (__COHERENT__ >= 0x420)
#  include <sys/utime.h>
# else
#  ifdef USE_UTIMES
#   include <sys/time.h>
#  else
#   include <utime.h>
#  endif
# endif
#endif

/* Since major is a function on SVR4, we can't use `ifndef major'.  */
#if MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#define HAVE_MAJOR
#endif
#if MAJOR_IN_SYSMACROS
#include <sys/sysmacros.h>
#define HAVE_MAJOR
#endif
#ifdef major			/* Might be defined in sys/types.h.  */
#define HAVE_MAJOR
#endif

#ifndef HAVE_MAJOR
#define major(dev)  (((dev) >> 8) & 0xff)
#define minor(dev)  ((dev) & 0xff)
#define makedev(maj, min)  (((maj) << 8) | (min))
#endif
#undef HAVE_MAJOR

#ifdef HAVE_GETOPT_H 
#include <getopt.h>     /* for long options (is this portable?)*/
#endif

#if HAVE_REGEX_H
#include <regex.h>
#endif

#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

#ifndef HAVE_STRERROR
/*@unchecked@*/
extern int sys_nerr;
/*@unchecked@*/
extern char *sys_errlist[];
#define strerror(e) \
	(((e) >= 0 && (e) < sys_nerr) ? sys_errlist[(e)] : "Unknown error")
#endif

#ifndef HAVE_STRTOUL
#define strtoul(a, b, c)	strtol(a, b, c)
#endif

#if HAVE_LOCALE_H
# include <locale.h>
#endif

#if defined(__LCLINT__)
#define FILE_RCSID(id)
#else
#define FILE_RCSID(id) \
static inline const char *rcsid(const char *p) { \
	return rcsid(p = id); \
}
#endif

#endif	/* H_SYSTEM */
