#ifndef	H_RPMIO
#define	H_RPMIO

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

typedef	/*@abstract@*/ /*@refcounted@*/ struct _FD_s * FD_t;
typedef /*@observer@*/ struct FDIO_s * FDIO_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef /*@null@*/ FD_t fdio_ref_function_t ( /*@only@*/ void * cookie,
		const char * msg, const char * file, unsigned line);
typedef /*@null@*/ FD_t fdio_deref_function_t ( /*@only@*/ FD_t fd,
		const char * msg, const char * file, unsigned line);

typedef /*@null@*/ FD_t fdio_new_function_t (const char * msg,
		const char * file, unsigned line);

typedef int fdio_fileno_function_t (void * cookie);

typedef FD_t fdio_open_function_t (const char * path, int flags, mode_t mode);
typedef FD_t fdio_fopen_function_t (const char * path, const char * fmode);
typedef void * fdio_ffileno_function_t (FD_t fd);
typedef int fdio_fflush_function_t (FD_t fd);

typedef int fdio_mkdir_function_t (const char * path, mode_t mode);
typedef int fdio_chdir_function_t (const char * path);
typedef int fdio_rmdir_function_t (const char * path);
typedef int fdio_rename_function_t (const char * oldpath, const char * newpath);
typedef int fdio_unlink_function_t (const char * path);

typedef int fdio_stat_function_t (const char * path, struct stat * st);
typedef int fdio_lstat_function_t (const char * path, struct stat * st);
typedef int fdio_access_function_t (const char * path, int amode);

struct FDIO_s {
  cookie_read_function_t *	read;
  cookie_write_function_t *	write;
  cookie_seek_function_t *	seek;
  cookie_close_function_t *	close;

  fdio_ref_function_t *		ref;
  fdio_deref_function_t *	deref;
  fdio_new_function_t *		new;
  fdio_fileno_function_t *	fileno;

  fdio_open_function_t *	open;
  fdio_fopen_function_t *	fopen;
  fdio_ffileno_function_t *	ffileno;
  fdio_fflush_function_t *	fflush;

  fdio_mkdir_function_t *	mkdir;
  fdio_chdir_function_t *	chdir;
  fdio_rmdir_function_t *	rmdir;
  fdio_rename_function_t *	rename;
  fdio_unlink_function_t *	unlink;
};

/*@observer@*/ const char * Fstrerror(FD_t fd);

size_t	Fread	(/*@out@*/ void * buf, size_t size, size_t nmemb, FD_t fd);
size_t	Fwrite	(const void *buf, size_t size, size_t nmemb, FD_t fd);
int	Fseek	(FD_t fd, long int offset, int whence);
int	Fclose	( /*@killref@*/ FD_t fd);
FD_t	Fdopen	(FD_t fd, const char * fmode);
FD_t	Fopen	(const char * path, const char * fmode);

int	Fflush	(FD_t fd);
int	Ferror	(FD_t fd);
int	Fileno	(FD_t fd);

int	Fcntl	(FD_t, int op, void *lip);
ssize_t Pread	(FD_t fd, /*@out@*/ void * buf, size_t count, off_t offset);
ssize_t Pwrite	(FD_t fd, const void * buf, size_t count, off_t offset);
int	Mkdir	(const char * path, mode_t mode);
int	Chdir	(const char * path);
int	Rmdir	(const char * path);
int	Rename	(const char * oldpath, const char * newpath);
int	Chroot	(const char * path);
int	Unlink	(const char * path);

int	Stat	(const char * path, struct stat * st);
int	Lstat	(const char * path, struct stat * st);
int	Access	(const char * path, int amode);

/*@observer@*/ extern FDIO_t gzdio;

void	fdPush	(FD_t fd, FDIO_t io, void * fp, int fdno);
void	fdPop	(FD_t fd);

void *	fdGetFp	(FD_t fd);
void	fdSetFdno(FD_t fd, int fdno);
off_t	fdSize	(FD_t fd);
void	fdSetSyserrno(FD_t fd, int syserrno, const void * errcookie);

/*@null@*/ const FDIO_t fdGetIo(FD_t fd);
void	fdSetIo	(FD_t fd, FDIO_t io);

int	fdGetRdTimeoutSecs(FD_t fd);

long int fdGetCpioPos(FD_t fd);
void	fdSetCpioPos(FD_t fd, long int cpioPos);

extern /*@null@*/ FD_t fdDup(int fdno);
#ifdef UNUSED
extern /*@null@*/ FILE *fdFdopen( /*@only@*/ void * cookie, const char * mode);
#endif

#if 0
#define	fdRead		fdio->read
#define	fdWrite		fdio->write
#define	fdSeek		fdio->seek
#define	fdClose		fdio->close
#endif

#define	fdLink(_fd, _msg)	fdio->ref(_fd, _msg, __FILE__, __LINE__)
#define	fdFree(_fd, _msg)	fdio->deref(_fd, _msg, __FILE__, __LINE__)
#define	fdNew(_msg)		fdio->new(_msg, __FILE__, __LINE__)

#if 0
#define	fdFileno	fdio->fileno
#define	fdOpen		fdio->open
#endif

int	fdWritable(FD_t fd, int secs);
int	fdReadable(FD_t fd, int secs);

/*@observer@*/ extern FDIO_t fdio;
/*@observer@*/ extern FDIO_t fpio;

/*
 * Support for FTP and HTTP I/O.
 */
#ifndef IPPORT_FTP
#define IPPORT_FTP	21
#endif
#ifndef	IPPORT_HTTP
#define	IPPORT_HTTP	80
#endif

#define FTPERR_BAD_SERVER_RESPONSE   -1
#define FTPERR_SERVER_IO_ERROR       -2
#define FTPERR_SERVER_TIMEOUT        -3
#define FTPERR_BAD_HOST_ADDR         -4
#define FTPERR_BAD_HOSTNAME          -5
#define FTPERR_FAILED_CONNECT        -6
#define FTPERR_FILE_IO_ERROR         -7
#define FTPERR_PASSIVE_ERROR         -8
#define FTPERR_FAILED_DATA_CONNECT   -9
#define FTPERR_FILE_NOT_FOUND        -10
#define FTPERR_NIC_ABORT_IN_PROGRESS -11
#define FTPERR_UNKNOWN               -100

/*@dependent@*/ /*@null@*/ void * ufdGetUrlinfo(FD_t fd);
/*@observer@*/ const char * urlStrerror(const char * url);

int	ufdCopy(FD_t sfd, FD_t tfd);
int	ufdGetFile( /*@killref@*/ FD_t sfd, FD_t tfd);
const char *const ftpStrerror(int errorNumber);

#if 0
#define	ufdRead		ufdio->read
#define	ufdWrite	ufdio->write
#define	ufdSeek		ufdio->seek
#define	ufdClose	ufdio->close
#define	ufdLink		ufdio->ref
#define	ufdFree		ufdio->deref
#define	ufdNew		ufdio->new
#define	ufdFileno	ufdio->fileno
#define	ufdOpen		ufdio->open
#define	ufdFopen	ufdio->fopen
#define	ufdFfileno	ufdio->ffileno
#define	ufdFflush	ufdio->fflush
#define	ufdMkdir	ufdio->mkdir
#define	ufdChdir	ufdio->chdir
#define	ufdRmdir	ufdio->rmdir
#define	ufdRename	ufdio->rename
#define	ufdUnlink	ufdio->unlink
#endif

int	timedRead(FD_t fd, /*@out@*/ void * bufptr, int length);
#define	timedRead	ufdio->read

/*@observer@*/ extern FDIO_t ufdio;

/*
 * Support for first fit File Allocation I/O.
 */

long int fadGetFileSize(FD_t fd);
void	fadSetFileSize(FD_t fd, long int fileSize);
unsigned int fadGetFirstFree(FD_t fd);
void	fadSetFirstFree(FD_t fd, unsigned int firstFree);

/*@observer@*/ extern FDIO_t fadio;

#ifdef	HAVE_ZLIB_H
/*
 * Support for GZIP library.
 */

#include <zlib.h>

/*@observer@*/ extern FDIO_t gzdio;

#endif	/* HAVE_ZLIB_H */

#ifdef	HAVE_BZLIB_H
/*
 * Support for BZIP2 library.
 */

#include <bzlib.h>

/*@observer@*/ extern FDIO_t bzdio;

#endif	/* HAVE_BZLIB_H */

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIO */
