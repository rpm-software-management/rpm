#ifndef	H_RPMIO
#define	H_RPMIO

/** \ingroup rpmio
 * \file rpmio/rpmio.h
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "glob.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
typedef struct pgpDig_s * pgpDig;

/**
 */
typedef struct pgpDigParams_s * pgpDigParams;

/** \ingroup rpmio
 * Hide libio API lossage.
 * The libio interface changed after glibc-2.1.3 to pass the seek offset
 * argument as a pointer rather than as an off_t. The snarl below defines
 * typedefs to isolate the lossage.
 */
#if defined(__GLIBC__) && \
	(__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 2))
#define USE_COOKIE_SEEK_POINTER 1
typedef _IO_off64_t 	_libio_off_t;
typedef _libio_off_t *	_libio_pos_t;
#else
typedef off_t 		_libio_off_t;
typedef off_t 		_libio_pos_t;
#endif

/** \ingroup rpmio
 */
typedef	struct _FD_s * FD_t;

/** \ingroup rpmio
 */
typedef struct FDIO_s * FDIO_t;

/** \ingroup rpmio
 * \name RPMIO Vectors.
 */

/**
 */
typedef ssize_t (*fdio_read_function_t) (void *cookie, char *buf, size_t nbytes);

/**
 */
typedef ssize_t (*fdio_write_function_t) (void *cookie, const char *buf, size_t nbytes);

/**
 */
typedef int (*fdio_seek_function_t) (void *cookie, _libio_pos_t pos, int whence);

/**
 */
typedef int (*fdio_close_function_t) (void *cookie);


/**
 */
typedef FD_t (*fdio_ref_function_t) ( void * cookie,
		const char * msg);

/**
 */
typedef FD_t (*fdio_deref_function_t) ( FD_t fd,
		const char * msg, const char * file, unsigned line);


/**
 */
typedef FD_t (*fdio_new_function_t) (const char * msg,
		const char * file, unsigned line);


/**
 */
typedef int (*fdio_fileno_function_t) (void * cookie);


/**
 */
typedef FD_t (*fdio_open_function_t) (const char * path, int flags, mode_t mode);

/**
 */
typedef FD_t (*fdio_fopen_function_t) (const char * path, const char * fmode);

/**
 */
typedef void * (*fdio_ffileno_function_t) (FD_t fd);

/**
 */
typedef int (*fdio_fflush_function_t) (FD_t fd);


/** \ingroup rpmrpc
 * \name RPMRPC Vectors.
 */

/**
 */
typedef int (*fdio_mkdir_function_t) (const char * path, mode_t mode);

/**
 */
typedef int (*fdio_chdir_function_t) (const char * path);

/**
 */
typedef int (*fdio_rmdir_function_t) (const char * path);

/**
 */
typedef int (*fdio_rename_function_t) (const char * oldpath, const char * newpath);

/**
 */
typedef int (*fdio_unlink_function_t) (const char * path);

/**
 */
typedef int (*fdio_stat_function_t) (const char * path, struct stat * st);

/**
 */
typedef int (*fdio_lstat_function_t) (const char * path, struct stat * st);

/**
 */
typedef int (*fdio_access_function_t) (const char * path, int amode);


/** \ingroup rpmio
 */
struct FDIO_s {
  fdio_read_function_t		read;
  fdio_write_function_t		write;
  fdio_seek_function_t		seek;
  fdio_close_function_t		close;

  fdio_ref_function_t		_fdref;
  fdio_deref_function_t		_fdderef;
  fdio_new_function_t		_fdnew;
  fdio_fileno_function_t	_fileno;

  fdio_open_function_t		_open;
  fdio_fopen_function_t		_fopen;
  fdio_ffileno_function_t	_ffileno;
  fdio_fflush_function_t	_fflush;

  fdio_mkdir_function_t		_mkdir;
  fdio_chdir_function_t		_chdir;
  fdio_rmdir_function_t		_rmdir;
  fdio_rename_function_t	_rename;
  fdio_unlink_function_t	_unlink;
};


/** \ingroup rpmio
 * \name RPMIO Interface.
 */

