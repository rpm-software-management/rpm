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

typedef /*@null@*/ FD_t fdio_ref_function_t ( /*@only@*/ void * cookie, const char *msg, const char *file, unsigned line);
typedef /*@null@*/ FD_t fdio_deref_function_t ( /*@only@*/ FD_t fd, const char *msg, const char *file, unsigned line);

typedef /*@null@*/ FD_t fdio_new_function_t (FDIO_t iop, const char *msg, const char *file, unsigned line);

typedef int fdio_fileno_function_t (void * cookie);

typedef FD_t fdio_open_function_t (const char *path, int flags, mode_t mode);
typedef FD_t fdio_fopen_function_t (const char *path, const char *fmode);
typedef void * fdio_ffileno_function_t (FD_t fd);
typedef int fdio_fflush_function_t (FD_t fd);

typedef int fdio_mkdir_function_t (const char *path, mode_t mode);
typedef int fdio_chdir_function_t (const char *path);
typedef int fdio_rmdir_function_t (const char *path);
typedef int fdio_rename_function_t (const char *oldpath, const char *newpath);
typedef int fdio_unlink_function_t (const char *path);

struct FDIO_s {
  cookie_read_function_t *read;
  cookie_write_function_t *write;
  cookie_seek_function_t *seek;
  cookie_close_function_t *close;

  fdio_ref_function_t *ref;
  fdio_deref_function_t *deref;
  fdio_new_function_t *new;
  fdio_fileno_function_t *fileno;

  fdio_open_function_t *open;
  fdio_fopen_function_t *fopen;
  fdio_ffileno_function_t *ffileno;
  fdio_fflush_function_t *fflush;

#ifdef NOTYET
  fdio_mkdir_function_t *mkdir;
  fdio_chdir_function_t *chdir;
  fdio_rmdir_function_t *rmdir;
  fdio_rename_function_t *rename;
  fdio_unlink_function_t *unlink;
#endif
};

/*@observer@*/ const char * Fstrerror(FD_t fd);

size_t	Fread	(/*@out@*/ void * buf, size_t size, size_t nmemb, FD_t fd);
size_t	Fwrite	(const void *buf, size_t size, size_t nmemb, FD_t fd);
int	Fseek	(FD_t fd, long int offset, int whence);
int	Fclose	( /*@killref@*/ FD_t fd);
FILE *	Fopen	(const char * path, const char * fmode);

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

int timedRead(FD_t fd, /*@out@*/void * bufptr, int length);

/*@null@*/ const FDIO_t fdGetIoCookie(FD_t fd);
void fdSetIoCookie(FD_t fd, FDIO_t iop);
#define	fdLink(_fd, _msg)	fdio->ref(_fd, _msg, __FILE__, __LINE__)
#define	fdFree(_fd, _msg)	fdio->deref(_fd, _msg, __FILE__, __LINE__)
#define	fdNew(_iop, _msg)	fdio->new(_iop, _msg, __FILE__, __LINE__)

extern /*@null@*/ FD_t fdDup(int fdno);
extern /*@null@*/ FILE *fdFdopen( /*@only@*/ void * cookie, const char * mode);

long int fdGetCpioPos(FD_t fd);
void fdSetCpioPos(FD_t fd, long int cpioPos);
int fdDebug(FD_t fd);
void fdDebugOn(FD_t fd);
void fdDebugOff(FD_t fd);

/*@observer@*/ extern FDIO_t fdio;

/*
 * Support for FTP and HTTP I/O.
 */
/*@dependent@*/ /*@null@*/ void * ufdGetUrlinfo(FD_t fd);
void ufdSetFd(FD_t fd, int fdno);
/*@observer@*/ const char * urlStrerror(const char * url);

int httpFile( /*@killref@*/ FD_t sfd, FD_t tfd, int dir);
int ftpFile( /*@killref@*/ FD_t sfd, FD_t tfd, int dir);
const char *const ftpStrerror(int errorNumber);

/*@observer@*/ extern FDIO_t ufdio;

/*
 * Support for first fit File Allocation I/O.
 */

long int fadGetFileSize(FD_t fd);
void fadSetFileSize(FD_t fd, long int fileSize);
unsigned int fadGetFirstFree(FD_t fd);
void fadSetFirstFree(FD_t fd, unsigned int firstFree);

/*@observer@*/ extern FDIO_t fadio;

#ifdef	HAVE_ZLIB_H
/*
 * Support for GZIP library.
 */

#include <zlib.h>

extern /*@null@*/ FD_t gzdFdopen( /*@only@*/ void * cookie, const char * mode);

/*@observer@*/ extern FDIO_t gzdio;

#endif	/* HAVE_ZLIB_H */

#ifdef	HAVE_BZLIB_H
/*
 * Support for BZIP2 library.
 */

#include <bzlib.h>

extern /*@null@*/ FD_t bzdFdopen( /*@only@*/ void * cookie, const char * mode);

/*@observer@*/ extern FDIO_t bzdio;

#endif	/* HAVE_BZLIB_H */

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIO */
