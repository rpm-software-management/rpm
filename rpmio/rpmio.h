#ifndef	H_RPMIO
#define	H_RPMIO

/** \ingroup rpmio
 * \file rpmio/rpmio.h
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/** \ingroup rpmio
 * Hide libio API lossage.
 * The libio interface changed after glibc-2.1.3 to pass the seek offset
 * argument as a pointer rather than as an off_t. The snarl below defines
 * typedefs to isolate the lossage.
 * API unchanged.
 */
/*@{*/
#if !defined(__LCLINT__) && defined(__GLIBC__) && __GLIBC__ == 2 && __GLIBC_MINOR__ == 2
#define USE_COOKIE_SEEK_POINTER 1
typedef _IO_off64_t 	_libio_off_t;
typedef _libio_off_t *	_libio_pos_t;
#else
typedef off_t 		_libio_off_t;
typedef off_t 		_libio_pos_t;
#endif
/*@}*/

/** \ingroup rpmio
 */
typedef	/*@abstract@*/ /*@refcounted@*/ struct _FD_s * FD_t;

/** \ingroup rpmio
 */
typedef /*@observer@*/ struct FDIO_s * FDIO_t;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmio
 * \name RPMIO Vectors.
 */
/*@{*/

/** \ingroup rpmio
 */
typedef ssize_t fdio_read_function_t (void *cookie, char *buf, size_t nbytes);

/** \ingroup rpmio
 */
typedef ssize_t fdio_write_function_t (void *cookie, const char *buf, size_t nbytes);

/** \ingroup rpmio
 */
typedef int fdio_seek_function_t (void *cookie, _libio_pos_t pos, int whence);

/** \ingroup rpmio
 */
typedef int fdio_close_function_t (void *cookie);


/** \ingroup rpmio
 */
typedef /*@only@*/ /*@null@*/ FD_t fdio_ref_function_t ( /*@only@*/ void * cookie,
		const char * msg, const char * file, unsigned line);

/** \ingroup rpmio
 */
typedef /*@only@*/ /*@null@*/ FD_t fdio_deref_function_t ( /*@only@*/ FD_t fd,
		const char * msg, const char * file, unsigned line);


/** \ingroup rpmio
 */
typedef /*@only@*/ /*@null@*/ FD_t fdio_new_function_t (const char * msg,
		const char * file, unsigned line);


/** \ingroup rpmio
 */
typedef int fdio_fileno_function_t (void * cookie);


/** \ingroup rpmio
 */
typedef FD_t fdio_open_function_t (const char * path, int flags, mode_t mode);

/** \ingroup rpmio
 */
typedef FD_t fdio_fopen_function_t (const char * path, const char * fmode);

/** \ingroup rpmio
 */
typedef void * fdio_ffileno_function_t (FD_t fd);

/** \ingroup rpmio
 */
typedef int fdio_fflush_function_t (FD_t fd);
/*@}*/


/** \ingroup rpmrpc
 * \name RPMRPC Vectors.
 */
/*@{*/

/** \ingroup rpmio
 */
typedef int fdio_mkdir_function_t (const char * path, mode_t mode);

/** \ingroup rpmrpc
 */
typedef int fdio_chdir_function_t (const char * path);

/** \ingroup rpmrpc
 */
typedef int fdio_rmdir_function_t (const char * path);

/** \ingroup rpmrpc
 */
typedef int fdio_rename_function_t (const char * oldpath, const char * newpath);

/** \ingroup rpmrpc
 */
typedef int fdio_unlink_function_t (const char * path);


/** \ingroup rpmrpc
 */
typedef int fdio_stat_function_t (const char * path, struct stat * st);

/** \ingroup rpmrpc
 */
typedef int fdio_lstat_function_t (const char * path, struct stat * st);

/** \ingroup rpmrpc
 */
typedef int fdio_access_function_t (const char * path, int amode);
/*@}*/


/** \ingroup rpmio
 */
struct FDIO_s {
  fdio_read_function_t *	read;
  fdio_write_function_t *	write;
  fdio_seek_function_t *	seek;
  fdio_close_function_t *	close;

  fdio_ref_function_t *		_fdref;
  fdio_deref_function_t *	_fdderef;
  fdio_new_function_t *		_fdnew;
  fdio_fileno_function_t *	_fileno;

  fdio_open_function_t *	_open;
  fdio_fopen_function_t *	_fopen;
  fdio_ffileno_function_t *	_ffileno;
  fdio_fflush_function_t *	_fflush;

  fdio_mkdir_function_t *	_mkdir;
  fdio_chdir_function_t *	_chdir;
  fdio_rmdir_function_t *	_rmdir;
  fdio_rename_function_t *	_rename;
  fdio_unlink_function_t *	_unlink;
};


/** \ingroup rpmio
 * \name RPMIO Interface.
 */
/*@{*/

/** \ingroup rpmio
 * strerror(3) clone.
 */
/*@observer@*/ const char * Fstrerror(FD_t fd);

/** \ingroup rpmio
 * fread(3) clone.
 */
size_t	Fread	(/*@out@*/ void * buf, size_t size, size_t nmemb, FD_t fd);

/** \ingroup rpmio
 * fwrite(3) clone.
 */
size_t	Fwrite	(const void *buf, size_t size, size_t nmemb, FD_t fd);


/** \ingroup rpmio
 * fseek(3) clone.
 */
int	Fseek	(FD_t fd, _libio_off_t offset, int whence);

/** \ingroup rpmio
 * fclose(3) clone.
 */
int	Fclose	( /*@killref@*/ FD_t fd);

/** \ingroup rpmio
 */
FD_t	Fdopen	(FD_t fd, const char * fmode);

/** \ingroup rpmio
 * fopen(3) clone.
 */
FD_t	Fopen	(const char * path, const char * fmode);


/** \ingroup rpmio
 * fflush(3) clone.
 */
int	Fflush	(FD_t fd);

/** \ingroup rpmio
 * ferror(3) clone.
 */
int	Ferror	(FD_t fd);

/** \ingroup rpmio
 * fileno(3) clone.
 */
int	Fileno	(FD_t fd);


/** \ingroup rpmio
 * fcntl(2) clone.
 */
int	Fcntl	(FD_t fd, int op, void *lip);

/** \ingroup rpmio
 * pread(2) clone.
 */
ssize_t Pread(FD_t fd, void * buf, size_t count, _libio_off_t offset);

/** \ingroup rpmio
 * pwrite(2) clone.
 */
ssize_t Pwrite(FD_t fd, const void * buf, size_t count, _libio_off_t offset);
/*@}*/

/** \ingroup rpmrpc
 * \name RPMRPC Interface.
 */
/*@{*/

/** \ingroup rpmrpc
 * mkdir(2) clone.
 */
int	Mkdir	(const char * path, mode_t mode);

/** \ingroup rpmrpc
 * chdir(2) clone.
 */
int	Chdir	(const char * path);

/** \ingroup rpmrpc
 * rmdir(2) clone.
 */
int	Rmdir	(const char * path);

/** \ingroup rpmrpc
 * rename(2) clone.
 */
int	Rename	(const char * oldpath, const char * newpath);

/** \ingroup rpmrpc
 * link(2) clone.
 */
int	Link	(const char * oldpath, const char * newpath);

/** \ingroup rpmrpc
 * unlink(2) clone.
 */
int	Unlink	(const char * path);

/** \ingroup rpmrpc
 * readlink(2) clone.
 */
int	Readlink(const char * path, char * buf, size_t bufsiz);


/** \ingroup rpmrpc
 * stat(2) clone.
 */
int	Stat	(const char * path, /*@out@*/ struct stat * st);

/** \ingroup rpmrpc
 * lstat(2) clone.
 */
int	Lstat	(const char * path, /*@out@*/ struct stat * st);

/** \ingroup rpmrpc
 * access(2) clone.
 */
int	Access	(const char * path, int amode);


/** \ingroup rpmrpc
 * glob(3) clone.
 */
int	Glob	(const char * pattern, int flags,
		int errfunc(const char * epath, int eerrno), /*@out@*/ glob_t * pglob);

/** \ingroup rpmrpc
 * globfree(3) clone.
 */
void	Globfree( /*@only@*/ glob_t * pglob);


/** \ingroup rpmrpc
 * opendir(3) clone.
 */
DIR *	Opendir	(const char * name);

