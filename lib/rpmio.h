#ifndef	H_RPMIO
#define	H_RPMIO

#include <sys/types.h>

typedef	/*@abstract@*/ struct {
    int fd_fd;
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

extern inline ssize_t fdLseek(FD_t fd, off_t off, int op);
extern inline ssize_t fdLseek(FD_t fd, off_t off, int op) {
    return lseek(fdFileno(fd), off, op);
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

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIO */
