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
	return fd;
    fd->fd_fd = -1;
    return fd;
}

extern inline int fdFileno(FD_t fd);
extern inline int fdFileno(FD_t fd) {
    if (fd == NULL)
	return -1;
    return fd->fd_fd;
}

extern inline /*@null@*/ FD_t fdOpen(const char *pathname, int flags, mode_t mode);
extern inline /*@null@*/ FD_t fdOpen(const char *pathname, int flags, mode_t mode) {
    FD_t fd;

    if ((fd = fdNew()) != NULL)
	fd->fd_fd = open(pathname, flags, mode);
    return fd;
}

extern inline /*@null@*/ FD_t fdDup(int fdno);
extern inline /*@null@*/ FD_t fdDup(int fdno) {
    FD_t fd;
    if ((fd = fdNew()) != NULL)
	fd->fd_fd = dup(fdno);
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

extern inline int fdClose(FD_t fd);
extern inline int fdClose(FD_t fd) {
    int fdno;

    if (fd != NULL && (fdno = fdFileno(fd)) >= 0) {
	fd->fd_fd = -1;
	free(fd);
    	return close(fdno);
    }
    return -2;
}

extern inline int fdFchmod(FD_t fd, mode_t mode);
extern inline int fdFchmod(FD_t fd, mode_t mode) {
    return fchmod(fdFileno(fd), mode);
}

extern inline int fdFstat(FD_t fd, struct stat *sb);
extern inline int fdFstat(FD_t fd, struct stat *sb) {
    return fstat(fdFileno(fd), sb);
}

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIO */
