#ifndef	H_RPMIO
#define	H_RPMIO

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

typedef	/*@abstract@*/ struct _FD {
    int		fd_fd;
    /*@owned@*/ void *	fd_bzd;
    /*@owned@*/ void *	fd_gzd;
    void *	fd_url;
    int		readOnly;	/* falloc.c */
    unsigned int firstFree;	/* falloc.c */
    long int	fileSize;	/* falloc.c */
    long int	fd_cpioPos;	/* cpio.c */
    /*@observer@*/ const char *fd_errstr;
    long int	fd_pos;
    /*@dependent@*/ cookie_io_functions_t *fd_io;
} *FD_t;

/*@observer@*/ const char * Fstrerror(FD_t);

size_t	Fread	(/*@out@*/ void *buf, size_t size, size_t nmemb, FD_t fd);
size_t	Fwrite	(const void *buf, size_t size, size_t nmemb, FD_t fd);
int	Fseek	(FD_t fd, long int offset, int whence);
int	Fclose	( /*@only@*/ FD_t fd);
FILE *	Fopen	(const char *path, const char *fmode);

#endif /* H_RPMIO */

#ifndef H_RPMIO_F
#define H_RPMIO_F

#ifdef __cplusplus
extern "C" {
#endif

int timedRead(FD_t fd, /*@out@*/void * bufptr, int length);

extern /*@only@*/ /*@null@*/ FD_t fdNew(cookie_io_functions_t * iop);
extern int fdValid(FD_t fd);
extern int fdFileno(FD_t fd);

extern /*@only@*/ /*@null@*/ FD_t fdOpen(const char *pathname, int flags, mode_t mode);
extern /*@only@*/ /*@null@*/ FD_t fdDup(int fdno);
extern /*@dependent@*/ /*@null@*/ FILE *fdFdopen( /*@only@*/ FD_t fd, const char *mode);

extern cookie_io_functions_t fdio;

/*
 * Support for GZIP library.
 */
#ifdef	HAVE_ZLIB_H

#include <zlib.h>

extern /*@dependent@*/ /*@null@*/ gzFile * gzdFileno(FD_t fd);

extern /*@only@*/ /*@null@*/ FD_t gzdOpen(const char *pathname, const char *mode);

extern /*@only@*/ /*@null@*/ FD_t gzdFdopen( /*@only@*/ FD_t fd, const char *mode);

extern int gzdFlush(FD_t fd);

extern cookie_io_functions_t gzdio;

#endif	/* HAVE_ZLIB_H */

/*
 * Support for BZIP2 library.
 */
#ifdef	HAVE_BZLIB_H

#include <bzlib.h>

extern /*@dependent@*/ /*@null@*/ BZFILE * bzdFileno(FD_t fd);

extern /*@only@*/ /*@null@*/ FD_t bzdOpen(const char *pathname, const char *mode);

extern /*@only@*/ /*@null@*/ FD_t bzdFdopen( /*@only@*/ FD_t fd, const char *mode);

extern int bzdFlush(FD_t fd);

extern cookie_io_functions_t bzdio;

#endif	/* HAVE_BZLIB_H */

/*@only@*/ FD_t	ufdOpen(const char * pathname, int flags, mode_t mode);
int	ufdClose( /*@only@*/ FD_t fd);
/*@observer@*/ const char *urlStrerror(const char *url);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIO_F */
