#ifndef	H_RPMIO
#define	H_RPMIO

#include <sys/types.h>

typedef	/*@abstract@*/ struct {
    int fd_fd;
    void *	fd_bzd;
    void *	fd_gzd;
    
} *FD_t;

#ifdef __cplusplus
extern "C" {
#endif

int timedRead(FD_t fd, void * bufptr, int length);

extern inline /*@null@*/ FD_t fdNew(void);
extern inline /*@null@*/ FD_t fdNew(void) {
    FD_t fd = (FD_t)malloc(sizeof(FD_t));
    if (fd == NULL)
	return NULL;
    fd->fd_fd = -1;
    fd->fd_bzd = NULL;
    fd->fd_gzd = NULL;
    return fd;
}

extern inline int fdValid(FD_t fd);
extern inline int fdValid(FD_t fd) {
    return (fd != NULL && fd->fd_fd >= 0);
}

extern inline int fdFileno(FD_t fd);
extern inline int fdFileno(FD_t fd) {
    return (fd != NULL ? fd->fd_fd : -1);
}

extern inline /*@null@*/ FD_t fdOpen(const char *pathname, int flags, mode_t mode);
extern inline /*@null@*/ FD_t fdOpen(const char *pathname, int flags, mode_t mode) {
    FD_t fd;
    int fdno;
    if ((fdno = open(pathname, flags, mode)) < 0)
	return NULL;
    fd = fdNew();
    fd->fd_fd = fdno;
    return fd;
}

extern inline /*@null@*/ FD_t fdDup(int fdno);
extern inline /*@null@*/ FD_t fdDup(int fdno) {
    FD_t fd;
    int nfdno;
    if ((nfdno = dup(fdno)) < 0)
	return NULL;
    fd = fdNew();
    fd->fd_fd = nfdno;
    return fd;
}

extern inline off_t fdLseek(FD_t fd, off_t offset, int whence);
extern inline off_t fdLseek(FD_t fd, off_t offset, int whence) {
    return lseek(fdFileno(fd), offset, whence);
}

extern inline ssize_t fdRead(FD_t fd, void * buf, size_t count);
extern inline ssize_t fdRead(FD_t fd, void * buf, size_t count) {
    return read(fdFileno(fd), buf, count);
}

extern inline ssize_t fdWrite(FD_t fd, const void * buf, size_t count);
extern inline ssize_t fdWrite(FD_t fd, const void * buf, size_t count) {
    return write(fdFileno(fd), buf, count);
}

extern inline int fdClose(/*@only@*/ FD_t fd);
extern inline int fdClose(/*@only@*/ FD_t fd) {
    int fdno;

    if (fd != NULL && (fdno = fdFileno(fd)) >= 0) {
	fd->fd_fd = -1;
	free(fd);
    	return close(fdno);
    }
    return -2;
}

extern inline /*@shared@*/ FILE *fdFdopen(/*@owned@*/ FD_t fd, const char *mode);
/*@-mustfree*/
extern inline /*@shared@*/ FILE *fdFdopen(/*@owned@*/ FD_t fd, const char *mode) {
    FILE *f = fdopen(fdFileno(fd), mode);
    if (f != NULL) {
	fd->fd_fd = -1;
	free(fd);
	return f;
    }
    return NULL;
}
/*@=mustfree*/

/*
 * Support for GZIP library.
 */
#ifdef	HAVE_ZLIB_H

#include <zlib.h>

extern inline gzFile * gzdFileno(FD_t fd);
extern inline gzFile * gzdFileno(FD_t fd) {
    return (fd != NULL ? ((gzFile *)fd->fd_gzd) : NULL);
}

extern inline /*@null@*/ FD_t gzdOpen(const char *pathname, const char *mode);
extern inline /*@null@*/ FD_t gzdOpen(const char *pathname, const char *mode) {
    FD_t fd;
    gzFile *gzfile;;
    if ((gzfile = gzopen(pathname, mode)) == NULL)
	return NULL;
    fd = fdNew();
    fd->fd_gzd = gzfile;
    return fd;
}

extern inline /*@shared@*/ FD_t gzdFdopen(FD_t fd, const char *mode);
extern inline /*@shared@*/ FD_t gzdFdopen(FD_t fd, const char *mode) {
    gzFile *gzfile  = gzdopen(fdFileno(fd), mode);
    if (gzfile != NULL) {
	fd->fd_fd = -1;
	fd->fd_gzd = gzfile;
	return fd;
    }
    return NULL;
}

extern inline ssize_t gzdRead(FD_t fd, void * buf, size_t count);
extern inline ssize_t gzdRead(FD_t fd, void * buf, size_t count) {
    return gzread(gzdFileno(fd), buf, count);
}

extern inline ssize_t gzdWrite(FD_t fd, const void * buf, size_t count);
extern inline ssize_t gzdWrite(FD_t fd, const void * buf, size_t count) {
    return gzwrite(gzdFileno(fd), (void *)buf, count);
}

extern inline off_t gzdLseek(FD_t fd, off_t offset, int whence);
extern inline off_t gzdLseek(FD_t fd, off_t offset, int whence) {
    return gzseek(gzdFileno(fd), offset, whence);
}

extern inline int gzdFlush(FD_t fd);
extern inline int gzdFlush(FD_t fd) {
    return gzflush(gzdFileno(fd), Z_SYNC_FLUSH);	/* XXX W2DO? */
}

extern inline char * gzdStrerror(FD_t fd);
extern inline char * gzdStrerror(FD_t fd) {
    static char *zlib_err [] = {
	"OK"
	"Errno",
	"Stream", 
	"Data", 
	"Memory",
	"Buffer",
	"Version"
    };  

    int zerror;

    gzerror(gzdFileno(fd), &zerror);
    switch (zerror) {
    case Z_ERRNO:
	return strerror(errno);
	break;
    default:
	break;
    }
    return zlib_err[-zerror];
}

extern inline int gzdClose(/*@only@*/ FD_t fd);
extern inline int gzdClose(/*@only@*/ FD_t fd) {
    gzFile *gzfile;
    int zerror;

    if (fd != NULL && (gzfile = gzdFileno(fd)) != NULL) {
	fd->fd_fd = -1;
	fd->fd_bzd = NULL;
	fd->fd_gzd = NULL;
	free(fd);
    	zerror = gzclose(gzfile);
	return 0;
    }
    return -2;
}

#endif	/* HAVE_BZLIB_H */
/*
 * Support for BZIP2 library.
 */
#ifdef	HAVE_BZLIB_H

#include <bzlib.h>

extern inline BZFILE * bzdFileno(FD_t fd);
extern inline BZFILE * bzdFileno(FD_t fd) {
    return (fd != NULL ? ((BZFILE *)fd->fd_bzd) : NULL);
}

extern inline /*@null@*/ FD_t bzdOpen(const char *pathname, const char *mode);
extern inline /*@null@*/ FD_t bzdOpen(const char *pathname, const char *mode) {
    FD_t fd;
    BZFILE *bzfile;;
    if ((bzfile = bzopen(pathname, mode)) == NULL)
	return NULL;
    fd = fdNew();
    fd->fd_bzd = bzfile;
    return fd;
}

extern inline /*@shared@*/ FD_t bzdFdopen(FD_t fd, const char *mode);
extern inline /*@shared@*/ FD_t bzdFdopen(FD_t fd, const char *mode) {
    BZFILE *bzfile  = bzdopen(fdFileno(fd), mode);
    if (bzfile != NULL) {
	fd->fd_fd = -1;
	fd->fd_bzd = bzfile;
	return fd;
    }
    return NULL;
}

extern inline ssize_t bzdRead(FD_t fd, void * buf, size_t count);
extern inline ssize_t bzdRead(FD_t fd, void * buf, size_t count) {
    return bzread(bzdFileno(fd), buf, count);
}

extern inline ssize_t bzdWrite(FD_t fd, const void * buf, size_t count);
extern inline ssize_t bzdWrite(FD_t fd, const void * buf, size_t count) {
    return bzwrite(bzdFileno(fd), (void *)buf, count);
}

extern inline int bzdFlush(FD_t fd);
extern inline int bzdFlush(FD_t fd) {
    return bzflush(bzdFileno(fd));
}

extern inline char * bzdStrerror(FD_t fd);
extern inline char * bzdStrerror(FD_t fd) {
    int bzerr;
    return (char *)bzerror(bzdFileno(fd), &bzerr);
}

extern inline int bzdClose(/*@only@*/ FD_t fd);
extern inline int bzdClose(/*@only@*/ FD_t fd) {
    BZFILE *bzfile;

    if (fd != NULL && (bzfile = bzdFileno(fd)) != NULL) {
	fd->fd_fd = -1;
	fd->fd_bzd = NULL;
	fd->fd_gzd = NULL;
	free(fd);
    	bzclose(bzfile);
	return 0;
    }
    return -2;
}

#endif	/* HAVE_BZLIB_H */

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIO */
