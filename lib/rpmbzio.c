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
    fd = fdNew(&bzdio);
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

int bzdFlush(FD_t fd) {
    return bzflush(bzdFileno(fd));
}

const char * bzdStrerror(FD_t fd) {
    int bzerr = 0;
    return bzerror(bzdFileno(fd), &bzerr);
}

/* =============================================================== */
static ssize_t bzdRead(void * cookie, char * buf, size_t count) {
    FD_t fd = cookie;
    *((char *)buf) = '\0';
    return bzread(bzdFileno(fd), buf, count);
}

static ssize_t bzdWrite(void * cookie, const char * buf, size_t count) {
    FD_t fd = cookie;
    return bzwrite(bzdFileno(fd), (void *)buf, count);
}

static int bzdSeek(void * cookie, off_t pos, int whence) {
    return -1;
}

static int bzdClose(void * cookie) {
    FD_t fd = cookie;
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

cookie_io_functions_t bzdio = { bzdRead, bzdWrite, bzdSeek, bzdClose };

#endif	/* HAVE_BZLIB_H */
