/**
 * \file system.h
 */

#ifndef	H_SYSTEM
#define	H_SYSTEM

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

/* <unistd.h> should be included before any preprocessor test
   of _POSIX_VERSION.  */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#if defined(__LCLINT__)
/*@-superuser -declundef -incondefs @*/	/* LCL: modifies clause missing */
extern int chroot (const char *__path)
	/*@globals errno, systemState @*/
	/*@modifies errno, systemState @*/;
/*@=superuser =declundef =incondefs @*/
#endif
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

#if NEED_TIMEZONE
extern time_t timezone;
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

#ifdef HAVE_UTIME_H
#include <utime.h>
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

#if !defined(HAVE_STPCPY)
char * stpcpy(/*@out@*/ char * dest, const char * src);
#endif

#if !defined(HAVE_STPNCPY)
char * stpncpy(/*@out@*/ char * dest, const char * src, size_t n);
#endif

#include <errno.h>
#ifndef errno
/*@-declundef @*/
extern int errno;
/*@=declundef @*/
#endif

#ifdef STDC_HEADERS
/*@-macrounrecog -incondefs -globuse -mustmod @*/ /* FIX: shrug */
#define getopt system_getopt
/*@=macrounrecog =incondefs =globuse =mustmod @*/
/*@-skipansiheaders@*/
#include <stdlib.h>
/*@=skipansiheaders@*/
#undef getopt
#if defined(__LCLINT__)
/*@-declundef -incondefs @*/	/* LCL: modifies clause missing */
extern char * realpath (const char * file_name, /*@out@*/ char * resolved_name)
	/*@globals errno, fileSystem @*/
	/*@requires maxSet(resolved_name) >=  (PATH_MAX - 1); @*/
	/*@modifies *resolved_name, errno, fileSystem @*/;
/*@=declundef =incondefs @*/
#endif
#else /* not STDC_HEADERS */
char *getenv (const char *name);
#if ! HAVE_REALPATH
char *realpath(const char *path, char resolved_path []);
#endif
#endif /* STDC_HEADERS */

/* XXX solaris2.5.1 has not */
#if !defined(EXIT_FAILURE)
#define	EXIT_FAILURE	1
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#else
#include <sys/file.h>
#endif

#if !defined(SEEK_SET) && !defined(__LCLINT__)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif
#if !defined(F_OK) && !defined(__LCLINT__)
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
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

#if defined(__LCLINT__)
/*@-declundef -incondefs @*/ /* LCL: missing annotation */
/*@only@*/ void * alloca (size_t __size)
	/*@ensures MaxSet(result) == (__size - 1) @*/
	/*@*/;
/*@=declundef =incondefs @*/
#endif

#ifdef __GNUC__
# undef alloca
# define alloca __builtin_alloca
#else
# ifdef HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifndef _AIX
/* AIX alloca decl has to be the first thing in the file, bletch! */
char *alloca ();
#  endif
# endif
#endif

#include <ctype.h>
#if defined (__GLIBC__) && defined(__LCLINT__)
/*@-declundef@*/
/*@unchecked@*/
extern __const __int32_t *__ctype_tolower;
/*@unchecked@*/
extern __const __int32_t *__ctype_toupper;
/*@=declundef@*/

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

#if HAVE_SYS_MMAN_H && !defined(__LCLINT__)
#include <sys/mman.h>
#endif

/* XXX FIXME: popt on sunos4.1.3: <sys/resource.h> requires <sys/time.h> */
#if HAVE_SYS_RESOURCE_H && HAVE_SYS_TIME_H
#include <sys/resource.h>
#endif

#if HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#if HAVE_GETOPT_H
/*@-noparams@*/
#include <getopt.h>
/*@=noparams@*/
#endif

#if HAVE_GRP_H
#include <grp.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_ERR_H
#include <err.h>
#endif

#if HAVE_MALLOC_H && !defined(__LCLINT__)
#include <malloc.h>
#endif

/*@-declundef -incondefs @*/ /* FIX: these are macros */
/**
 */
/*@mayexit@*/ /*@only@*/ /*@out@*/ void * xmalloc (size_t size)
	/*@globals errno @*/
	/*@ensures MaxSet(result) == (size - 1) @*/
	/*@modifies errno @*/;

/**
 */
/*@mayexit@*/ /*@only@*/ void * xcalloc (size_t nmemb, size_t size)
	/*@ensures MaxSet(result) == (nmemb - 1) @*/
	/*@*/;

/**
 * @todo Annotate ptr with returned/out.
 */
/*@mayexit@*/ /*@only@*/ void * xrealloc (/*@null@*/ /*@only@*/ void * ptr,
					size_t size)
	/*@ensures MaxSet(result) == (size - 1) @*/
	/*@modifies *ptr @*/;

/**
 */
/*@mayexit@*/ /*@only@*/ char * xstrdup (const char *str)
	/*@*/;
/*@=declundef =incondefs @*/

/**
 */
