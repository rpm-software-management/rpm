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
/*@-exportfcn@*/
extern __const unsigned short int **__ctype_b_loc (void)
     __attribute__ ((__const)) /*@*/;
extern __const __int32_t **__ctype_tolower_loc (void)
     __attribute__ ((__const)) /*@*/;
extern __const __int32_t **__ctype_toupper_loc (void)
     __attribute__ ((__const)) /*@*/;
/*@=exportfcn@*/
/*@-exportvar@*/
/*@unchecked@*/
extern __const __int32_t *__ctype_tolower;
/*@unchecked@*/
extern __const __int32_t *__ctype_toupper;
/*@=exportvar@*/
/*@=declundef@*/
#endif

#include <ctype.h>

#if defined (__GLIBC__) && defined(__LCLINT__)
/*@-exportlocal@*/
extern int isalnum(int) __THROW	/*@*/;
extern int isalpha(int) __THROW	/*@*/;
extern int isascii(int) __THROW	/*@*/;
extern int iscntrl(int) __THROW	/*@*/;
extern int isdigit(int) __THROW	/*@*/;
extern int isgraph(int) __THROW	/*@*/;
extern int islower(int) __THROW	/*@*/;
extern int isprint(int) __THROW	/*@*/;
extern int ispunct(int) __THROW	/*@*/;
extern int isspace(int) __THROW	/*@*/;
extern int isupper(int) __THROW	/*@*/;
extern int isxdigit(int) __THROW	/*@*/;
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
#if defined(__LCLINT__)
/*@-declundef -exportfcn @*/
extern int getopt_long (int ___argc, char *const *___argv,
		const char *__shortopts, const struct option *__longopts,
		int * __longind)
	/*@*/;
/*@=declundef =exportfcn @*/
#endif
#include <getopt.h>     /* for long options (is this portable?)*/
#endif

#ifdef NEED_GETOPT
/*@unchecked@*/
extern int optind;		/* From getopt(3)			*/
/*@unchecked@*/
extern char *optarg;
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

/*@-declundef -exportfcn -incondefs @*/
/**
 */
/*@mayexit@*/ /*@only@*/ /*@out@*/
void * xmalloc (size_t size)
	/*@globals errno @*/
	/*@ensures maxSet(result) == (size - 1) @*/
	/*@modifies errno @*/;

/**
 */
/*@mayexit@*/ /*@only@*/
void * xcalloc (size_t nmemb, size_t size)
	/*@ensures maxSet(result) == (nmemb - 1) @*/
	/*@*/;

/**
 * @todo Annotate ptr with returned/out.
 */
/*@mayexit@*/ /*@only@*/
void * xrealloc (/*@null@*/ /*@only@*/ void * ptr, size_t size)
	/*@ensures maxSet(result) == (size - 1) @*/
	/*@modifies *ptr @*/;

/**
 */
/*@-fcnuse@*/
/*@mayexit@*/ /*@only@*/
char * xstrdup (const char *str)
	/*@*/;
/*@=fcnuse@*/
/*@=declundef =exportfcn=incondefs @*/

#if HAVE_MCHECK_H
#include <mcheck.h>
#if defined(__LCLINT__)
/*@-declundef -incondefs @*/ /* LCL: missing annotations */
extern int mcheck (void (*__abortfunc) (enum mcheck_status)) __THROW
	/*@globals internalState@*/
	/*@modifies internalState @*/;
extern int mcheck_pedantic (void (*__abortfunc) (enum mcheck_status)) __THROW
	/*@globals internalState@*/
	/*@modifies internalState @*/;
extern void mcheck_check_all (void)
	/*@globals internalState@*/
	/*@modifies internalState @*/;
extern enum mcheck_status mprobe (void *__ptr) __THROW
	/*@globals internalState@*/
	/*@modifies internalState @*/;
extern void mtrace (void) __THROW
	/*@globals internalState@*/
	/*@modifies internalState @*/;
extern void muntrace (void) __THROW
	/*@globals internalState@*/
	/*@modifies internalState @*/;
/*@=declundef =incondefs @*/
#endif /* defined(__LCLINT__) */
#endif	/* HAVE_MCHECK_H */

#if !defined(__LCLINT__)
/* Memory allocation via macro defs to get meaningful locations from mtrace() */
#define	xmalloc(_size) 		(malloc(_size) ? : vmefail(0))
#define	xcalloc(_nmemb, _size)	(calloc((_nmemb), (_size)) ? : vmefail(0))
#define	xrealloc(_ptr, _size)	(realloc((_ptr), (_size)) ? : vmefail(0))
#define	xstrdup(_str)	(strcpy(xmalloc(strlen(_str)+1), (_str)))
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
