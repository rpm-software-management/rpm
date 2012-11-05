/** \ingroup rpmio
 * \file rpmio/rpmio.c
 */

#include "system.h"
#include <stdarg.h>
#include <errno.h>

#include <rpm/rpmlog.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmsw.h>
#include <rpm/rpmurl.h>

#include "rpmio/rpmio_internal.h"

#include "debug.h"

typedef struct _FDSTACK_s {
    FDIO_t		io;
    void *		fp;
    int			fdno;
} FDSTACK_t;

/** \ingroup rpmio
 * Cumulative statistics for a descriptor.
 */
typedef	struct {
    struct rpmop_s	ops[FDSTAT_MAX];	/*!< Cumulative statistics. */
} * FDSTAT_t;

/** \ingroup rpmio
 * The FD_t File Handle data structure.
 */
struct _FD_s {
    int		nrefs;
    int		flags;
#define	RPMIO_DEBUG_IO		0x40000000
    int		magic;
#define	FDMAGIC			0x04463138
    int		nfps;
    FDSTACK_t	fps[8];
    int		urlType;	/* ufdio: */

    int		syserrno;	/* last system errno encountered */
    const char *errcookie;	/* gzdio/bzdio/ufdio/xzdio: */

    char	*descr;		/* file name (or other description) */
    FDSTAT_t	stats;		/* I/O statistics */

    rpmDigestBundle digests;
};

#define DBG(_f, _m, _x) \
    \
    if ((_rpmio_debug | ((_f) ? ((FD_t)(_f))->flags : 0)) & (_m)) fprintf _x \

#define DBGIO(_f, _x)   DBG((_f), RPMIO_DEBUG_IO, _x)

static FDIO_t fdGetIo(FD_t fd)
{
    return (fd != NULL) ? fd->fps[fd->nfps].io : NULL;
}

static void fdSetIo(FD_t fd, FDIO_t io)
{
    if (fd)
	fd->fps[fd->nfps].io = io;
}

static void * fdGetFp(FD_t fd)
{
    return (fd != NULL) ? fd->fps[fd->nfps].fp : NULL;
}

static void fdSetFp(FD_t fd, void * fp)
{
    if (fd)
	fd->fps[fd->nfps].fp = fp;
}

static void fdSetFdno(FD_t fd, int fdno)
{
    if (fd) 
	fd->fps[fd->nfps].fdno = fdno;
}

static void fdPush(FD_t fd, FDIO_t io, void * fp, int fdno)
{
    if (fd == NULL || fd->nfps >= (sizeof(fd->fps)/sizeof(fd->fps[0]) - 1))
	return;
    fd->nfps++;
    fdSetIo(fd, io);
    fdSetFp(fd, fp);
    fdSetFdno(fd, fdno);
}

static void fdPop(FD_t fd)
{
    if (fd == NULL || fd->nfps < 0) return;
    fdSetIo(fd, NULL);
    fdSetFp(fd, NULL);
    fdSetFdno(fd, -1);
    fd->nfps--;
}

void fdSetBundle(FD_t fd, rpmDigestBundle bundle)
{
    if (fd)
	fd->digests = bundle;
}

rpmDigestBundle fdGetBundle(FD_t fd)
{
    return (fd != NULL) ? fd->digests : NULL;
}

static void * iotFileno(FD_t fd, FDIO_t iot)
{
    void * rc = NULL;

    if (fd == NULL)
	return NULL;

    for (int i = fd->nfps; i >= 0; i--) {
	FDSTACK_t * fps = &fd->fps[i];
	if (fps->io != iot)
	    continue;
	rc = fps->fp;
	break;
    }
    
    return rc;
}

/** \ingroup rpmio
 * \name RPMIO Vectors.
 */
typedef ssize_t (*fdio_read_function_t) (FD_t fd, void *buf, size_t nbytes);
typedef ssize_t (*fdio_write_function_t) (FD_t fd, const void *buf, size_t nbytes);
typedef int (*fdio_seek_function_t) (FD_t fd, off_t pos, int whence);
typedef int (*fdio_close_function_t) (FD_t fd);
typedef FD_t (*fdio_ref_function_t) (FD_t fd);
typedef FD_t (*fdio_deref_function_t) (FD_t fd);
typedef FD_t (*fdio_new_function_t) (const char *descr);
typedef int (*fdio_fileno_function_t) (FD_t fd);
typedef FD_t (*fdio_open_function_t) (const char * path, int flags, mode_t mode);
typedef FD_t (*fdio_fopen_function_t) (const char * path, const char * fmode);
typedef void * (*fdio_ffileno_function_t) (FD_t fd);
typedef int (*fdio_fflush_function_t) (FD_t fd);
typedef long (*fdio_ftell_function_t) (FD_t);

struct FDIO_s {
  fdio_read_function_t		read;
  fdio_write_function_t		write;
  fdio_seek_function_t		seek;
  fdio_close_function_t		close;

  fdio_ref_function_t		_fdref;
  fdio_deref_function_t		_fdderef;
  fdio_new_function_t		_fdnew;
  fdio_fileno_function_t	_fileno;

  fdio_open_function_t		_open;
  fdio_fopen_function_t		_fopen;
  fdio_ffileno_function_t	_ffileno;
  fdio_fflush_function_t	_fflush;
  fdio_ftell_function_t		_ftell;
};

/* forward refs */
static const FDIO_t fdio;
static const FDIO_t ufdio;
static const FDIO_t gzdio;
static const FDIO_t bzdio;
static const FDIO_t xzdio;
static const FDIO_t lzdio;

/** \ingroup rpmio
 * Update digest(s) attached to fd.
 */
static void fdUpdateDigests(FD_t fd, const void * buf, size_t buflen);
static FD_t fdNew(const char *descr);
/**
 */
int _rpmio_debug = 0;

/* =============================================================== */

static const char * fdbg(FD_t fd)
{
    static char buf[BUFSIZ];
    char *be = buf;
    int i;

    buf[0] = '\0';
    if (fd == NULL)
	return buf;

    *be++ = '\t';
    for (i = fd->nfps; i >= 0; i--) {
	FDSTACK_t * fps = &fd->fps[i];
	if (i != fd->nfps)
	    *be++ = ' ';
	*be++ = '|';
	*be++ = ' ';
	if (fps->io == fdio) {
	    sprintf(be, "FD %d fp %p", fps->fdno, fps->fp);
	} else if (fps->io == ufdio) {
	    sprintf(be, "UFD %d fp %p", fps->fdno, fps->fp);
	} else if (fps->io == gzdio) {
	    sprintf(be, "GZD %p fdno %d", fps->fp, fps->fdno);
#if HAVE_BZLIB_H
	} else if (fps->io == bzdio) {
	    sprintf(be, "BZD %p fdno %d", fps->fp, fps->fdno);
#endif
#if HAVE_LZMA_H
	} else if (fps->io == xzdio) {
	    sprintf(be, "XZD %p fdno %d", fps->fp, fps->fdno);
	} else if (fps->io == lzdio) {
	    sprintf(be, "LZD %p fdno %d", fps->fp, fps->fdno);
#endif
	} else {
	    sprintf(be, "??? io %p fp %p fdno %d ???",
		fps->io, fps->fp, fps->fdno);
	}
	be += strlen(be);
	*be = '\0';
    }
    return buf;
}

static void fdstat_enter(FD_t fd, fdOpX opx)
{
    if (fd->stats != NULL)
	(void) rpmswEnter(fdOp(fd, opx), (ssize_t) 0);
}

static void fdstat_exit(FD_t fd, fdOpX opx, ssize_t rc)
{
    if (rc == -1)
	fd->syserrno = errno;
    if (fd->stats != NULL)
	(void) rpmswExit(fdOp(fd, opx), rc);
}

static void fdstat_print(FD_t fd, const char * msg, FILE * fp)
{
    static const int usec_scale = (1000*1000);
    int opx;

    if (fd == NULL || fd->stats == NULL) return;
    for (opx = 0; opx < 4; opx++) {
	rpmop op = &fd->stats->ops[opx];
	if (op->count <= 0) continue;
	switch (opx) {
	case FDSTAT_READ:
	    if (msg) fprintf(fp, "%s:", msg);
	    fprintf(fp, "%8d reads, %8ld total bytes in %d.%06d secs\n",
		op->count, (long)op->bytes,
		(int)(op->usecs/usec_scale), (int)(op->usecs%usec_scale));
	    break;
	case FDSTAT_WRITE:
	    if (msg) fprintf(fp, "%s:", msg);
	    fprintf(fp, "%8d writes, %8ld total bytes in %d.%06d secs\n",
		op->count, (long)op->bytes,
		(int)(op->usecs/usec_scale), (int)(op->usecs%usec_scale));
	    break;
	case FDSTAT_SEEK:
	    break;
	case FDSTAT_CLOSE:
	    break;
	}
    }
}

off_t fdSize(FD_t fd)
{
    struct stat sb;
    off_t rc = -1; 

    if (fd != NULL && fstat(Fileno(fd), &sb) == 0)
	rc = sb.st_size;
    return rc;
}

FD_t fdDup(int fdno)
{
    FD_t fd;
    int nfdno;

    if ((nfdno = dup(fdno)) < 0)
	return NULL;
    fd = fdNew(NULL);
    fdSetFdno(fd, nfdno);
DBGIO(fd, (stderr, "==> fdDup(%d) fd %p %s\n", fdno, (fd ? fd : NULL), fdbg(fd)));
    return fd;
}

/* Regular fd doesn't have fflush() equivalent but its not an error either */
static int fdFlush(FD_t fd)
{
    return 0;
}

static int fdFileno(FD_t fd)
{
    return (fd != NULL) ? fd->fps[0].fdno : -2;
}

const char * Fdescr(FD_t fd)
{
    if (fd == NULL)
	return _("[none]");

    /* Lazy lookup if description is not set (eg dupped fd) */
    if (fd->descr == NULL) {
	int fdno = fd->fps[fd->nfps].fdno;
#if defined(__linux__)
	/* Grab the path from /proc if we can */
	char *procpath = NULL;
	char buf[PATH_MAX];
	ssize_t llen;

	rasprintf(&procpath, "/proc/self/fd/%d", fdno);
	llen = readlink(procpath, buf, sizeof(buf)-1);
	free(procpath);

	if (llen >= 1) {
	    buf[llen] = '\0';
	    /* Real paths in /proc are always absolute */
	    if (buf[0] == '/')
		fd->descr = xstrdup(buf);
	    else
		fd->descr = rstrscat(NULL, "[", buf, "]", NULL);
	}
#endif
	/* Still no description, base it on fdno which is always there */
	if (fd->descr == NULL)
	    rasprintf(&(fd->descr), "[fd %d]", fdno);
    }
    return fd->descr;
}

FD_t fdLink(FD_t fd)
{
    if (fd)
	fd->nrefs++;
    return fd;
}

FD_t fdFree( FD_t fd)
{
    if (fd) {
	if (--fd->nrefs > 0)
	    return fd;
	fd->stats = _free(fd->stats);
	if (fd->digests) {
	    fd->digests = rpmDigestBundleFree(fd->digests);
	}
	free(fd->descr);
	free(fd);
    }
    return NULL;
}

FD_t fdNew(const char *descr)
{
    FD_t fd = xcalloc(1, sizeof(*fd));
    if (fd == NULL) /* XXX xmalloc never returns NULL */
	return NULL;
    fd->nrefs = 0;
    fd->flags = 0;
    fd->magic = FDMAGIC;
    fd->urlType = URL_IS_UNKNOWN;

    fd->nfps = 0;
    memset(fd->fps, 0, sizeof(fd->fps));

    fd->fps[0].io = fdio;
    fd->fps[0].fp = NULL;
    fd->fps[0].fdno = -1;

    fd->syserrno = 0;
    fd->errcookie = NULL;
    fd->stats = xcalloc(1, sizeof(*fd->stats));
    fd->digests = NULL;
    fd->descr = descr ? xstrdup(descr) : NULL;

    return fdLink(fd);
}

static ssize_t fdRead(FD_t fd, void * buf, size_t count)
{
    return read(fdFileno(fd), buf, count);
}

static ssize_t fdWrite(FD_t fd, const void * buf, size_t count)
{
    if (count == 0)
	return 0;

    return write(fdFileno(fd), buf, count);
}

static int fdSeek(FD_t fd, off_t pos, int whence)
{
    return lseek(fdFileno(fd), pos, whence);
}

static int fdClose(FD_t fd)
{
    int fdno;
    int rc;

    if (fd == NULL) return -2;
    fdno = fdFileno(fd);

    fdSetFdno(fd, -1);

    rc = ((fdno >= 0) ? close(fdno) : -2);

    fdFree(fd);
    return rc;
}

static FD_t fdOpen(const char *path, int flags, mode_t mode)
{
    FD_t fd;
    int fdno;

    fdno = open(path, flags, mode);
    if (fdno < 0) return NULL;
    if (fcntl(fdno, F_SETFD, FD_CLOEXEC)) {
	(void) close(fdno);
	return NULL;
    }
    fd = fdNew(path);
    fdSetFdno(fd, fdno);
    fd->flags = flags;
    return fd;
}

static long fdTell(FD_t fd)
{
    return lseek(Fileno(fd), 0, SEEK_CUR);
}

static const struct FDIO_s fdio_s = {
  fdRead, fdWrite, fdSeek, fdClose, fdLink, fdFree, fdNew, fdFileno,
  fdOpen, NULL, fdGetFp, fdFlush, fdTell
};
static const FDIO_t fdio = &fdio_s ;

off_t ufdCopy(FD_t sfd, FD_t tfd)
{
    char buf[BUFSIZ];
    ssize_t rdbytes, wrbytes;
    off_t total = 0;

    while (1) {
	rdbytes = Fread(buf, sizeof(buf[0]), sizeof(buf), sfd);

	if (rdbytes > 0) {
	    wrbytes = Fwrite(buf, sizeof(buf[0]), rdbytes, tfd);
	    if (wrbytes != rdbytes) {
		total = -1;
		break;
	    }
	    total += wrbytes;
	} else {
	    if (rdbytes < 0)
		total = -1;
	    break;
	}
    }

    return total;
}

/*
 * Deal with remote url's by fetching them with a helper application
 * and treat as local file afterwards.
 * TODO:
 * - better error checking + reporting 
 * - curl & friends don't know about hkp://, transform to http?
 */

static FD_t urlOpen(const char * url, int flags, mode_t mode)
{
    FD_t fd;
    char *dest = NULL;
    int rc = 1; /* assume failure */

    fd = rpmMkTempFile(NULL, &dest);
    if (fd == NULL) {
	return NULL;
    }
    Fclose(fd);

    rc = urlGetFile(url, dest);
    if (rc == 0) {
	fd = fdOpen(dest, flags, mode);
	unlink(dest);
    } else {
	fd = NULL;
    }
    dest = _free(dest);

    return fd;

}
static FD_t ufdOpen(const char * url, int flags, mode_t mode)
{
    FD_t fd = NULL;
    const char * path;
    urltype urlType = urlPath(url, &path);

if (_rpmio_debug)
fprintf(stderr, "*** ufdOpen(%s,0x%x,0%o)\n", url, (unsigned)flags, (unsigned)mode);

    switch (urlType) {
    case URL_IS_FTP:
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_HKP:
	fd = urlOpen(url, flags, mode);
	/* we're dealing with local file when urlOpen() returns */
	urlType = URL_IS_UNKNOWN;
	break;
    case URL_IS_DASH:
	if ((flags & O_ACCMODE) == O_RDWR) {
	    fd = NULL;
	} else {
	    fd = fdDup((flags & O_ACCMODE) == O_WRONLY ?
			STDOUT_FILENO : STDIN_FILENO);
	}
	break;
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
    default:
	fd = fdOpen(path, flags, mode);
	break;
    }

    if (fd == NULL) return NULL;

    fdSetIo(fd, ufdio);
    fd->urlType = urlType;

    if (Fileno(fd) < 0) {
	(void) fdClose(fd);
	return NULL;
    }
    return fd;
}

static const struct FDIO_s ufdio_s = {
  fdRead, fdWrite, fdSeek, fdClose, fdLink, fdFree, fdNew, fdFileno,
  ufdOpen, NULL, fdGetFp, fdFlush, fdTell
};
static const FDIO_t ufdio = &ufdio_s ;

/* =============================================================== */
/* Support for GZIP library.  */
#include <zlib.h>

static void * gzdFileno(FD_t fd)
{
    return iotFileno(fd, gzdio);
}

static
FD_t gzdOpen(const char * path, const char * fmode)
{
    FD_t fd;
    gzFile gzfile;
    if ((gzfile = gzopen(path, fmode)) == NULL)
	return NULL;
    fd = fdNew(path);
    fdPop(fd); fdPush(fd, gzdio, gzfile, -1);
    
    return fdLink(fd);
}

static FD_t gzdFdopen(FD_t fd, const char *fmode)
{
    int fdno;
    gzFile gzfile;

    if (fd == NULL || fmode == NULL) return NULL;
    fdno = fdFileno(fd);
    fdSetFdno(fd, -1);		/* XXX skip the fdio close */
    if (fdno < 0) return NULL;
    gzfile = gzdopen(fdno, fmode);
    if (gzfile == NULL) return NULL;

    fdPush(fd, gzdio, gzfile, fdno);		/* Push gzdio onto stack */

    return fdLink(fd);
}

static int gzdFlush(FD_t fd)
{
    gzFile gzfile;
    gzfile = gzdFileno(fd);
    if (gzfile == NULL) return -2;
    return gzflush(gzfile, Z_SYNC_FLUSH);	/* XXX W2DO? */
}

static ssize_t gzdRead(FD_t fd, void * buf, size_t count)
{
    gzFile gzfile;
    ssize_t rc;

    gzfile = gzdFileno(fd);
    if (gzfile == NULL) return -2;	/* XXX can't happen */

    rc = gzread(gzfile, buf, count);
    if (rc < 0) {
	int zerror = 0;
	fd->errcookie = gzerror(gzfile, &zerror);
	if (zerror == Z_ERRNO) {
	    fd->syserrno = errno;
	    fd->errcookie = strerror(fd->syserrno);
	}
    }
    return rc;
}

static ssize_t gzdWrite(FD_t fd, const void * buf, size_t count)
{
    gzFile gzfile;
    ssize_t rc;

    gzfile = gzdFileno(fd);
    if (gzfile == NULL) return -2;	/* XXX can't happen */

    rc = gzwrite(gzfile, (void *)buf, count);
    if (rc < 0) {
	int zerror = 0;
	fd->errcookie = gzerror(gzfile, &zerror);
	if (zerror == Z_ERRNO) {
	    fd->syserrno = errno;
	    fd->errcookie = strerror(fd->syserrno);
	}
    }
    return rc;
}

/* XXX zlib-1.0.4 has not */
static int gzdSeek(FD_t fd, off_t pos, int whence)
{
    off_t p = pos;
    int rc;
#if HAVE_GZSEEK
    gzFile gzfile;

    if (fd == NULL) return -2;

    gzfile = gzdFileno(fd);
    if (gzfile == NULL) return -2;	/* XXX can't happen */

    rc = gzseek(gzfile, p, whence);
    if (rc < 0) {
	int zerror = 0;
	fd->errcookie = gzerror(gzfile, &zerror);
	if (zerror == Z_ERRNO) {
	    fd->syserrno = errno;
	    fd->errcookie = strerror(fd->syserrno);
	}
    }
#else
    rc = -2;
#endif
    return rc;
}

static int gzdClose(FD_t fd)
{
    gzFile gzfile;
    int rc;

    gzfile = gzdFileno(fd);
    if (gzfile == NULL) return -2;	/* XXX can't happen */

    rc = gzclose(gzfile);

    /* XXX TODO: preserve fd if errors */

    if (fd) {
	if (rc < 0) {
	    fd->errcookie = "gzclose error";
	    if (rc == Z_ERRNO) {
		fd->syserrno = errno;
		fd->errcookie = strerror(fd->syserrno);
	    }
	}
    }

    if (_rpmio_debug || rpmIsDebug()) fdstat_print(fd, "GZDIO", stderr);
    if (rc == 0)
	fdFree(fd);
    return rc;
}

static long gzdTell(FD_t fd)
{
    off_t pos = -1;
    gzFile gzfile = gzdFileno(fd);

    if (gzfile != NULL) {
#if HAVE_GZSEEK
	pos = gztell(gzfile);
	if (pos < 0) {
	    int zerror = 0;
	    fd->errcookie = gzerror(gzfile, &zerror);
	    if (zerror == Z_ERRNO) {
		fd->syserrno = errno;
		fd->errcookie = strerror(fd->syserrno);
	    }
	}
#else
	pos = -2;
#endif    
    }
    return pos;
}
static const struct FDIO_s gzdio_s = {
  gzdRead, gzdWrite, gzdSeek, gzdClose, fdLink, fdFree, fdNew, fdFileno,
  NULL, gzdOpen, gzdFileno, gzdFlush, gzdTell
};
static const FDIO_t gzdio = &gzdio_s ;

/* =============================================================== */
/* Support for BZIP2 library.  */
#if HAVE_BZLIB_H

#include <bzlib.h>

static void * bzdFileno(FD_t fd)
{
    return iotFileno(fd, bzdio);
}

static FD_t bzdOpen(const char * path, const char * mode)
{
    FD_t fd;
    BZFILE *bzfile;;
    if ((bzfile = BZ2_bzopen(path, mode)) == NULL)
	return NULL;
    fd = fdNew(path);
    fdPop(fd); fdPush(fd, bzdio, bzfile, -1);
    return fdLink(fd);
}

static FD_t bzdFdopen(FD_t fd, const char * fmode)
{
    int fdno;
    BZFILE *bzfile;

    if (fd == NULL || fmode == NULL) return NULL;
    fdno = fdFileno(fd);
    fdSetFdno(fd, -1);		/* XXX skip the fdio close */
    if (fdno < 0) return NULL;
    bzfile = BZ2_bzdopen(fdno, fmode);
    if (bzfile == NULL) return NULL;

    fdPush(fd, bzdio, bzfile, fdno);		/* Push bzdio onto stack */

    return fdLink(fd);
}

static int bzdFlush(FD_t fd)
{
    return BZ2_bzflush(bzdFileno(fd));
}

static ssize_t bzdRead(FD_t fd, void * buf, size_t count)
{
    BZFILE *bzfile;
    ssize_t rc = 0;

    bzfile = bzdFileno(fd);
    if (bzfile)
	rc = BZ2_bzread(bzfile, buf, count);
    if (rc == -1) {
	int zerror = 0;
	if (bzfile)
	    fd->errcookie = BZ2_bzerror(bzfile, &zerror);
    }
    return rc;
}

static ssize_t bzdWrite(FD_t fd, const void * buf, size_t count)
{
    BZFILE *bzfile;
    ssize_t rc;

    bzfile = bzdFileno(fd);
    rc = BZ2_bzwrite(bzfile, (void *)buf, count);
    if (rc == -1) {
	int zerror = 0;
	fd->errcookie = BZ2_bzerror(bzfile, &zerror);
    }
    return rc;
}

static int bzdClose(FD_t fd)
{
    BZFILE *bzfile;
    int rc;

    bzfile = bzdFileno(fd);

    if (bzfile == NULL) return -2;
    /* FIX: check rc */
    BZ2_bzclose(bzfile);
    rc = 0;	/* XXX FIXME */

    /* XXX TODO: preserve fd if errors */

    if (fd) {
	if (rc == -1) {
	    int zerror = 0;
	    fd->errcookie = BZ2_bzerror(bzfile, &zerror);
	}
    }

    if (_rpmio_debug || rpmIsDebug()) fdstat_print(fd, "BZDIO", stderr);
    if (rc == 0)
	fdFree(fd);
    return rc;
}

static const struct FDIO_s bzdio_s = {
  bzdRead, bzdWrite, NULL, bzdClose, fdLink, fdFree, fdNew, fdFileno,
  NULL, bzdOpen, bzdFileno, bzdFlush, NULL
};
static const FDIO_t bzdio = &bzdio_s ;

#endif	/* HAVE_BZLIB_H */

static const char * getFdErrstr (FD_t fd)
{
    const char *errstr = NULL;

    if (fdGetIo(fd) == gzdio) {
	errstr = fd->errcookie;
    } else
#ifdef	HAVE_BZLIB_H
    if (fdGetIo(fd) == bzdio) {
	errstr = fd->errcookie;
    } else
#endif	/* HAVE_BZLIB_H */
#ifdef	HAVE_LZMA_H
    if (fdGetIo(fd) == xzdio || fdGetIo(fd) == lzdio) {
	errstr = fd->errcookie;
    } else
#endif	/* HAVE_LZMA_H */
    {
	errstr = (fd->syserrno ? strerror(fd->syserrno) : "");
    }

    return errstr;
}

/* =============================================================== */
/* Support for LZMA library.  */

#ifdef HAVE_LZMA_H

#include <sys/types.h>
#include <inttypes.h>
#include <lzma.h>

#define kBufferSize (1 << 15)

typedef struct lzfile {
  /* IO buffer */
    uint8_t buf[kBufferSize];

    lzma_stream strm;

    FILE *file;

    int encoding;
    int eof;

} LZFILE;

static LZFILE *lzopen_internal(const char *path, const char *mode, int fd, int xz)
{
    int level = 7;	/* Use XZ's default compression level if unspecified */
    int encoding = 0;
    FILE *fp;
    LZFILE *lzfile;
    lzma_ret ret;
    lzma_stream init_strm = LZMA_STREAM_INIT;

    for (; *mode; mode++) {
	if (*mode == 'w')
	    encoding = 1;
	else if (*mode == 'r')
	    encoding = 0;
	else if (*mode >= '1' && *mode <= '9')
	    level = *mode - '0';
    }
    if (fd != -1)
	fp = fdopen(fd, encoding ? "w" : "r");
    else
	fp = fopen(path, encoding ? "w" : "r");
    if (!fp)
	return 0;
    lzfile = calloc(1, sizeof(*lzfile));
    if (!lzfile) {
	fclose(fp);
	return 0;
    }
    
    lzfile->file = fp;
    lzfile->encoding = encoding;
    lzfile->eof = 0;
    lzfile->strm = init_strm;
    if (encoding) {
	if (xz) {
	    ret = lzma_easy_encoder(&lzfile->strm, level, LZMA_CHECK_SHA256);
	} else {
	    lzma_options_lzma options;
	    lzma_lzma_preset(&options, level);
	    ret = lzma_alone_encoder(&lzfile->strm, &options);
	}
    } else {	/* lzma_easy_decoder_memusage(level) is not ready yet, use hardcoded limit for now */
	ret = lzma_auto_decoder(&lzfile->strm, 100<<20, 0);
    }
    if (ret != LZMA_OK) {
	fclose(fp);
	free(lzfile);
	return 0;
    }
    return lzfile;
}

static LZFILE *xzopen(const char *path, const char *mode)
{
    return lzopen_internal(path, mode, -1, 1);
}

static LZFILE *xzdopen(int fd, const char *mode)
{
    if (fd < 0)
	return 0;
    return lzopen_internal(0, mode, fd, 1);
}

static LZFILE *lzopen(const char *path, const char *mode)
{
    return lzopen_internal(path, mode, -1, 0);
}

static LZFILE *lzdopen(int fd, const char *mode)
{
    if (fd < 0)
	return 0;
    return lzopen_internal(0, mode, fd, 0);
}

static int lzflush(LZFILE *lzfile)
{
    return fflush(lzfile->file);
}

static int lzclose(LZFILE *lzfile)
{
    lzma_ret ret;
    size_t n;
    int rc;

    if (!lzfile)
	return -1;
    if (lzfile->encoding) {
	for (;;) {
	    lzfile->strm.avail_out = kBufferSize;
	    lzfile->strm.next_out = lzfile->buf;
	    ret = lzma_code(&lzfile->strm, LZMA_FINISH);
	    if (ret != LZMA_OK && ret != LZMA_STREAM_END)
		return -1;
	    n = kBufferSize - lzfile->strm.avail_out;
	    if (n && fwrite(lzfile->buf, 1, n, lzfile->file) != n)
		return -1;
	    if (ret == LZMA_STREAM_END)
		break;
	}
    }
    lzma_end(&lzfile->strm);
    rc = fclose(lzfile->file);
    free(lzfile);
    return rc;
}

static ssize_t lzread(LZFILE *lzfile, void *buf, size_t len)
{
    lzma_ret ret;
    int eof = 0;

    if (!lzfile || lzfile->encoding)
      return -1;
    if (lzfile->eof)
      return 0;
    lzfile->strm.next_out = buf;
    lzfile->strm.avail_out = len;
    for (;;) {
	if (!lzfile->strm.avail_in) {
	    lzfile->strm.next_in = lzfile->buf;
	    lzfile->strm.avail_in = fread(lzfile->buf, 1, kBufferSize, lzfile->file);
	    if (!lzfile->strm.avail_in)
		eof = 1;
	}
	ret = lzma_code(&lzfile->strm, LZMA_RUN);
	if (ret == LZMA_STREAM_END) {
	    lzfile->eof = 1;
	    return len - lzfile->strm.avail_out;
	}
	if (ret != LZMA_OK)
	    return -1;
	if (!lzfile->strm.avail_out)
	    return len;
	if (eof)
	    return -1;
      }
}

static ssize_t lzwrite(LZFILE *lzfile, void *buf, size_t len)
{
    lzma_ret ret;
    size_t n;
    if (!lzfile || !lzfile->encoding)
	return -1;
    if (!len)
	return 0;
    lzfile->strm.next_in = buf;
    lzfile->strm.avail_in = len;
    for (;;) {
	lzfile->strm.next_out = lzfile->buf;
	lzfile->strm.avail_out = kBufferSize;
	ret = lzma_code(&lzfile->strm, LZMA_RUN);
	if (ret != LZMA_OK)
	    return -1;
	n = kBufferSize - lzfile->strm.avail_out;
	if (n && fwrite(lzfile->buf, 1, n, lzfile->file) != n)
	    return -1;
	if (!lzfile->strm.avail_in)
	    return len;
    }
}

static void * lzdFileno(FD_t fd)
{
    void * rc = NULL;
    
    if (fd == NULL)
	return NULL;

    for (int i = fd->nfps; i >= 0; i--) {
	FDSTACK_t * fps = &fd->fps[i];
	if (fps->io != xzdio && fps->io != lzdio)
	    continue;
	rc = fps->fp;
	break;
    }
    return rc;
}

static FD_t xzdOpen(const char * path, const char * mode)
{
    FD_t fd;
    LZFILE *lzfile;
    if ((lzfile = xzopen(path, mode)) == NULL)
	return NULL;
    fd = fdNew(path);
    fdPop(fd); fdPush(fd, xzdio, lzfile, -1);
    return fdLink(fd);
}

static FD_t xzdFdopen(FD_t fd, const char * fmode)
{
    int fdno;
    LZFILE *lzfile;

    if (fd == NULL || fmode == NULL) return NULL;
    fdno = fdFileno(fd);
    fdSetFdno(fd, -1);          /* XXX skip the fdio close */
    if (fdno < 0) return NULL;
    lzfile = xzdopen(fdno, fmode);
    if (lzfile == NULL) return NULL;
    fdPush(fd, xzdio, lzfile, fdno);
    return fdLink(fd);
}

static FD_t lzdOpen(const char * path, const char * mode)
{
    FD_t fd;
    LZFILE *lzfile;
    if ((lzfile = lzopen(path, mode)) == NULL)
	return NULL;
    fd = fdNew(path);
    fdPop(fd); fdPush(fd, xzdio, lzfile, -1);
    return fdLink(fd);
}

static FD_t lzdFdopen(FD_t fd, const char * fmode)
{
    int fdno;
    LZFILE *lzfile;

    if (fd == NULL || fmode == NULL) return NULL;
    fdno = fdFileno(fd);
    fdSetFdno(fd, -1);          /* XXX skip the fdio close */
    if (fdno < 0) return NULL;
    lzfile = lzdopen(fdno, fmode);
    if (lzfile == NULL) return NULL;
    fdPush(fd, xzdio, lzfile, fdno);
    return fdLink(fd);
}

static int lzdFlush(FD_t fd)
{
    return lzflush(lzdFileno(fd));
}

static ssize_t lzdRead(FD_t fd, void * buf, size_t count)
{
    LZFILE *lzfile;
    ssize_t rc = 0;

    lzfile = lzdFileno(fd);
    if (lzfile)
	rc = lzread(lzfile, buf, count);
    if (rc == -1) {
	fd->errcookie = "Lzma: decoding error";
    }
    return rc;
}

static ssize_t lzdWrite(FD_t fd, const void * buf, size_t count)
{
    LZFILE *lzfile;
    ssize_t rc = 0;

    lzfile = lzdFileno(fd);

    rc = lzwrite(lzfile, (void *)buf, count);
    if (rc < 0) {
	fd->errcookie = "Lzma: encoding error";
    }
    return rc;
}

static int lzdClose(FD_t fd)
{
    LZFILE *lzfile;
    int rc;

    lzfile = lzdFileno(fd);

    if (lzfile == NULL) return -2;
    rc = lzclose(lzfile);

    /* XXX TODO: preserve fd if errors */

    if (fd) {
	if (rc == -1) {
	    fd->errcookie = "lzclose error";
	    fd->syserrno = errno;
	    fd->errcookie = strerror(fd->syserrno);
	}
    }

    if (_rpmio_debug || rpmIsDebug()) fdstat_print(fd, "XZDIO", stderr);
    if (rc == 0)
	fdFree(fd);
    return rc;
}

static struct FDIO_s xzdio_s = {
  lzdRead, lzdWrite, NULL, lzdClose, NULL, NULL, NULL, fdFileno,
  NULL, xzdOpen, lzdFileno, lzdFlush, NULL
};
static const FDIO_t xzdio = &xzdio_s;

static struct FDIO_s lzdio_s = {
  lzdRead, lzdWrite, NULL, lzdClose, NULL, NULL, NULL, fdFileno,
  NULL, lzdOpen, lzdFileno, lzdFlush, NULL
};
static const FDIO_t lzdio = &lzdio_s;

#endif	/* HAVE_LZMA_H */

/* =============================================================== */

const char *Fstrerror(FD_t fd)
{
    if (fd == NULL)
	return (errno ? strerror(errno) : "");
    return getFdErrstr(fd);
}

#define	FDIOVEC(_fd, _vec)	\
  ((fdGetIo(_fd) && fdGetIo(_fd)->_vec) ? fdGetIo(_fd)->_vec : NULL)

ssize_t Fread(void *buf, size_t size, size_t nmemb, FD_t fd)
{
    ssize_t rc = -1;

    if (fd != NULL) {
	fdio_read_function_t _read = FDIOVEC(fd, read);

	fdstat_enter(fd, FDSTAT_READ);
	do {
	    rc = (_read ? (*_read) (fd, buf, size * nmemb) : -2);
	} while (rc == -1 && errno == EINTR);
	fdstat_exit(fd, FDSTAT_READ, rc);

	if (fd->digests && rc > 0)
	    fdUpdateDigests(fd, buf, rc);
    }

    DBGIO(fd, (stderr, "==>\tFread(%p,%p,%ld) rc %ld %s\n",
	  fd, buf, (long)size * nmemb, (long)rc, fdbg(fd)));

    return rc;
}

ssize_t Fwrite(const void *buf, size_t size, size_t nmemb, FD_t fd)
{
    ssize_t rc = -1;

    if (fd != NULL) {
	fdio_write_function_t _write = FDIOVEC(fd, write);
	
	fdstat_enter(fd, FDSTAT_WRITE);
	do {
	    rc = (_write ? _write(fd, buf, size * nmemb) : -2);
	} while (rc == -1 && errno == EINTR);
	fdstat_exit(fd, FDSTAT_WRITE, rc);

	if (fd->digests && rc > 0)
	    fdUpdateDigests(fd, buf, rc);
    }

    DBGIO(fd, (stderr, "==>\tFwrite(%p,%p,%ld) rc %ld %s\n",
	  fd, buf, (long)size * nmemb, (long)rc, fdbg(fd)));

    return rc;
}

int Fseek(FD_t fd, off_t offset, int whence)
{
    int rc = -1;

    if (fd != NULL) {
	fdio_seek_function_t _seek = FDIOVEC(fd, seek);

	fdstat_enter(fd, FDSTAT_SEEK);
	rc = (_seek ? _seek(fd, offset, whence) : -2);
	fdstat_exit(fd, FDSTAT_SEEK, rc);
    }

    DBGIO(fd, (stderr, "==>\tFseek(%p,%ld,%d) rc %lx %s\n",
	  fd, (long)offset, whence, (unsigned long)rc, fdbg(fd)));

    return rc;
}

int Fclose(FD_t fd)
{
    int rc = 0, ec = 0;

    if (fd == NULL)
	return -1;

    fd = fdLink(fd);
    fdstat_enter(fd, FDSTAT_CLOSE);
    while (fd->nfps >= 0) {
	fdio_close_function_t _close = FDIOVEC(fd, close);
	rc = _close ? _close(fd) : -2;

	if (fd->nfps == 0)
	    break;
	if (ec == 0 && rc)
	    ec = rc;
	fdPop(fd);
    }
    fdstat_exit(fd, FDSTAT_CLOSE, rc);
    DBGIO(fd, (stderr, "==>\tFclose(%p) rc %lx %s\n",
	  (fd ? fd : NULL), (unsigned long)rc, fdbg(fd)));

    fdFree(fd);
    return ec;
}

/**
 * Convert stdio fmode to open(2) mode, filtering out zlib/bzlib flags.
 *	returns stdio[0] = NUL on error.
 *
 * - gzopen:	[0-9] is compression level
 * - gzopen:	'f' is filtered (Z_FILTERED)
 * - gzopen:	'h' is Huffman encoding (Z_HUFFMAN_ONLY)
 * - bzopen:	[1-9] is block size (modulo 100K)
 * - bzopen:	's' is smallmode
 * - HACK:	'.' terminates, rest is type of I/O
 */
static void cvtfmode (const char *m,
				char *stdio, size_t nstdio,
				char *other, size_t nother,
				const char **end, int * f)
{
    int flags = 0;
    char c;

    switch (*m) {
    case 'a':
	flags |= O_WRONLY | O_CREAT | O_APPEND;
	if (--nstdio > 0) *stdio++ = *m;
	break;
    case 'w':
	flags |= O_WRONLY | O_CREAT | O_TRUNC;
	if (--nstdio > 0) *stdio++ = *m;
	break;
    case 'r':
	flags |= O_RDONLY;
	if (--nstdio > 0) *stdio++ = *m;
	break;
    default:
	*stdio = '\0';
	return;
	break;
    }
    m++;

    while ((c = *m++) != '\0') {
	switch (c) {
	case '.':
	    break;
	case '+':
	    flags &= ~(O_RDONLY|O_WRONLY);
	    flags |= O_RDWR;
	    if (--nstdio > 0) *stdio++ = c;
	    continue;
	    break;
	case 'b':
	    if (--nstdio > 0) *stdio++ = c;
	    continue;
	    break;
	case 'x':
	    flags |= O_EXCL;
	    if (--nstdio > 0) *stdio++ = c;
	    continue;
	    break;
	default:
	    if (--nother > 0) *other++ = c;
	    continue;
	    break;
	}
	break;
    }

    *stdio = *other = '\0';
    if (end != NULL)
	*end = (*m != '\0' ? m : NULL);
    if (f != NULL)
	*f = flags;
}

FD_t Fdopen(FD_t ofd, const char *fmode)
{
    char stdio[20], other[20], zstdio[40];
    const char *end = NULL;
    FDIO_t iof = NULL;
    FD_t fd = ofd;

if (_rpmio_debug)
fprintf(stderr, "*** Fdopen(%p,%s) %s\n", fd, fmode, fdbg(fd));

    if (fd == NULL || fmode == NULL)
	return NULL;

    cvtfmode(fmode, stdio, sizeof(stdio), other, sizeof(other), &end, NULL);
    if (stdio[0] == '\0')
	return NULL;
    zstdio[0] = '\0';
    strncat(zstdio, stdio, sizeof(zstdio) - strlen(zstdio) - 1);
    strncat(zstdio, other, sizeof(zstdio) - strlen(zstdio) - 1);

    if (end == NULL && other[0] == '\0')
	return fd;

    if (end && *end) {
	if (rstreq(end, "fdio")) {
	    iof = fdio;
	} else if (rstreq(end, "gzdio") || rstreq(end, "gzip")) {
	    iof = gzdio;
	    fd = gzdFdopen(fd, zstdio);
#if HAVE_BZLIB_H
	} else if (rstreq(end, "bzdio") || rstreq(end, "bzip2")) {
	    iof = bzdio;
	    fd = bzdFdopen(fd, zstdio);
#endif
#if HAVE_LZMA_H
	} else if (rstreq(end, "xzdio") || rstreq(end, "xz")) {
	    iof = xzdio;
	    fd = xzdFdopen(fd, zstdio);
	} else if (rstreq(end, "lzdio") || rstreq(end, "lzma")) {
	    iof = lzdio;
	    fd = lzdFdopen(fd, zstdio);
#endif
	} else if (rstreq(end, "ufdio")) {
	    iof = ufdio;
	}
    } else if (other[0] != '\0') {
	for (end = other; *end && strchr("0123456789fh", *end); end++)
	    {};
	if (*end == '\0') {
	    iof = gzdio;
	    fd = gzdFdopen(fd, zstdio);
	}
    }
    if (iof == NULL)
	return fd;

DBGIO(fd, (stderr, "==> Fdopen(%p,\"%s\") returns fd %p %s\n", ofd, fmode, (fd ? fd : NULL), fdbg(fd)));
    return fd;
}

FD_t Fopen(const char *path, const char *fmode)
{
    char stdio[20], other[20];
    const char *end = NULL;
    mode_t perms = 0666;
    int flags = 0;
    FD_t fd;

    if (path == NULL || fmode == NULL)
	return NULL;

    stdio[0] = '\0';
    cvtfmode(fmode, stdio, sizeof(stdio), other, sizeof(other), &end, &flags);
    if (stdio[0] == '\0')
	return NULL;

    if (end == NULL || rstreq(end, "fdio")) {
if (_rpmio_debug)
fprintf(stderr, "*** Fopen fdio path %s fmode %s\n", path, fmode);
	fd = fdOpen(path, flags, perms);
	if (fdFileno(fd) < 0) {
	    if (fd) (void) fdClose(fd);
	    return NULL;
	}
    } else {
	/* XXX gzdio and bzdio here too */

	switch (urlIsURL(path)) {
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_HKP:
	case URL_IS_PATH:
	case URL_IS_DASH:
	case URL_IS_FTP:
	case URL_IS_UNKNOWN:
if (_rpmio_debug)
fprintf(stderr, "*** Fopen ufdio path %s fmode %s\n", path, fmode);
	    fd = ufdOpen(path, flags, perms);
	    if (fd == NULL || !(fdFileno(fd) >= 0))
		return fd;
	    break;
	default:
if (_rpmio_debug)
fprintf(stderr, "*** Fopen WTFO path %s fmode %s\n", path, fmode);
	    return NULL;
	    break;
	}

    }

    if (fd)
	fd = Fdopen(fd, fmode);

    DBGIO(fd, (stderr, "==>\tFopen(\"%s\",%x,0%o) %s\n",
	  path, (unsigned)flags, (unsigned)perms, fdbg(fd)));

    return fd;
}

int Fflush(FD_t fd)
{
    int rc = -1;
    if (fd != NULL) {
	fdio_fflush_function_t _fflush = FDIOVEC(fd, _fflush);

	rc = (_fflush ? _fflush(fd) : -2);
    }
    return rc;
}

off_t Ftell(FD_t fd)
{
    off_t pos = -1;
    if (fd != NULL) {
	fdio_ftell_function_t _ftell = FDIOVEC(fd, _ftell);

	pos = (_ftell ? _ftell(fd) : -2);
    }
    return pos;
}

int Ferror(FD_t fd)
{
    int i, rc = 0;

    if (fd == NULL) return -1;
    for (i = fd->nfps; rc == 0 && i >= 0; i--) {
	FDSTACK_t * fps = &fd->fps[i];
	int ec;
	
	if (fps->io == gzdio) {
	    ec = (fd->syserrno || fd->errcookie != NULL) ? -1 : 0;
	    i--;	/* XXX fdio under gzdio always has fdno == -1 */
#if HAVE_BZLIB_H
	} else if (fps->io == bzdio) {
	    ec = (fd->syserrno  || fd->errcookie != NULL) ? -1 : 0;
	    i--;	/* XXX fdio under bzdio always has fdno == -1 */
#endif
#if HAVE_LZMA_H
	} else if (fps->io == xzdio || fps->io == lzdio) {
	    ec = (fd->syserrno  || fd->errcookie != NULL) ? -1 : 0;
	    i--;	/* XXX fdio under xzdio/lzdio always has fdno == -1 */
#endif
	} else {
	/* XXX need to check ufdio/gzdio/bzdio/fdio errors correctly. */
	    ec = (fdFileno(fd) < 0 ? -1 : 0);
	}

	if (rc == 0 && ec)
	    rc = ec;
    }
DBGIO(fd, (stderr, "==> Ferror(%p) rc %d %s\n", fd, rc, fdbg(fd)));
    return rc;
}

int Fileno(FD_t fd)
{
    int i, rc = -1;

    if (fd == NULL) return -1;
    for (i = fd->nfps ; rc == -1 && i >= 0; i--) {
	rc = fd->fps[i].fdno;
    }
    
DBGIO(fd, (stderr, "==> Fileno(%p) rc %d %s\n", (fd ? fd : NULL), rc, fdbg(fd)));
    return rc;
}

/* XXX this is naive */
int Fcntl(FD_t fd, int op, void *lip)
{
    return fcntl(Fileno(fd), op, lip);
}

rpmop fdOp(FD_t fd, fdOpX opx)
{
    rpmop op = NULL;

    if (fd != NULL && fd->stats != NULL && opx >= 0 && opx < FDSTAT_MAX)
        op = fd->stats->ops + opx;
    return op;
}

int rpmioSlurp(const char * fn, uint8_t ** bp, ssize_t * blenp)
{
    static const ssize_t blenmax = (32 * BUFSIZ);
    ssize_t blen = 0;
    uint8_t * b = NULL;
    ssize_t size;
    FD_t fd;
    int rc = 0;

    fd = Fopen(fn, "r.ufdio");
    if (fd == NULL || Ferror(fd)) {
	rc = 2;
	goto exit;
    }

    size = fdSize(fd);
    blen = (size >= 0 ? size : blenmax);
    if (blen) {
	int nb;
	b = xmalloc(blen+1);
	b[0] = '\0';
	nb = Fread(b, sizeof(*b), blen, fd);
	if (Ferror(fd) || (size > 0 && nb != blen)) {
	    rc = 1;
	    goto exit;
	}
	if (blen == blenmax && nb < blen) {
	    blen = nb;
	    b = xrealloc(b, blen+1);
	}
	b[blen] = '\0';
    }

exit:
    if (fd) (void) Fclose(fd);
	
    if (rc) {
	if (b) free(b);
	b = NULL;
	blen = 0;
    }

    if (bp) *bp = b;
    else if (b) free(b);

    if (blenp) *blenp = blen;

    return rc;
}

void fdInitDigest(FD_t fd, int hashalgo, rpmDigestFlags flags)
{
    if (fd->digests == NULL) {
	fd->digests = rpmDigestBundleNew();
    }
    fdstat_enter(fd, FDSTAT_DIGEST);
    rpmDigestBundleAdd(fd->digests, hashalgo, flags);
    fdstat_exit(fd, FDSTAT_DIGEST, (ssize_t) 0);
}

static void fdUpdateDigests(FD_t fd, const void * buf, size_t buflen)
{
    if (fd && fd->digests) {
	fdstat_enter(fd, FDSTAT_DIGEST);
	rpmDigestBundleUpdate(fd->digests, buf, buflen);
	fdstat_exit(fd, FDSTAT_DIGEST, (ssize_t) buflen);
    }
}

void fdFiniDigest(FD_t fd, int hashalgo,
		void ** datap, size_t * lenp, int asAscii)
{
    if (fd && fd->digests) {
	fdstat_enter(fd, FDSTAT_DIGEST);
	rpmDigestBundleFinal(fd->digests, hashalgo, datap, lenp, asAscii);
	fdstat_exit(fd, FDSTAT_DIGEST, (ssize_t) 0);
    }
}


