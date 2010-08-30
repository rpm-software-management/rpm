/**
 * \file system.h
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

#if HAVE___SECURE_GETENV
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

#if WITH_SELINUX
#include <selinux/selinux.h>
#include <selinux/label.h>
#else
typedef	char * security_context_t;

#define	freecon(_c)

#define	setfilecon(_fn, _c)	(-1)
#define	lsetfilecon(_fn, _c)	(-1)

#define	is_selinux_enabled()	(0)

#define matchpathcon_init(_fn)			(-1)
#define matchpathcon_fini()			(0)
#define matchpathcon(_fn, _fm, _c)		(-1)

#define selabel_lookup_raw(_hnd, _scon, _key,_type)	(-1)

#define selinux_file_context_path() (0)

#define rpm_execcon(_v, _fn, _av, _envp)	(0)
#endif

#include "rpmio/rpmutil.h"
/* compatibility macros to avoid a mass-renaming all over the codebase */
#define xmalloc(_size) rmalloc((_size))
#define xcalloc(_nmemb, _size) rcalloc((_nmemb), (_size))
#define xrealloc(_ptr, _size) rrealloc((_ptr), (_size))
#define xstrdup(_str) rstrdup((_str))
#define _free(_ptr) rfree((_ptr))

/* Retrofit glibc __progname */
#if defined __GLIBC__ && __GLIBC__ >= 2
#if __GLIBC_MINOR__ >= 1
#define	__progname	__assert_program_name
#endif
#define	setprogname(pn)
#else
#define	__progname	program_name
#define	setprogname(pn)	\
  { if ((__progname = strrchr(pn, '/')) != NULL) __progname++; \
    else __progname = pn;		\
  }
#endif
extern const char *__progname;

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

#if !defined(USE_GNU_GLOB) 
#if HAVE_FNMATCH_H
#include <fnmatch.h>
#endif

#if HAVE_GLOB_H 
#include <glob.h>
#endif
#else
#include "misc/glob.h"
#include "misc/fnmatch.h"
#endif

#include <dlfcn.h>

#endif	/* H_SYSTEM */