/** \ingroup rpmrpc
 * readdir(3) clone.
 */
struct dirent *	Readdir	(DIR * dir);

/** \ingroup rpmrpc
 * closedir(3) clone.
 */
int	Closedir(DIR * dir);
/*@}*/


/** \ingroup rpmio
 * \name RPMIO Utilities.
 */
/*@{*/

/** \ingroup rpmio
 */
off_t	fdSize	(FD_t fd);

/** \ingroup rpmio
 */
/*@null@*/ FD_t fdDup(int fdno);
#ifdef UNUSED
/*@null@*/ FILE *fdFdopen( /*@only@*/ void * cookie, const char * mode);
#endif

/* XXX Legacy interfaces needed by gnorpm, rpmfind et al */

/** \ingroup rpmio
 */
/*@-shadow@*/
int	fdFileno(void * cookie);
/*@=shadow@*/


/** \ingroup rpmio
 */
/*@null@*/ FD_t fdOpen(const char *path, int flags, mode_t mode);

/** \ingroup rpmio
 */
ssize_t fdRead(void * cookie, /*@out@*/ char * buf, size_t count);

/** \ingroup rpmio
 */
ssize_t	fdWrite(void * cookie, const char * buf, size_t count);

/** \ingroup rpmio
 */
int	fdClose( /*@only@*/ void * cookie);

/* XXX FD_t reference count debugging wrappers */
#define	fdLink(_fd, _msg)	fdio->_fdref(_fd, _msg, __FILE__, __LINE__)
#define	fdFree(_fd, _msg)	fdio->_fdderef(_fd, _msg, __FILE__, __LINE__)
#define	fdNew(_msg)		fdio->_fdnew(_msg, __FILE__, __LINE__)


/** \ingroup rpmio
 */
int	fdWritable(FD_t fd, int secs);

/** \ingroup rpmio
 */
int	fdReadable(FD_t fd, int secs);

/*
 * Support for FTP and HTTP I/O.
 */
typedef enum ftperrCode_e {
    FTPERR_BAD_SERVER_RESPONSE	= -1,	/*!< */
    FTPERR_SERVER_IO_ERROR	= -2,	/*!< */
    FTPERR_SERVER_TIMEOUT	= -3,	/*!< */
    FTPERR_BAD_HOST_ADDR	= -4,	/*!< */
    FTPERR_BAD_HOSTNAME		= -5,	/*!< */
    FTPERR_FAILED_CONNECT	= -6,	/*!< */
    FTPERR_FILE_IO_ERROR	= -7,	/*!< */
    FTPERR_PASSIVE_ERROR	= -8,	/*!< */
    FTPERR_FAILED_DATA_CONNECT	= -9,	/*!< */
    FTPERR_FILE_NOT_FOUND	= -10,	/*!< */
    FTPERR_NIC_ABORT_IN_PROGRESS= -11,	/*!< */
    FTPERR_UNKNOWN		= -100	/*!< */
} ftperrCode;

/** \ingroup rpmio
 */
/*@dependent@*/ /*@null@*/ void * ufdGetUrlinfo(FD_t fd);

/** \ingroup rpmio
 */
/*@observer@*/ const char * urlStrerror(const char * url);


/** \ingroup rpmio
 */
int	ufdCopy(FD_t sfd, FD_t tfd);

/** \ingroup rpmio
 */
int	ufdGetFile( /*@killref@*/ FD_t sfd, FD_t tfd);

/** \ingroup rpmio
 */
/*@observer@*/ const char *const ftpStrerror(int errorNumber);

int	timedRead(FD_t fd, /*@out@*/ void * bufptr, int length);
#define	timedRead	ufdio->read


/** \ingroup rpmio
 */
/*@observer@*/ extern FDIO_t fdio;

/** \ingroup rpmio
 */
/*@observer@*/ extern FDIO_t fpio;

/** \ingroup rpmio
 */
/*@observer@*/ extern FDIO_t ufdio;

/** \ingroup rpmio
 */
/*@observer@*/ extern FDIO_t gzdio;

/** \ingroup rpmio
 */
/*@observer@*/ extern FDIO_t bzdio;

/** \ingroup rpmio
 */
/*@observer@*/ extern FDIO_t fadio;
/*@}*/

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIO */
