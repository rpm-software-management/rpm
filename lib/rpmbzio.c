#include "system.h"

#include <rpmlib.h>
#include <rpmio.h>

/*@access FD_t@*/

/* =============================================================== */
/* Support for BZIP2 library.
 */
#ifdef	HAVE_BZLIB_H

#include <bzlib.h>

BZFILE * bzdFileno(FD_t fd) {
    return (fd != NULL ? ((BZFILE *)fd->fd_bzd) : NULL);
}

FD_t bzdOpen(const char *pathname, const char *mode) {
    FD_t fd;
    BZFILE *bzfile;;
    if ((bzfile = bzopen(pathname, mode)) == NULL)
	return NULL;
    fd = fdNew();
    fd->fd_bzd = bzfile;
    return fd;
}

FD_t bzdFdopen(FD_t fd, const char *mode) {
    BZFILE *bzfile  = bzdopen(fdFileno(fd), mode);
    if (bzfile != NULL) {
	fd->fd_fd = -1;
	fd->fd_bzd = bzfile;
	return fd;
    }
    return NULL;
}

ssize_t bzdRead(FD_t fd, void * buf, size_t count) {
    *((char *)buf) = '\0';
    return bzread(bzdFileno(fd), buf, count);
}

ssize_t bzdWrite(FD_t fd, const void * buf, size_t count) {
    return bzwrite(bzdFileno(fd), (void *)buf, count);
}

int bzdFlush(FD_t fd) {
    return bzflush(bzdFileno(fd));
}

const char * bzdStrerror(FD_t fd) {
    int bzerr = 0;
    return bzerror(bzdFileno(fd), &bzerr);
}

int bzdClose(FD_t fd) {
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