/**
 * strerror(3) clone.
 */
const char * Fstrerror(FD_t fd);

/**
 * fread(3) clone.
 */
ssize_t Fread(void * buf, size_t size, size_t nmemb, FD_t fd);

/**
 * fwrite(3) clone.
 */
ssize_t Fwrite(const void * buf, size_t size, size_t nmemb, FD_t fd);

/**
 * fseek(3) clone.
 */
int Fseek(FD_t fd, _libio_off_t offset, int whence);

/**
 * fclose(3) clone.
 */
int Fclose( FD_t fd);

/**
 */
FD_t	Fdopen(FD_t ofd, const char * fmode);

/**
 * fopen(3) clone.
 */
FD_t	Fopen(const char * path,
			const char * fmode);


/**
 * fflush(3) clone.
 */
int Fflush(FD_t fd);

/**
 * ferror(3) clone.
 */
int Ferror(FD_t fd);

/**
 * fileno(3) clone.
 */
int Fileno(FD_t fd);

/**
 * fcntl(2) clone.
 */
int Fcntl(FD_t fd, int op, void *lip);


/** \ingroup rpmrpc
 * \name RPMRPC Interface.
 */

/**
 * mkdir(2) clone.
 */
int Mkdir(const char * path, mode_t mode);

/**
 * chdir(2) clone.
 */
int Chdir(const char * path);

/**
 * rmdir(2) clone.
 */
int Rmdir(const char * path);

/**
 * rename(2) clone.
 */
int Rename(const char * oldpath, const char * newpath);

/**
 * link(2) clone.
 */
int Link(const char * oldpath, const char * newpath);

/**
 * unlink(2) clone.
 */
int Unlink(const char * path);

/**
 * readlink(2) clone.
 */
int Readlink(const char * path, char * buf, size_t bufsiz);

/**
 * stat(2) clone.
 */
int Stat(const char * path, struct stat * st);

/**
 * lstat(2) clone.
 */
int Lstat(const char * path, struct stat * st);

/**
 * access(2) clone.
 */
int Access(const char * path, int amode);

/**
 * glob_pattern_p(3) clone.
 */
int Glob_pattern_p (const char *pattern, int quote);

/**
 * glob_error(3) clone.
 */
int Glob_error(const char * epath, int eerrno);

/**
 * glob(3) clone.
 */
int Glob(const char * pattern, int flags,
		int errfunc(const char * epath, int eerrno),
		glob_t * pglob);

/**
 * globfree(3) clone.
 */
void Globfree( glob_t * pglob);


/**
 * opendir(3) clone.
 */
DIR * Opendir(const char * path);

/**
 * readdir(3) clone.
 */
struct dirent * Readdir(DIR * dir);

/**
 * closedir(3) clone.
 */
int Closedir(DIR * dir);



/** \ingroup rpmio
 * \name RPMIO Utilities.
 */

/**
 */
off_t	fdSize(FD_t fd);

/**
 */
FD_t fdDup(int fdno);

#ifdef UNUSED
FILE *fdFdopen( void * cookie, const char * mode);
#endif

/* XXX Legacy interfaces needed by gnorpm, rpmfind et al */

/**
 */
extern int fdFileno(void * cookie);

/**
 */
extern FD_t fdOpen(const char *path, int flags, mode_t mode);

/**
 */
extern ssize_t fdRead(void * cookie, char * buf, size_t count);

/**
 */
extern ssize_t	fdWrite(void * cookie, const char * buf, size_t count);

/**
 */
extern int fdClose( void * cookie);

/**
 */
extern FD_t fdLink (void * cookie, const char * msg);

/**
 */
extern FD_t fdFree(FD_t fd, const char * msg);

/**
 */
extern FD_t fdNew (const char * msg);

/**
 */
int fdWritable(FD_t fd, int secs);

/**
 */
int fdReadable(FD_t fd, int secs);

/**
 * Insure that directories in path exist, creating as needed.
 * @param path		directory path
 * @param mode		directory mode (if created)
 * @param uid		directory uid (if created), or -1 to skip
 * @param gid		directory uid (if created), or -1 to skip
 * @return		0 on success, errno (or -1) on error
 */
