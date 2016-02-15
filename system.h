/**
 * \file system.h
 *
 *  Some misc low-level API
 */

#ifndef	H_SYSTEM
#define	H_SYSTEM

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

/* <unistd.h> should be included before any preprocessor test
   of _POSIX_VERSION.  */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#if !defined(__GLIBC__)
#ifdef __APPLE__
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern char ** environ;
#endif /* __APPLE__ */
#endif
#endif

#if !defined(HAVE_STPCPY)
char * stpcpy(char * dest, const char * src);
#endif

#if !defined(HAVE_STPNCPY)
char * stpncpy(char * dest, const char * src, size_t n);
#endif

#if HAVE_SECURE_GETENV
#define	getenv(_s)	secure_getenv(_s)
#elif HAVE___SECURE_GETENV
#define	getenv(_s)	__secure_getenv(_s)
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else
#include <sys/file.h>
#endif

#ifdef HAVE_DIRENT_H
# include <dirent.h>
# define NLENGTH(direct) (strlen((direct)->d_name))
#else /* not HAVE_DIRENT_H */
# define dirent direct
# define NLENGTH(direct) ((direct)->d_namlen)
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif /* HAVE_SYS_NDIR_H */
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif /* HAVE_SYS_DIR_H */
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif /* HAVE_NDIR_H */
#endif /* HAVE_DIRENT_H */

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#ifndef PATH_MAX
#ifdef _POSIX_PATH_MAX
#define PATH_MAX _POSIX_PATH_MAX
#elif defined MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX 256
#endif
#endif

#if defined(HAVE_FDATASYNC) && !HAVE_DECL_FDATASYNC
extern int fdatasync(int fildes);
#endif

#include "rpmio/rpmutil.h"
/* compatibility macros to avoid a mass-renaming all over the codebase */
#define xmalloc(_size) rmalloc((_size))
#define xcalloc(_nmemb, _size) rcalloc((_nmemb), (_size))
#define xrealloc(_ptr, _size) rrealloc((_ptr), (_size))
#define xstrdup(_str) rstrdup((_str))
#define _free(_ptr) rfree((_ptr))

/* To extract program's name: use calls (reimplemented or shipped with system):
   - void setprogname(const char *pn)
   - const char *getprogname(void)

   setprogname(*pn) must be the first call in main() in order to set the name
   as soon as possible. */
#if defined(__NetBSD__) || defined(__APPLE__)
/* `man setprogname' v. May 21, 2011 (NetBSD 6.1) says:
   Portable programs that wish to use getprogname() should call setprogname()
   in main(). In some systems (like NetBSD) it can be set only once and it is
   done before an execution of main() -- therefore calling it again has no
   effect.

   Getprogname and setprogname function calls appeared in NetBSD 1.6
   (released in 2002). So nothing to (re)implement for this platform. */
# include <stdlib.h> /* Make sure this header is included */
# define xsetprogname(pn) setprogname(pn)
# define xgetprogname(pn) getprogname(pn)
#elif defined(__GLIBC__) /* GNU LIBC ships with (const *char *) __progname */
# define xsetprogname(pn) /* No need to implement it in GNU LIBC. */
  extern const char *__progname;
# define xgetprogname(pn) __progname
#else /* Reimplement setprogname and getprogname */
# include "misc/rpmxprogname.h"
# define xsetprogname(pn) _rpmxsetprogname(pn)
# define xgetprogname() _rpmxgetprogname()
#endif

/* Take care of NLS matters.  */
#if ENABLE_NLS
# include <locale.h>
# include <libintl.h>
# define _(Text) dgettext (PACKAGE, Text)
#else
# define _(Text) Text
#endif

#define N_(Text) Text

/* ============== from misc/miscfn.h */

#include "misc/fnmatch.h"

#include <dlfcn.h>

#endif	/* H_SYSTEM */