/*@unused@*/ /*@exits@*/ /*@only@*/ void * vmefail(size_t size)
	/*@*/;

#if HAVE_MCHECK_H
#include <mcheck.h>
#if defined(__LCLINT__)
/*@-declundef -incondefs @*/ /* LCL: missing annotations */
#if 0
enum mcheck_status
  {
    MCHECK_DISABLED = -1,       /* Consistency checking is not turned on.  */
    MCHECK_OK,                  /* Block is fine.  */
    MCHECK_FREE,                /* Block freed twice.  */
    MCHECK_HEAD,                /* Memory before the block was clobbered.  */
    MCHECK_TAIL                 /* Memory after the block was clobbered.  */
  };
#endif

extern int mcheck (void (*__abortfunc) (enum mcheck_status))
	/*@globals internalState@*/
	/*@modifies internalState @*/;
extern int mcheck_pedantic (void (*__abortfunc) (enum mcheck_status))
	/*@globals internalState@*/
	/*@modifies internalState @*/;
extern void mcheck_check_all (void)
	/*@globals internalState@*/
	/*@modifies internalState @*/;
extern enum mcheck_status mprobe (void *__ptr)
	/*@globals internalState@*/
	/*@modifies internalState @*/;
extern void mtrace (void)
	/*@globals internalState@*/
	/*@modifies internalState @*/;
extern void muntrace (void)
	/*@globals internalState@*/
	/*@modifies internalState @*/;
/*@=declundef =incondefs @*/
#endif /* defined(__LCLINT__) */

/* Memory allocation via macro defs to get meaningful locations from mtrace() */
#if defined(__GNUC__)
#define	xmalloc(_size) 		(malloc(_size) ? : vmefail(_size))
#define	xcalloc(_nmemb, _size)	(calloc((_nmemb), (_size)) ? : vmefail(_size))
#define	xrealloc(_ptr, _size)	(realloc((_ptr), (_size)) ? : vmefail(_size))
#define	xstrdup(_str)	(strcpy((malloc(strlen(_str)+1) ? : vmefail(strlen(_str)+1)), (_str)))
#endif	/* defined(__GNUC__) */
#endif	/* HAVE_MCHECK_H */

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
const char *__progname;

#if HAVE_NETDB_H
#include <netdb.h>
#endif

#if HAVE_PWD_H
#include <pwd.h>
#endif

/* Take care of NLS matters.  */

#if HAVE_LOCALE_H
# include <locale.h>
#endif
#if !HAVE_SETLOCALE
# define setlocale(Category, Locale) /* empty */
#endif

#if ENABLE_NLS && !defined(__LCLINT__)
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# undef bindtextdomain
# define bindtextdomain(Domain, Directory) /* empty */
# undef textdomain
# define textdomain(Domain) /* empty */
# define _(Text) Text
# undef dgettext
# define dgettext(DomainName, Text) Text
#endif

#define N_(Text) Text

/* ============== from misc/miscfn.h */

#if !defined(USE_GNU_GLOB)
#if HAVE_FNMATCH_H
/*@-noparams@*/
#include <fnmatch.h>
/*@=noparams@*/
#endif

#if HAVE_GLOB_H
/*@-noparams@*/
#include <glob.h>
/*@=noparams@*/
#endif
#else
/*@-noparams@*/
#include "misc/glob.h"
#include "misc/fnmatch.h"
/*@=noparams@*/
#endif

#if defined(__LCLINT__)
/*@-declundef -incondefs @*/ /* LCL: missing annotation */
#if 0
typedef /*@concrete@*/ struct
  {
    size_t gl_pathc;
    char **gl_pathv;
    size_t gl_offs;
    int gl_flags;

    void (*gl_closedir) (void *);
#ifdef _GNU_SOURCE
    struct dirent *(*gl_readdir) (void *);
#else
    void *(*gl_readdir) (void *);
#endif
    ptr_t (*gl_opendir) (const char *);
#ifdef _GNU_SOURCE
    int (*gl_lstat) (const char *restrict, struct stat *restrict);
    int (*gl_stat) (const char *restrict, struct stat *restrict);
#else
    int (*gl_lstat) (const char *restrict, void *restrict);
    int (*gl_stat) (const char *restrict, void *restrict);
#endif
  } glob_t;
#endif

#if 0
/*@-constuse@*/
/*@constant int GLOB_ERR@*/
/*@constant int GLOB_MARK@*/
/*@constant int GLOB_NOSORT@*/
/*@constant int GLOB_DOOFFS@*/
/*@constant int GLOB_NOCHECK@*/
/*@constant int GLOB_APPEND@*/
/*@constant int GLOB_NOESCAPE@*/
/*@constant int GLOB_PERIOD@*/

#ifdef _GNU_SOURCE
/*@constant int GLOB_MAGCHAR@*/
/*@constant int GLOB_ALTDIRFUNC@*/
/*@constant int GLOB_BRACE@*/
/*@constant int GLOB_NOMAGIC@*/
/*@constant int GLOB_TILDE@*/
/*@constant int GLOB_ONLYDIR@*/
/*@constant int GLOB_TILDE_CHECK@*/
#endif

/*@constant int GLOB_FLAGS@*/

/*@constant int GLOB_NOSPACE@*/
/*@constant int GLOB_ABORTED@*/
/*@constant int GLOB_NOMATCH@*/
/*@constant int GLOB_NOSYS@*/
#ifdef _GNU_SOURCE
/*@constant int GLOB_ABEND@*/
#endif
/*@=constuse@*/
#endif

extern int glob (const char *pattern, int flags,
                      int (*errfunc) (const char *, int),
                      /*@out@*/ glob_t *pglob)
	/*@globals errno, fileSystem @*/
	/*@modifies *pglob, errno, fileSystem @*/;
	/* XXX only annotation is a white lie */
extern void globfree (/*@only@*/ glob_t *pglob)
	/*@modifies *pglob @*/;
#ifdef _GNU_SOURCE
extern int glob_pattern_p (const char *pattern, int quote)
	/*@*/;
#endif

#if 0
/*@-constuse@*/
/*@constant int FNM_PATHNAME@*/
/*@constant int FNM_NOESCAPE@*/
/*@constant int FNM_PERIOD@*/

#ifdef _GNU_SOURCE
/*@constant int FNM_FILE_NAME@*/	/* GNU extension */
/*@constant int FNM_LEADING_DIR@*/	/* GNU extension */
/*@constant int FNM_CASEFOLD@*/		/* GNU extension */
/*@constant int FNM_EXTMATCH@*/		/* GNU extension */
#endif

/*@constant int FNM_NOMATCH@*/

#ifdef _XOPEN_SOURCE
/*@constant int FNM_NOSYS@*/		/* X/Open */
#endif
/*@=constuse@*/
#endif

extern int fnmatch (const char *pattern, const char *string, int flags)
	/*@*/;
/*@=declundef =incondefs @*/
#endif

#if ! HAVE_S_IFSOCK
#define S_IFSOCK (0xc000)
#endif

#if ! HAVE_S_ISLNK
#define S_ISLNK(mode) ((mode & 0xf000) == S_IFLNK)
#endif

#if ! HAVE_S_ISSOCK
#define S_ISSOCK(mode) ((mode & 0xf000) == S_IFSOCK)
#endif

#if NEED_STRINGS_H
#include <strings.h>
#endif

#if NEED_MYREALLOC
#define realloc(ptr,size) myrealloc(ptr,size)
extern void *myrealloc(void *, size_t);
#endif

#if ! HAVE_SETENV
extern int setenv(const char *name, const char *value, int replace);
extern void unsetenv(const char *name);
#endif

#if HAVE_SYS_SOCKET_H
#include <sys/types.h>
#include <sys/socket.h>
#endif

#if HAVE_SYS_SELECT_H && !defined(__LCLINT__)
#include <sys/select.h>
#endif

/* Solaris <= 2.6 limits getpass return to only 8 chars */
#if HAVE_GETPASSPHRASE
#define	getpass	getpassphrase
#endif

#if ! HAVE_LCHOWN
#define lchown chown
#endif

#if HAVE_GETMNTINFO_R || HAVE_MNTCTL
# define GETMNTENT_ONE 0
# define GETMNTENT_TWO 0
# if HAVE_SYS_MNTCTL_H
#  include <sys/mntctl.h>
# endif
# if HAVE_SYS_VMOUNT_H
#  include <sys/vmount.h>
# endif
# if HAVE_SYS_MOUNT_H
#  include <sys/mount.h>
# endif
#elif HAVE_MNTENT_H || !(HAVE_GETMNTENT) || HAVE_STRUCT_MNTTAB
# if HAVE_MNTENT_H
#  include <stdio.h>
#  include <mntent.h>
#  define our_mntent struct mntent
#  define our_mntdir mnt_dir
# elif HAVE_STRUCT_MNTTAB
#  include <stdio.h>
#  include <mnttab.h>
   struct our_mntent {
       char * our_mntdir;
   };
   struct our_mntent *getmntent(FILE *filep);
#  define our_mntent struct our_mntent
# else
#  include <stdio.h>
   struct our_mntent {
       char * our_mntdir;
   };
   struct our_mntent *getmntent(FILE *filep);
#  define our_mntent struct our_mntent
# endif
# define GETMNTENT_ONE 1
# define GETMNTENT_TWO 0
#elif HAVE_SYS_MNTTAB_H
# include <stdio.h>
# include <sys/mnttab.h>
# define GETMNTENT_ONE 0
# define GETMNTENT_TWO 1
# define our_mntent struct mnttab
# define our_mntdir mnt_mountp
#else /* if !HAVE_MNTCTL */
# error Neither mntent.h, mnttab.h, or mntctl() exists. I cannot build on this system.
#endif

#ifndef MOUNTED
#define MOUNTED "/etc/mnttab"
#endif
#endif	/* H_SYSTEM */