int rpmioMkpath(const char * path, mode_t mode, uid_t uid, gid_t gid);

/**
 * FTP and HTTP error codes.
 */
typedef enum ftperrCode_e {
    FTPERR_NE_ERROR		= -1,	/*!< Generic error. */
    FTPERR_NE_LOOKUP		= -2,	/*!< Hostname lookup failed. */
    FTPERR_NE_AUTH		= -3,	/*!< Server authentication failed. */
    FTPERR_NE_PROXYAUTH		= -4,	/*!< Proxy authentication failed. */
    FTPERR_NE_CONNECT		= -5,	/*!< Could not connect to server. */
    FTPERR_NE_TIMEOUT		= -6,	/*!< Connection timed out. */
    FTPERR_NE_FAILED		= -7,	/*!< The precondition failed. */
    FTPERR_NE_RETRY		= -8,	/*!< Retry request. */
    FTPERR_NE_REDIRECT		= -9,	/*!< Redirect received. */

    FTPERR_BAD_SERVER_RESPONSE	= -81,	/*!< Bad server response */
    FTPERR_SERVER_IO_ERROR	= -82,	/*!< Server I/O error */
    FTPERR_SERVER_TIMEOUT	= -83,	/*!< Server timeout */
    FTPERR_BAD_HOST_ADDR	= -84,	/*!< Unable to lookup server host address */
    FTPERR_BAD_HOSTNAME		= -85,	/*!< Unable to lookup server host name */
    FTPERR_FAILED_CONNECT	= -86,	/*!< Failed to connect to server */
    FTPERR_FILE_IO_ERROR	= -87,	/*!< Failed to establish data connection to server */
    FTPERR_PASSIVE_ERROR	= -88,	/*!< I/O error to local file */
    FTPERR_FAILED_DATA_CONNECT	= -89,	/*!< Error setting remote server to passive mode */
    FTPERR_FILE_NOT_FOUND	= -90,	/*!< File not found on server */
    FTPERR_NIC_ABORT_IN_PROGRESS= -91,	/*!< Abort in progress */
    FTPERR_UNKNOWN		= -100	/*!< Unknown or unexpected error */
} ftperrCode;

/**
 */
const char * ftpStrerror(int errorNumber);

/**
 */
int ufdCopy(FD_t sfd, FD_t tfd);

/**
 */
int ufdGetFile( FD_t sfd, FD_t tfd);

/**
 */
int timedRead(FD_t fd, void * bufptr, int length);
#define	timedRead	(ufdio->read)

/**
 */
extern FDIO_t fdio;

/**
 */
extern FDIO_t fpio;

/**
 */
extern FDIO_t ufdio;

/**
 */
extern FDIO_t gzdio;

/**
 */
extern FDIO_t bzdio;

/**
 */
extern FDIO_t fadio;

static inline int xislower(int c)  {
    return (c >= 'a' && c <= 'z');
}
static inline int xisupper(int c)  {
    return (c >= 'A' && c <= 'Z');
}
static inline int xisalpha(int c)  {
    return (xislower(c) || xisupper(c));
}
static inline int xisdigit(int c)  {
    return (c >= '0' && c <= '9');
}
static inline int xisalnum(int c)  {
    return (xisalpha(c) || xisdigit(c));
}
static inline int xisblank(int c)  {
    return (c == ' ' || c == '\t');
}
static inline int xisspace(int c)  {
    return (xisblank(c) || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

static inline int xtolower(int c)  {
    return ((xisupper(c)) ? (c | ('a' - 'A')) : c);
}
static inline int xtoupper(int c)  {
    return ((xislower(c)) ? (c & ~('a' - 'A')) : c);
}

/** \ingroup rpmio
 * Locale insensitive strcasecmp(3).
 */
int xstrcasecmp(const char * s1, const char * s2)		;

/** \ingroup rpmio
 * Locale insensitive strncasecmp(3).
 */
int xstrncasecmp(const char *s1, const char * s2, size_t n)	;

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIO */
