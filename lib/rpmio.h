#ifndef	H_RPMIO
#define	H_RPMIO

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

typedef	/*@abstract@*/ struct _FD {
	int		fd_fd;		/* file descriptor */
/*@owned@*/ void *	fd_bzd;		/* bzdio: I/O cookie */
/*@owned@*/ void *	fd_gzd;		/* gzdio: I/O cookie */
/*@dependent@*/ void *	fd_url;		/* ufdio: URL info */
/*@observer@*/ const void *fd_errcookie;/* opaque error token */
	int		fd_errno;	/* last system errno encountered */
/*@observer@*/ const char *fd_errstr;
	int		fd_flags;
	unsigned int	firstFree;	/* fadio: */
	long int	fileSize;	/* fadio: */
	long int	fd_cpioPos;	/* cfdio: */
	long int	fd_pos;
/*@dependent@*/ cookie_io_functions_t *fd_io;
} *FD_t;

/*@observer@*/ const char * Fstrerror(FD_t);

size_t	Fread	(/*@out@*/ void *buf, size_t size, size_t nmemb, FD_t fd);
size_t	Fwrite	(const void *buf, size_t size, size_t nmemb, FD_t fd);
int	Fseek	(FD_t fd, long int offset, int whence);
int	Fclose	( /*@only@*/ FD_t fd);
FILE *	Fopen	(const char *path, const char *fmode);

int	Ferror	(FD_t fd);
int	Fileno	(FD_t fd);

int	Fcntl	(FD_t, int op, void *lip);
ssize_t Pread	(FD_t fd, /*@out@*/ void * buf, size_t count, off_t offset);
ssize_t Pwrite	(FD_t fd, const void * buf, size_t count, off_t offset);

#endif /* H_RPMIO */

#ifndef H_RPMIO_F
#define H_RPMIO_F

#ifdef __cplusplus
extern "C" {
#endif

int timedRead(FD_t fd, /*@out@*/void * bufptr, int length);

extern /*@only@*/ /*@null@*/ FD_t fdNew(cookie_io_functions_t * iop);
extern int fdValid(FD_t fd);

extern /*@only@*/ /*@null@*/ FD_t fdOpen(const char *pathname, int flags, mode_t mode);
extern /*@only@*/ /*@null@*/ FD_t fdDup(int fdno);
extern /*@dependent@*/ /*@null@*/ FILE *fdFdopen( /*@only@*/ FD_t fd, const char *mode);

/*@observer@*/ const cookie_io_functions_t * fdGetIoCookie(FD_t fd);
void fdSetIoCookie(FD_t fd, cookie_io_functions_t * io);

void fdSetDebugOn(FD_t fd);
void fdSetDebugOff(FD_t fd);

extern cookie_io_functions_t fdio;

/*
 * Support for FTP/HTTP I/O.
 */
/*@only@*/ FD_t	ufdOpen(const char * pathname, int flags, mode_t mode);
/*@dependent@*/ void * ufdGetUrlinfo(FD_t fd);
/*@observer@*/ const char *urlStrerror(const char *url);

extern cookie_io_functions_t ufdio;

/*
 * Support for first fit File Allocation I/O.
 */

long int fadGetFileSize(FD_t fd);
void fadSetFileSize(FD_t fd, long int fileSize);
unsigned int fadGetFirstFree(FD_t fd);
void fadSetFirstFree(FD_t fd, unsigned int firstFree);

extern cookie_io_functions_t fadio;

#ifdef	HAVE_ZLIB_H
/*
 * Support for GZIP library.
 */

#include <zlib.h>

extern /*@dependent@*/ /*@null@*/ gzFile * gzdFileno(FD_t fd);

extern /*@only@*/ /*@null@*/ FD_t gzdOpen(const char *pathname, const char *mode);

extern /*@only@*/ /*@null@*/ FD_t gzdFdopen( /*@only@*/ FD_t fd, const char *mode);

extern int gzdFlush(FD_t fd);

extern cookie_io_functions_t gzdio;

#endif	/* HAVE_ZLIB_H */

#ifdef	HAVE_BZLIB_H
/*
 * Support for BZIP2 library.
 */

#include <bzlib.h>

extern /*@dependent@*/ /*@null@*/ BZFILE * bzdFileno(FD_t fd);

extern /*@only@*/ /*@null@*/ FD_t bzdOpen(const char *pathname, const char *mode);

extern /*@only@*/ /*@null@*/ FD_t bzdFdopen( /*@only@*/ FD_t fd, const char *mode);

extern int bzdFlush(FD_t fd);

extern cookie_io_functions_t bzdio;

#endif	/* HAVE_BZLIB_H */

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIO_F */
