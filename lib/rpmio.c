#include "system.h"

#include <stdarg.h>

#ifdef	__LCLINT__
#define	ntohl(_x)	(_x)
#define	ntohs(_x)	(_x)
#define	htonl(_x)	(_x)
#define	htons(_x)	(_x)
typedef	unsigned int		uint32_t;
#define	INADDR_ANY		((uint32_t) 0x00000000)
#define	IPPROTO_IP		0

#else	/* __LCLINT__ */

#if HAVE_MACHINE_TYPES_H
# include <machine/types.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>		/* XXX for inet_aton and HP-UX */

#if HAVE_NETINET_IN_SYSTM_H
# include <sys/types.h>
# include <netinet/in_systm.h>
#endif

#if HAVE_LIBIO_H && defined(_IO_BAD_SEEN)
#define	_USE_LIBIO	1
#endif

#endif	/* __LCLINT__ */

#if HAVE_HERRNO && defined(__hpux) /* XXX HP-UX w/o -D_XOPEN_SOURCE needs */
extern int h_errno;
#endif

#ifndef IPPORT_FTP
#define IPPORT_FTP	21
#endif
#ifndef	IPPORT_HTTP
#define	IPPORT_HTTP	80
#endif


#if !defined(HAVE_INET_ATON)
static int inet_aton(const char *cp, struct in_addr *inp)
{
    long addr;

    addr = inet_addr(cp);
    if (addr == ((long) -1)) return 0;

    memcpy(inp, &addr, sizeof(addr));
    return 1;
}
#endif

#if defined(USE_ALT_DNS) && USE_ALT_DNS
#include "dns.h"
#endif

#include <rpmlib.h>
#include <rpmio.h>
#include <rpmurl.h>
#include "misc.h"

#include <assert.h>

typedef struct _FDSTACK_s {
	FDIO_t		io;
/*@dependent@*/ void *	fp;
	int		fdno;
} FDSTACK_t;

typedef struct {
	int		count;
	off_t		bytes;
	time_t		msecs;
} OPSTAT_t;

typedef	struct {
	struct timeval	create;
	struct timeval	begin;
	OPSTAT_t	ops[4];
#define	FDSTAT_READ	0
#define	FDSTAT_WRITE	1
#define	FDSTAT_SEEK	2
#define	FDSTAT_CLOSE	3
} FDSTAT_t;

struct _FD_s {
/*@refs@*/ int		nrefs;
	int		flags;
#define	RPMIO_DEBUG_IO		0x40000000
#define	RPMIO_DEBUG_REFS	0x20000000
	int		magic;
#define	FDMAGIC		0xbeefdead

	int		nfps;
	FDSTACK_t	fps[8];
	int		urlType;	/* ufdio: */

/*@dependent@*/ void *	url;		/* ufdio: URL info */
	int		rd_timeoutsecs;	/* ufdRead: per FD_t timer */
	ssize_t		bytesRemain;	/* ufdio: */
	ssize_t		contentLength;	/* ufdio: */
	int		persist;	/* ufdio: */
	int		wr_chunked;	/* ufdio: */

	int		syserrno;	/* last system errno encountered */
/*@observer@*/ const void *errcookie;	/* gzdio/bzdio/ufdio: */

	FDSTAT_t	*stats;		/* I/O statistics */

	int		ftpFileDoneNeeded; /* ufdio: (FTP) */
	unsigned int	firstFree;	/* fadio: */
	long int	fileSize;	/* fadio: */
	long int	fd_cpioPos;	/* cpio: */
};

#define	FDSANE(fd)	assert(fd && fd->magic == FDMAGIC)
#define FDNREFS(fd)	(fd ? ((FD_t)fd)->nrefs : -9)
#define FDTO(fd)	(fd ? ((FD_t)fd)->rd_timeoutsecs : -99)
#define FDCPIOPOS(fd)	(fd ? ((FD_t)fd)->fd_cpioPos : -99)

#define	FDONLY(fd)	assert(fdGetIo(fd) == fdio)
#define	GZDONLY(fd)	assert(fdGetIo(fd) == gzdio)
#define	BZDONLY(fd)	assert(fdGetIo(fd) == bzdio)

#define	UFDONLY(fd)	/* assert(fdGetIo(fd) == ufdio) */

#define	fdGetFILE(_fd)	((FILE *)fdGetFp(_fd))

/*@access urlinfo@*/

#if _USE_LIBIO
int noLibio = 0;
#else
int noLibio = 1;
#endif

#define TIMEOUT_SECS 60
static int ftpTimeoutSecs = TIMEOUT_SECS;
static int httpTimeoutSecs = TIMEOUT_SECS;

int _ftp_debug = 0;
int _rpmio_debug = 0;
#define	DBG(_f, _m, _x)	\
    if ((_rpmio_debug | ((_f) ? ((FD_t)(_f))->flags : 0)) & (_m)) fprintf _x

#define DBGIO(_f, _x)	DBG((_f), RPMIO_DEBUG_IO, _x)
#define DBGREFS(_f, _x)	DBG((_f), RPMIO_DEBUG_REFS, _x)

/* =============================================================== */
const FDIO_t fdGetIo(FD_t fd) {
#ifdef	NOISY
DBGIO(0, (stderr, "==>\tfdGetIo(%p)\n", fd));
#endif
    FDSANE(fd);
    return fd->fps[fd->nfps].io;
}

void fdSetIo(FD_t fd, FDIO_t io) {
#ifdef	NOISY
DBGIO(0, (stderr, "==>\tfdSetIo(%p,%p) lvl %d \n", fd, io, (fd ? fd->nfps : -1)));
#endif
    FDSANE(fd);
    fd->fps[fd->nfps].io = io;
    return;
}

inline /*@dependent@*/ /*@null@*/ void * fdGetFp(FD_t fd) {
#ifdef	NOISY
DBGIO(0, (stderr, "==>\tfdGetFp(%p) lvl %d\n", fd, (fd ? fd->nfps : -1)));
#endif
    FDSANE(fd);
    return fd->fps[fd->nfps].fp;
}

static inline void fdSetFp(FD_t fd, /*@keep@*/ void * fp) {
#ifdef	NOISY
DBGIO(0, (stderr, "==>\tfdSetFp(%p,%p) lvl %d\n", fd, fp, (fd ? fd->nfps : -1)));
#endif
    FDSANE(fd);
    fd->fps[fd->nfps].fp = fp;
}

static inline int fdGetFdno(FD_t fd) {
#ifdef	NOISY
DBGIO(0, (stderr, "==>\tfdGetFdno(%p) lvl %d\n", fd, (fd ? fd->nfps : -1)));
#endif
    FDSANE(fd);
    return fd->fps[fd->nfps].fdno;
}

void fdSetFdno(FD_t fd, int fdno) {
#ifdef	NOISY
DBGIO(0, (stderr, "==>\tfdSetFdno(%p,%d)\n", fd, fdno));
#endif
    FDSANE(fd);
    fd->fps[fd->nfps].fdno = fdno;
}

void fdSetContentLength(FD_t fd, ssize_t contentLength)
{
    FDSANE(fd);
    fd->contentLength = fd->bytesRemain = contentLength;
}

static /*@observer@*/ const char * fdbg(FD_t fd)
{
    static char buf[BUFSIZ];
    char *be = buf;
    int i;

#if DYING
    sprintf(be, "fd %p", fd);	be += strlen(be);
    if (fd->rd_timeoutsecs >= 0) {
	sprintf(be, " secs %d", fd->rd_timeoutsecs);
	be += strlen(be);
    }
#endif
    if (fd->bytesRemain != -1) {
	sprintf(be, " clen %d", (int)fd->bytesRemain);
	be += strlen(be);
     }
    if (fd->wr_chunked) {
	strcpy(be, " chunked");
	be += strlen(be);
     }
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
	} else if (fps->io == fadio) {
	    sprintf(be, "FAD %d fp %p", fps->fdno, fps->fp);
	} else if (fps->io == gzdio) {
	    sprintf(be, "GZD %p fdno %d", fps->fp, fps->fdno);
#if HAVE_BZLIB_H
	} else if (fps->io == bzdio) {
	    sprintf(be, "BZD %p fdno %d", fps->fp, fps->fdno);
#endif
	} else if (fps->io == fpio) {
	    sprintf(be, "%s %p(%d) fdno %d",
		(fps->fdno < 0 ? "LIBIO" : "FP"),
		fps->fp, fileno(((FILE *)fps->fp)), fps->fdno);
	} else {
	    sprintf(be, "??? io %p fp %p fdno %d ???",
		fps->io, fps->fp, fps->fdno);
	}
	be += strlen(be);
	*be = '\0';
    }
    return buf;
}

inline void fdPush(FD_t fd, FDIO_t io, void * fp, int fdno) {
    FDSANE(fd);
    if (fd->nfps >= (sizeof(fd->fps)/sizeof(fd->fps[0]) - 1))
	return;
    fd->nfps++;
    fdSetIo(fd, io);
    fdSetFp(fd, fp);
    fdSetFdno(fd, fdno);
DBGIO(0, (stderr, "==>\tfdPush(%p,%p,%p,%d) lvl %d %s\n", fd, io, fp, fdno, fd->nfps, fdbg(fd)));
}

inline void fdPop(FD_t fd) {
    FDSANE(fd);
    if (fd->nfps < 0) return;
DBGIO(0, (stderr, "==>\tfdPop(%p) lvl %d io %p fp %p fdno %d %s\n", fd, fd->nfps, fdGetIo(fd), fdGetFp(fd), fdGetFdno(fd), fdbg(fd)));
    fdSetIo(fd, NULL);
    fdSetFp(fd, NULL);
    fdSetFdno(fd, -1);
    fd->nfps--;
}

static inline void fdstat_enter(FD_t fd, int opx)
{
    if (fd->stats == NULL) return;
    fd->stats->ops[opx].count++;
    gettimeofday(&fd->stats->begin, NULL);
}

static inline time_t tvsub(struct timeval *etv, struct timeval *btv) {
    time_t secs, usecs;
    if (!(etv && btv)) return 0;
    secs = etv->tv_sec - btv->tv_sec;
    usecs = etv->tv_usec - btv->tv_usec;
    while (usecs < 0) {
	secs++;
	usecs += 1000000;
    }
    return ((secs * 1000) + (usecs/1000));
}

static inline void fdstat_exit(FD_t fd, int opx, ssize_t rc)
{
    struct timeval end;
    if (rc == -1) fd->syserrno = errno;
    if (fd->stats == NULL) return;
    gettimeofday(&end, NULL);
    if (rc >= 0) {
	switch(opx) {
	case FDSTAT_SEEK:
	    fd->stats->ops[opx].bytes = rc;
	    break;
	default:
	    fd->stats->ops[opx].bytes += rc;
	    if (fd->bytesRemain > 0) fd->bytesRemain -= rc;
	    break;
	}
    }
    fd->stats->ops[opx].msecs += tvsub(&end, &fd->stats->begin);
    fd->stats->begin = end;	/* structure assignment */
}

static void fdstat_print(FD_t fd, const char * msg, FILE * fp) {
    int opx;
    if (fd->stats == NULL) return;
    for (opx = 0; opx < 4; opx++) {
	OPSTAT_t *ops = &fd->stats->ops[opx];
	if (ops->count <= 0) continue;
	switch (opx) {
	case FDSTAT_READ:
	    if (msg) fprintf(fp, "%s:", msg);
	    fprintf(fp, "%8d reads, %8ld total bytes in %d.%03d secs\n",
		ops->count, (long)ops->bytes,
		(int)(ops->msecs/1000), (int)(ops->msecs%1000));
	    break;
	case FDSTAT_WRITE:
	    if (msg) fprintf(fp, "%s:", msg);
	    fprintf(fp, "%8d writes, %8ld total bytes in %d.%03d secs\n",
		ops->count, (long)ops->bytes,
		(int)(ops->msecs/1000), (int)(ops->msecs%1000));
	    break;
	case FDSTAT_SEEK:
	    break;
	case FDSTAT_CLOSE:
	    break;
	}
    }
}

/* =============================================================== */
off_t fdSize(FD_t fd) {
    struct stat sb;
    off_t rc = -1; 

#ifdef	NOISY
DBGIO(0, (stderr, "==>\tfdSize(%p) rc %ld\n", fd, (long)rc));
#endif
    FDSANE(fd);
    if (fd->contentLength >= 0)
	rc = fd->contentLength;
    else switch (fd->urlType) {
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	if (fstat(Fileno(fd), &sb) == 0)
	    rc = sb.st_size;
	/*@fallthrough@*/
    case URL_IS_FTP:
    case URL_IS_HTTP:
    case URL_IS_DASH:
	break;
    }
    return rc;
}

void fdSetSyserrno(FD_t fd, int syserrno, const void * errcookie) {
    FDSANE(fd);
    fd->syserrno = syserrno;
    fd->errcookie = errcookie;
}

int fdGetRdTimeoutSecs(FD_t fd) {
#ifdef	NOISY
DBGIO(0, (stderr, "==>\tfdGetRdTimeoutSecs(%p) timeout %d\n", fd, FDTO(fd)));
#endif
    FDSANE(fd);
    return fd->rd_timeoutsecs;
}

#ifdef	DYING
int fdGetFtpFileDoneNeeded(FD_t fd) {
    FDSANE(fd);
    return fd->ftpFileDoneNeeded;
}

void fdSetFtpFileDoneNeeded(FD_t fd, int ftpFileDoneNeeded) {
    FDSANE(fd);
    fd->ftpFileDoneNeeded = ftpFileDoneNeeded;
}
#endif

long int fdGetCpioPos(FD_t fd) {
    FDSANE(fd);
    return fd->fd_cpioPos;
}

void fdSetCpioPos(FD_t fd, long int cpioPos) {
    FDSANE(fd);
    fd->fd_cpioPos = cpioPos;
}

FD_t fdDup(int fdno) {
    FD_t fd;
    int nfdno;

    if ((nfdno = dup(fdno)) < 0)
	return NULL;
    fd = fdNew("open (fdDup)");
    fdSetFdno(fd, nfdno);
DBGIO(fd, (stderr, "==> fdDup(%d) fd %p %s\n", fdno, fd, fdbg(fd)));
    return fd;
}

static inline FD_t c2f(void * cookie) {
	FD_t fd = (FD_t) cookie;
	FDSANE(fd);
	return fd;
}

#ifdef USE_COOKIE_SEEK_POINTER
static inline int fdSeekNot(void * cookie,  /*@unused@*/ _IO_off64_t *pos,  /*@unused@*/ int whence) {
#else
static inline int fdSeekNot(void * cookie,  /*@unused@*/ off_t pos,  /*@unused@*/ int whence) {
#endif
    FD_t fd = c2f(cookie);
    FDSANE(fd);		/* XXX keep gcc quiet */
    return -2;
}

#ifdef UNUSED
FILE *fdFdopen(void * cookie, const char *fmode) {
    FD_t fd = c2f(cookie);
    int fdno;
    FILE * fp;

    if (fmode == NULL) return NULL;
    fdno = fdFileno(fd);
    if (fdno < 0) return NULL;
    fp = fdopen(fdno, fmode);
DBGIO(fd, (stderr, "==> fdFdopen(%p,\"%s\") fdno %d -> fp %p fdno %d\n", cookie, fmode, fdno, fp, fileno(fp)));
    fd = fdFree(fd, "open (fdFdopen)");
    return fp;
}
#endif

#undef	fdRead
#undef	fdWrite
#undef	fdSeek
#undef	fdClose
#if 0
#undef	fdLink
#undef	fdFree
#undef	fdNew
#endif
#undef	fdFileno
#undef	fdOpen

/* =============================================================== */
static inline FD_t XfdLink(void * cookie, const char *msg, const char *file, unsigned line) {
    FD_t fd;
if (cookie == NULL)
DBGREFS(0, (stderr, "--> fd  %p ++ %d %s at %s:%u\n", cookie, FDNREFS(cookie)+1, msg, file, line));
    fd = c2f(cookie);
    if (fd) {
	fd->nrefs++;
DBGREFS(fd, (stderr, "--> fd  %p ++ %d %s at %s:%u %s\n", fd, fd->nrefs, msg, file, line, fdbg(fd)));
    }
    return fd;
}

static inline /*@null@*/ FD_t XfdFree( /*@killref@*/ FD_t fd, const char *msg, const char *file, unsigned line) {
if (fd == NULL)
DBGREFS(0, (stderr, "--> fd  %p -- %d %s at %s:%u\n", fd, FDNREFS(fd), msg, file, line));
    FDSANE(fd);
    if (fd) {
DBGREFS(fd, (stderr, "--> fd  %p -- %d %s at %s:%u %s\n", fd, fd->nrefs, msg, file, line, fdbg(fd)));
	if (--fd->nrefs > 0)
	    /*@-refcounttrans@*/ return fd; /*@=refcounttrans@*/
	if (fd->stats) free(fd->stats);
	/*@-refcounttrans@*/ free(fd); /*@=refcounttrans@*/
    }
    return NULL;
}

static inline /*@null@*/ FD_t XfdNew(const char *msg, const char *file, unsigned line) {
    FD_t fd = (FD_t) xmalloc(sizeof(struct _FD_s));
    if (fd == NULL)
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

    fd->url = NULL;
    fd->rd_timeoutsecs = 1;	/* XXX default value used to be -1 */
    fd->contentLength = fd->bytesRemain = -1;
    fd->wr_chunked = 0;
    fd->syserrno = 0;
    fd->errcookie = NULL;
    fd->stats = calloc(1, sizeof(FDSTAT_t));
    gettimeofday(&fd->stats->create, NULL);
    fd->stats->begin = fd->stats->create;	/* structure assignment */

    fd->ftpFileDoneNeeded = 0;
    fd->firstFree = 0;
    fd->fileSize = 0;
    fd->fd_cpioPos = 0;

    return XfdLink(fd, msg, file, line);
}

static inline int fdFileno(void * cookie) {
    FD_t fd;
    if (cookie == NULL) return -2;
    fd = c2f(cookie);
    return fd->fps[0].fdno;	/* XXX WRONG but expedient */
}

static inline ssize_t fdRead(void * cookie, /*@out@*/ char * buf, size_t count) {
    FD_t fd = c2f(cookie);
    ssize_t rc;

    if (fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */

    fdstat_enter(fd, FDSTAT_READ);
    rc = read(fdFileno(fd), buf, (count > fd->bytesRemain ? fd->bytesRemain : count));
    fdstat_exit(fd, FDSTAT_READ, rc);

DBGIO(fd, (stderr, "==>\tfdRead(%p,%p,%ld) rc %ld %s\n", cookie, buf, (long)count, (long)rc, fdbg(fd)));

    return rc;
}

static inline ssize_t fdWrite(void * cookie, const char * buf, size_t count) {
    FD_t fd = c2f(cookie);
    int fdno = fdFileno(fd);
    ssize_t rc;

    if (fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */
    if (fd->wr_chunked) {
	char chunksize[20];
	sprintf(chunksize, "%x\r\n", (unsigned)count);
#ifdef	DYING
if (_ftp_debug)
fprintf(stderr, "-> %s", chunksize);
#endif
	rc = write(fdno, chunksize, strlen(chunksize));
	if (rc == -1)	fd->syserrno = errno;
    }
    if (count == 0) return 0;

    fdstat_enter(fd, FDSTAT_WRITE);
    rc = write(fdno, buf, (count > fd->bytesRemain ? fd->bytesRemain : count));
    fdstat_exit(fd, FDSTAT_WRITE, rc);

    if (fd->wr_chunked) {
	int ec;
#ifdef	DYING
if (_ftp_debug)
fprintf(stderr, "-> \r\n");
#endif
	ec = write(fdno, "\r\n", sizeof("\r\n")-1);
	if (ec == -1)	fd->syserrno = errno;
    }

DBGIO(fd, (stderr, "==>\tfdWrite(%p,%p,%ld) rc %ld %s\n", cookie, buf, (long)count, (long)rc, fdbg(fd)));

    return rc;
}

#ifdef USE_COOKIE_SEEK_POINTER
static inline int fdSeek(void * cookie, _IO_off64_t *pos, int whence) {
    _IO_off64_t p = *pos;
#else
static inline int fdSeek(void * cookie, off_t p, int whence) {
#endif
    FD_t fd = c2f(cookie);
    off_t rc;

    assert(fd->bytesRemain == -1);	/* XXX FIXME fadio only for now */
    fdstat_enter(fd, FDSTAT_SEEK);
    rc = lseek(fdFileno(fd), p, whence);
    fdstat_exit(fd, FDSTAT_SEEK, rc);

DBGIO(fd, (stderr, "==>\tfdSeek(%p,%ld,%d) rc %lx %s\n", cookie, (long)p, whence, (long)rc, fdbg(fd)));

    return rc;
}

static inline int fdClose( /*@only@*/ void * cookie) {
    FD_t fd;
    int fdno;
    int rc;

    if (cookie == NULL) return -2;
    fd = c2f(cookie);
    fdno = fdFileno(fd);

    fdSetFdno(fd, -1);

    fdstat_enter(fd, FDSTAT_CLOSE);
    rc = ((fdno >= 0) ? close(fdno) : -2);
    fdstat_exit(fd, FDSTAT_CLOSE, rc);

DBGIO(fd, (stderr, "==>\tfdClose(%p) rc %lx %s\n", fd, (long)rc, fdbg(fd)));

    fd = fdFree(fd, "open (fdClose)");
    return rc;
}

static inline /*@null@*/ FD_t fdOpen(const char *path, int flags, mode_t mode) {
    FD_t fd;
    int fdno;

#ifdef	DYING
if (_rpmio_debug)
fprintf(stderr, "*** fdOpen(%s,0x%x,0%o)\n", path, flags, (unsigned)mode);
#endif
    fdno = open(path, flags, mode);
    if (fdno < 0) return NULL;
    fd = fdNew("open (fdOpen)");
    fdSetFdno(fd, fdno);
    fd->flags = flags;
DBGIO(fd, (stderr, "==>\tfdOpen(\"%s\",%x,0%o) %s\n", path, flags, (unsigned)mode, fdbg(fd)));
    return fd;
}

static struct FDIO_s fdio_s = {
  fdRead, fdWrite, fdSeek, fdClose, XfdLink, XfdFree, XfdNew, fdFileno,
  fdOpen, NULL, fdGetFp, NULL,	mkdir, chdir, rmdir, rename, unlink
};
FDIO_t fdio = /*@-compmempass@*/ &fdio_s /*@=compmempass@*/ ;

int fdWritable(FD_t fd, int secs)
{
    int fdno;
    fd_set wrfds;
    struct timeval timeout, *tvp = (secs >= 0 ? &timeout : NULL);
    int rc;
	
    if ((fdno = fdFileno(fd)) < 0)
	return -1;	/* XXX W2DO? */
	
    FD_ZERO(&wrfds);
    do {
	FD_SET(fdno, &wrfds);

	if (tvp) {
	    tvp->tv_sec = secs;
	    tvp->tv_usec = 0;
	}
	errno = 0;
	rc = select(fdno + 1, NULL, &wrfds, NULL, tvp);

if (_rpmio_debug && !(rc == 1 && errno == 0))
fprintf(stderr, "*** fdWritable fdno %d rc %d %s\n", fdno, rc, strerror(errno));
	if (rc < 0) {
	    switch (errno) {
	    case EINTR:
		continue;
		/*@notreached@*/ break;
	    default:
		return rc;
		/*@notreached@*/ break;
	    }
	}
	return rc;
    } while (1);
    /*@notreached@*/
}

int fdReadable(FD_t fd, int secs)
{
    int fdno;
    fd_set rdfds;
    struct timeval timeout, *tvp = (secs >= 0 ? &timeout : NULL);
    int rc;

    if ((fdno = fdFileno(fd)) < 0)
	return -1;	/* XXX W2DO? */
	
    FD_ZERO(&rdfds);
    do {
	FD_SET(fdno, &rdfds);

	if (tvp) {
	    tvp->tv_sec = secs;
	    tvp->tv_usec = 0;
	}
	errno = 0;
	rc = select(fdno + 1, &rdfds, NULL, NULL, tvp);

	if (rc < 0) {
	    switch (errno) {
	    case EINTR:
		continue;
		/*@notreached@*/ break;
	    default:
		return rc;
		/*@notreached@*/ break;
	    }
	}
	return rc;
    } while (1);
    /*@notreached@*/
}

static int fdFgets(FD_t fd, char * buf, size_t len)
{
    int fdno;
    int secs = fd->rd_timeoutsecs;
    size_t nb = 0;
    int ec = 0;
    char lastchar = '\0';

    if ((fdno = fdFileno(fd)) < 0)
	return 0;	/* XXX W2DO? */
	
    do {
	int rc;

	/* Is there data to read? */
	rc = fdReadable(fd, secs);

	switch (rc) {
	case -1:	/* error */
	    ec = -1;
	    continue;
	    /*@notreached@*/ break;
	case  0:	/* timeout */
	    ec = -1;
	    continue;
	    /*@notreached@*/ break;
	default:	/* data to read */
	    break;
	}

	errno = 0;
#ifdef	NOISY
	rc = fdRead(fd, buf + nb, 1);
#else
	rc = read(fdFileno(fd), buf + nb, 1);
#endif
	if (rc < 0) {
	    fd->syserrno = errno;
	    switch (errno) {
	    case EWOULDBLOCK:
		continue;
		/*@notreached@*/ break;
	    default:
		break;
	    }
if (_rpmio_debug)
fprintf(stderr, "*** read: fd %p rc %d errno %d %s \"%s\"\n", fd, rc, errno, strerror(errno), buf);
	    ec = -1;
	    break;
	} else if (rc == 0) {
if (_rpmio_debug)
fprintf(stderr, "*** read: fd %p rc %d EOF errno %d %s \"%s\"\n", fd, rc, errno, strerror(errno), buf);
	    break;
	} else {
	    nb += rc;
	    buf[nb] = '\0';
	    lastchar = buf[nb - 1];
	}
    } while (ec == 0 && nb < len && lastchar != '\n');

    return (ec >= 0 ? nb : ec);
}

/* =============================================================== */
/* Support for FTP/HTTP I/O.
 */
const char *const ftpStrerror(int errorNumber) {
  switch (errorNumber) {
    case 0:
	return _("Success");

    case FTPERR_BAD_SERVER_RESPONSE:
	return _("Bad server response");

    case FTPERR_SERVER_IO_ERROR:
	return _("Server IO error");

    case FTPERR_SERVER_TIMEOUT:
	return _("Server timeout");

    case FTPERR_BAD_HOST_ADDR:
	return _("Unable to lookup server host address");

    case FTPERR_BAD_HOSTNAME:
	return _("Unable to lookup server host name");

    case FTPERR_FAILED_CONNECT:
	return _("Failed to connect to server");

    case FTPERR_FAILED_DATA_CONNECT:
	return _("Failed to establish data connection to server");

    case FTPERR_FILE_IO_ERROR:
	return _("IO error to local file");

    case FTPERR_PASSIVE_ERROR:
	return _("Error setting remote server to passive mode");

    case FTPERR_FILE_NOT_FOUND:
	return _("File not found on server");

    case FTPERR_NIC_ABORT_IN_PROGRESS:
	return _("Abort in progress");

    case FTPERR_UNKNOWN:
    default:
	return _("Unknown or unexpected error");
  }
}

const char *urlStrerror(const char *url)
{
    const char *retstr;
    switch (urlIsURL(url)) {
    case URL_IS_FTP:
    case URL_IS_HTTP:
    {	urlinfo u;
/* XXX This only works for httpReq/ftpLogin/ftpReq failures */
	if (urlSplit(url, &u) == 0) {
	    retstr = ftpStrerror(u->openError);
	} else
	    retstr = "Malformed URL";
    }	break;
    default:
	retstr = strerror(errno);
	break;
    }
    return retstr;
}

#if !defined(USE_ALT_DNS) || !USE_ALT_DNS 
static int mygethostbyname(const char * host, struct in_addr * address)
{
    struct hostent * hostinfo;

    hostinfo = /*@-unrecog@*/ gethostbyname(host) /*@=unrecog@*/;
    if (!hostinfo) return 1;

    memcpy(address, hostinfo->h_addr_list[0], sizeof(*address));
    return 0;
}
#endif

static int getHostAddress(const char * host, struct in_addr * address)
{
    if (isdigit(host[0])) {
      if (! /*@-unrecog@*/ inet_aton(host, address) /*@=unrecog@*/ ) {
	  return FTPERR_BAD_HOST_ADDR;
      }
    } else {
      if (mygethostbyname(host, address)) {
	  errno = h_errno;
	  return FTPERR_BAD_HOSTNAME;
      }
    }
    
    return 0;
}

static int tcpConnect(FD_t ctrl, const char *host, int port)
{
    struct sockaddr_in sin;
    int fdno = -1;
    int rc;

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = INADDR_ANY;
    
  do {
    if ((rc = getHostAddress(host, &sin.sin_addr)) < 0)
	break;

    if ((fdno = socket(sin.sin_family, SOCK_STREAM, IPPROTO_IP)) < 0) {
	rc = FTPERR_FAILED_CONNECT;
	break;
    }

    if (connect(fdno, (struct sockaddr *) &sin, sizeof(sin))) {
	rc = FTPERR_FAILED_CONNECT;
	break;
    }
  } while (0);

    if (rc < 0)
	goto errxit;

if (_ftp_debug)
fprintf(stderr,"++ connect %s:%d on fdno %d\n",
/*@-unrecog@*/ inet_ntoa(sin.sin_addr) /*@=unrecog@*/ ,
ntohs(sin.sin_port), fdno);

    fdSetFdno(ctrl, (fdno >= 0 ? fdno : -1));
    return 0;

errxit:
    fdSetSyserrno(ctrl, errno, ftpStrerror(rc));
    if (fdno >= 0)
	close(fdno);
    return rc;
}

static int checkResponse(void * uu, FD_t ctrl, /*@out@*/ int *ecp, /*@out@*/ char ** str)
{
    urlinfo u = uu;
    char *buf;
    size_t bufAlloced;
    int bufLength = 0; 
    const char *s;
    char *se;
    int ec = 0;
    int moretodo = 1;
    char errorCode[4];
 
    URLSANE(u);
    if (u->bufAlloced == 0 || u->buf == NULL) {
	u->bufAlloced = url_iobuf_size;
	u->buf = xcalloc(u->bufAlloced, sizeof(char));
    }
    buf = u->buf;
    bufAlloced = u->bufAlloced;
    *buf = '\0';

    errorCode[0] = '\0';
    
    do {
	int rc;

	/*
	 * Read next line from server.
	 */
	se = buf + bufLength;
	*se = '\0';
	rc = fdFgets(ctrl, se, (bufAlloced - bufLength));
	if (rc < 0) {
	    ec = FTPERR_BAD_SERVER_RESPONSE;
	    continue;
	} else if (rc == 0 || fdWritable(ctrl, 0) < 1)
	    moretodo = 0;

	/*
	 * Process next line from server.
	 */
	for (s = se; *s != '\0'; s = se) {
		const char *e;

		while (*se && *se != '\n') se++;

		if (se > s && se[-1] == '\r')
		   se[-1] = '\0';
		if (*se == '\0')
		    break;

if (_ftp_debug)
fprintf(stderr, "<- %s\n", s);

		/* HTTP: header termination on empty line */
		if (*s == '\0') {
		    moretodo = 0;
		    break;
		}
		*se++ = '\0';

		/* HTTP: look for "HTTP/1.1 123 ..." */
		if (!strncmp(s, "HTTP", sizeof("HTTP")-1)) {
		    ctrl->contentLength = -1;
		    if ((e = strchr(s, '.')) != NULL) {
			e++;
			u->httpVersion = *e - '0';
			if (u->httpVersion < 1 || u->httpVersion > 2)
			    ctrl->persist = u->httpVersion = 0;
			else
			    ctrl->persist = 1;
		    }
		    if ((e = strchr(s, ' ')) != NULL) {
			e++;
			if (strchr("0123456789", *e))
			    strncpy(errorCode, e, 3);
			errorCode[3] = '\0';
		    }
		    continue;
		}

		/* HTTP: look for "token: ..." */
		for (e = s; *e && !(*e == ' ' || *e == ':'); e++)
		    ;
		if (e > s && *e++ == ':') {
		    size_t ne = (e - s);
		    while (*e && *e == ' ') e++;
#if 0
		    if (!strncmp(s, "Date:", ne)) {
		    } else
		    if (!strncmp(s, "Server:", ne)) {
		    } else
		    if (!strncmp(s, "Last-Modified:", ne)) {
		    } else
		    if (!strncmp(s, "ETag:", ne)) {
		    } else
#endif
		    if (!strncmp(s, "Accept-Ranges:", ne)) {
			if (!strcmp(e, "bytes"))
			    u->httpHasRange = 1;
			if (!strcmp(e, "none"))
			    u->httpHasRange = 0;
		    } else
		    if (!strncmp(s, "Content-Length:", ne)) {
			if (strchr("0123456789", *e))
			    ctrl->contentLength = atoi(e);
		    } else
		    if (!strncmp(s, "Connection:", ne)) {
			if (!strcmp(e, "close"))
			    ctrl->persist = 0;
		    } else
#if 0
		    if (!strncmp(s, "Content-Type:", ne)) {
		    } else
		    if (!strncmp(s, "Transfer-Encoding:", ne)) {
			if (!strcmp(e, "chunked"))
			    ctrl->wr_chunked = 1;
			else
			    ctrl->wr_chunked = 0;
		    } else
		    if (!strncmp(s, "Allow:", ne)) {
		    } else
#endif
			;
		    continue;
		}

		/* HTTP: look for "<TITLE>501 ... </TITLE>" */
		if (!strncmp(s, "<TITLE>", sizeof("<TITLE>")-1))
		    s += sizeof("<TITLE>") - 1;

		/* FTP: look for "123-" and/or "123 " */
		if (strchr("0123456789", *s)) {
		    if (errorCode[0]) {
			if (!strncmp(s, errorCode, sizeof("123")-1) && s[3] == ' ')
			    moretodo = 0;
		    } else {
			strncpy(errorCode, s, sizeof("123")-1);
			errorCode[3] = '\0';
			if (s[3] != '-')
			    moretodo = 0;
		    }
		}
	}

	if (moretodo && se > s) {
	    bufLength = se - s - 1;
	    if (s != buf)
		memmove(buf, s, bufLength);
	} else {
	    bufLength = 0;
	}
    } while (moretodo && ec == 0);

    if (str)	*str = buf;
    if (ecp)	*ecp = atoi(errorCode);

    return ec;
}

static int ftpCheckResponse(urlinfo u, /*@out@*/ char ** str)
{
    int ec = 0;
    int rc;

    URLSANE(u);
    rc = checkResponse(u, u->ctrl, &ec, str);

    switch (ec) {
    case 550:
	return FTPERR_FILE_NOT_FOUND;
	/*@notreached@*/ break;
    case 552:
	return FTPERR_NIC_ABORT_IN_PROGRESS;
	/*@notreached@*/ break;
    default:
	if (ec >= 400 && ec <= 599) {
	    return FTPERR_BAD_SERVER_RESPONSE;
	}
	break;
    }
    return rc;
}

static int ftpCommand(urlinfo u, char ** str, ...)
{
    va_list ap;
    int len = 0;
    const char * s, * t;
    char * te;
    int rc;

    URLSANE(u);
    va_start(ap, str);
    while ((s = va_arg(ap, const char *)) != NULL) {
	if (len) len++;
	len += strlen(s);
    }
    len += sizeof("\r\n")-1;
    va_end(ap);

    t = te = alloca(len + 1);

    va_start(ap, str);
    while ((s = va_arg(ap, const char *)) != NULL) {
	if (te > t) *te++ = ' ';
	te = stpcpy(te, s);
    }
    te = stpcpy(te, "\r\n");
    va_end(ap);

if (_ftp_debug)
fprintf(stderr, "-> %s", t);
    if (fdWrite(u->ctrl, t, (te-t)) != (te-t))
	return FTPERR_SERVER_IO_ERROR;

    rc = ftpCheckResponse(u, str);
    return rc;
}

static int ftpLogin(urlinfo u)
{
    const char * host;
    const char * user;
    const char * password;
    int port;
    int rc;

    URLSANE(u);
    u->ctrl = fdLink(u->ctrl, "open ctrl");

    if (((host = (u->proxyh ? u->proxyh : u->host)) == NULL)) {
	rc = FTPERR_BAD_HOSTNAME;
	goto errxit;
    }

    if ((port = (u->proxyp > 0 ? u->proxyp : u->port)) < 0) port = IPPORT_FTP;

    if ((user = (u->proxyu ? u->proxyu : u->user)) == NULL)
	user = "anonymous";

    if ((password = u->password) == NULL) {
	if (getuid()) {
	    struct passwd * pw = getpwuid(getuid());
	    char *myp = alloca(strlen(pw->pw_name) + sizeof("@"));
	    strcpy(myp, pw->pw_name);
	    strcat(myp, "@");
	    password = myp;
	} else {
	    password = "root@";
	}
    }

    if (fdFileno(u->ctrl) >= 0 && fdWritable(u->ctrl, 0) < 1)
	fdClose(u->ctrl);

    if (fdFileno(u->ctrl) < 0) {
	rc = tcpConnect(u->ctrl, host, port);
	if (rc < 0)
	    goto errxit2;
    }

    if ((rc = ftpCheckResponse(u, NULL)))
	goto errxit;

    if ((rc = ftpCommand(u, NULL, "USER", user, NULL)))
	goto errxit;

    if ((rc = ftpCommand(u, NULL, "PASS", password, NULL)))
	goto errxit;

    if ((rc = ftpCommand(u, NULL, "TYPE", "I", NULL)))
	goto errxit;

    return 0;

errxit:
    fdSetSyserrno(u->ctrl, errno, ftpStrerror(rc));
errxit2:
    if (fdFileno(u->ctrl) >= 0)
	fdClose(u->ctrl);
    return rc;
}

static int ftpReq(FD_t data, const char * ftpCmd, const char * ftpArg)
{
    urlinfo u = data->url;
    struct sockaddr_in dataAddress;
    char * cmd;
    int cmdlen;
    char * passReply;
    char * chptr;
    int rc;

    URLSANE(u);
    if (ftpCmd == NULL)
	return FTPERR_UNKNOWN;	/* XXX W2DO? */

    cmdlen = strlen(ftpCmd) + (ftpArg ? 1+strlen(ftpArg) : 0) + sizeof("\r\n");
    chptr = cmd = alloca(cmdlen);
    chptr = stpcpy(chptr, ftpCmd);
    if (ftpArg) {
	*chptr++ = ' ';
	chptr = stpcpy(chptr, ftpArg);
    }
    chptr = stpcpy(chptr, "\r\n");
    cmdlen = chptr - cmd;

/*
 * Get the ftp version of the Content-Length.
 */
    if (!strncmp(cmd, "RETR", 4)) {
	unsigned cl;

	passReply = NULL;
	rc = ftpCommand(u, &passReply, "SIZE", ftpArg, NULL);
	if (rc)
	    goto errxit;
	if (sscanf(passReply, "%d %u", &rc, &cl) != 2) {
	    rc = FTPERR_BAD_SERVER_RESPONSE;
	    goto errxit;
	}
	rc = 0;
	data->contentLength = cl;
    }

    passReply = NULL;
    rc = ftpCommand(u, &passReply, "PASV", NULL);
    if (rc) {
	rc = FTPERR_PASSIVE_ERROR;
	goto errxit;
    }

    chptr = passReply;
    while (*chptr && *chptr != '(') chptr++;
    if (*chptr != '(') return FTPERR_PASSIVE_ERROR; 
    chptr++;
    passReply = chptr;
    while (*chptr && *chptr != ')') chptr++;
    if (*chptr != ')') return FTPERR_PASSIVE_ERROR;
    *chptr-- = '\0';

    while (*chptr && *chptr != ',') chptr--;
    if (*chptr != ',') return FTPERR_PASSIVE_ERROR;
    chptr--;
    while (*chptr && *chptr != ',') chptr--;
    if (*chptr != ',') return FTPERR_PASSIVE_ERROR;
    *chptr++ = '\0';
    
    /* now passReply points to the IP portion, and chptr points to the
       port number portion */

    {	int i, j;
	dataAddress.sin_family = AF_INET;
	if (sscanf(chptr, "%d,%d", &i, &j) != 2) {
	    rc = FTPERR_PASSIVE_ERROR;
	    goto errxit;
	}
	dataAddress.sin_port = htons((((unsigned)i) << 8) + j);
    }

    chptr = passReply;
    while (*chptr++) {
	if (*chptr == ',') *chptr = '.';
    }

    if (!inet_aton(passReply, &dataAddress.sin_addr)) {
	rc = FTPERR_PASSIVE_ERROR;
	goto errxit;
    }

    rc = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    fdSetFdno(data, (rc >= 0 ? rc : -1));
    if (rc < 0) {
	rc = FTPERR_FAILED_CONNECT;
	goto errxit;
    }
    data = fdLink(data, "open data (ftpReq)");

    /* XXX setsockopt SO_LINGER */
    /* XXX setsockopt SO_KEEPALIVE */
    /* XXX setsockopt SO_TOS IPTOS_THROUGHPUT */

    while (connect(fdFileno(data), (struct sockaddr *) &dataAddress, 
	        sizeof(dataAddress)) < 0) {
	if (errno == EINTR)
	    continue;
	rc = FTPERR_FAILED_DATA_CONNECT;
	goto errxit;
    }

if (_ftp_debug)
fprintf(stderr, "-> %s", cmd);
    if (fdWrite(u->ctrl, cmd, cmdlen) != cmdlen) {
	rc = FTPERR_SERVER_IO_ERROR;
	goto errxit;
    }

    if ((rc = ftpCheckResponse(u, NULL))) {
	goto errxit;
    }

    data->ftpFileDoneNeeded = 1;
    u->ctrl = fdLink(u->ctrl, "grab data (ftpReq)");
    u->ctrl = fdLink(u->ctrl, "open data (ftpReq)");
    return 0;

errxit:
    fdSetSyserrno(u->ctrl, errno, ftpStrerror(rc));
    if (fdFileno(data) >= 0)
	fdClose(data);
    return rc;
}

static int urlConnect(const char * url, /*@out@*/ urlinfo * uret)
{
    urlinfo u;
    int rc = 0;

    if (urlSplit(url, &u) < 0)
	return -1;

    if (u->urltype == URL_IS_FTP) {
	FD_t fd;

	if ((fd = u->ctrl) == NULL) {
	    fd = u->ctrl = fdNew("persist ctrl (urlConnect FTP)");
	    fdSetIo(u->ctrl, ufdio);
	}
	
	fd->rd_timeoutsecs = ftpTimeoutSecs;
	fd->contentLength = fd->bytesRemain = -1;
	fd->url = NULL;		/* XXX FTP ctrl has not */
	fd->ftpFileDoneNeeded = 0;
	fd = fdLink(fd, "grab ctrl (urlConnect FTP)");

	if (fdFileno(u->ctrl) < 0) {
	    rpmMessage(RPMMESS_DEBUG, _("logging into %s as %s, pw %s\n"),
			u->host,
			u->user ? u->user : "ftp",
			u->password ? u->password : "(username)");

	    if ((rc = ftpLogin(u)) < 0) {	/* XXX save ftpLogin error */
		u->ctrl = fdFree(fd, "grab ctrl (urlConnect FTP)");
		u->openError = rc;
	    }
	}
    }

    if (uret != NULL)
	*uret = urlLink(u, "urlConnect");
    u = urlFree(u, "urlSplit (urlConnect)");	

    return rc;
}

/*@null@*/ static rpmCallbackFunction	urlNotify = NULL;
/*@null@*/ static void *	urlNotifyData = NULL;
static int			urlNotifyCount = -1;

void urlSetCallback(rpmCallbackFunction notify, void *notifyData, int notifyCount) {
    urlNotify = notify;
    urlNotifyData = notifyData;
    urlNotifyCount = (notifyCount >= 0) ? notifyCount : 4096;
}

int ufdCopy(FD_t sfd, FD_t tfd)
{
    char buf[BUFSIZ];
    int itemsRead;
    int itemsCopied = 0;
    int rc = 0;
    int notifier = -1;

    if (urlNotify) {
	(void)(*urlNotify) (NULL, RPMCALLBACK_INST_OPEN_FILE,
		0, 0, NULL, urlNotifyData);
    }
    
    while (1) {
	rc = Fread(buf, sizeof(buf[0]), sizeof(buf), sfd);
	if (rc < 0)
	    break;
	else if (rc == 0) {
	    rc = itemsCopied;
	    break;
	}
	itemsRead = rc;
	rc = Fwrite(buf, sizeof(buf[0]), itemsRead, tfd);
	if (rc < 0)
	    break;
 	if (rc != itemsRead) {
	    rc = FTPERR_FILE_IO_ERROR;
	    break;
	}

	itemsCopied += itemsRead;
	if (urlNotify && urlNotifyCount > 0) {
	    int n = itemsCopied/urlNotifyCount;
	    if (n != notifier) {
		(void)(*urlNotify) (NULL, RPMCALLBACK_INST_PROGRESS,
			itemsCopied, 0, NULL, urlNotifyData);
		notifier = n;
	    }
	}
    }

    DBGIO(sfd, (stderr, "++ copied %d bytes: %s\n", itemsCopied,
	ftpStrerror(rc)));

    if (urlNotify) {
	(void)(*urlNotify) (NULL, RPMCALLBACK_INST_OPEN_FILE,
		itemsCopied, itemsCopied, NULL, urlNotifyData);
    }
    
    return rc;
}

int ufdGetFile(FD_t sfd, FD_t tfd)
{
    int rc;

    FDSANE(sfd);
    FDSANE(tfd);
    rc = ufdCopy(sfd, tfd);
    Fclose(sfd);
    if (rc > 0)		/* XXX ufdCopy now returns no. bytes copied */
	rc = 0;
    return rc;
}

static int ftpCmd(const char * cmd, const char * url, const char * arg2) {
    urlinfo u;
    int rc;
    const char * path;

    if (urlConnect(url, &u) < 0)
	return -1;

    (void) urlPath(url, &path);

    rc = ftpCommand(u, NULL, cmd, path, arg2, NULL);
    u->ctrl = fdFree(u->ctrl, "grab ctrl (ftpCmd)");
#ifdef	DYING
if (_rpmio_debug)
fprintf(stderr, "*** ftpCmd %s %s %s rc %d\n", cmd, path, arg2, rc);
#endif
    return rc;
}

static int ftpMkdir(const char * path, /*@unused@*/ mode_t mode) {
    int rc;
    if ((rc = ftpCmd("MKD", path, NULL)) != 0)
	return rc;
#if NOTYET
    {	char buf[20];
	sprintf(buf, " 0%o", mode);
	(void) ftpCmd("SITE CHMOD", path, buf);
    }
#endif
    return rc;
}

static int ftpChdir(const char * path) {
    return ftpCmd("CWD", path, NULL);
}

static int ftpRmdir(const char * path) {
    return ftpCmd("RMD", path, NULL);
}

static int ftpRename(const char * oldpath, const char * newpath) {
    int rc;
    if ((rc = ftpCmd("RNFR", oldpath, NULL)) != 0)
	return rc;
    return ftpCmd("RNTO", newpath, NULL);
}

static int ftpUnlink(const char * path) {
    return ftpCmd("DELE", path, NULL);
}

/* XXX these aren't worth the pain of including correctly */
#if !defined(IAC)
#define	IAC	255		/* interpret as command: */
#endif
#if !defined(IP)
#define	IP	244		/* interrupt process--permanently */
#endif
#if !defined(DM)
#define	DM	242		/* data mark--for connect. cleaning */
#endif
#if !defined(SHUT_RDWR)
#define	SHUT_RDWR	1+1
#endif

static int ftpAbort(urlinfo u, FD_t data) {
    static unsigned char ipbuf[3] = { IAC, IP, IAC };
    FD_t ctrl;
    int rc;
    int tosecs;

    URLSANE(u);

    if (data != NULL) {
	data->ftpFileDoneNeeded = 0;
	if (fdFileno(data) >= 0)
	    u->ctrl = fdFree(u->ctrl, "open data (ftpAbort)");
	u->ctrl = fdFree(u->ctrl, "grab data (ftpAbort)");
    }
    ctrl = u->ctrl;

    DBGIO(0, (stderr, "-> ABOR\n"));

    if (send(fdFileno(ctrl), ipbuf, sizeof(ipbuf), MSG_OOB) != sizeof(ipbuf)) {
	fdClose(ctrl);
	return FTPERR_SERVER_IO_ERROR;
    }

    sprintf(u->buf, "%cABOR\r\n",(char) DM);
    if (fdWrite(ctrl, u->buf, 7) != 7) {
	fdClose(ctrl);
	return FTPERR_SERVER_IO_ERROR;
    }

    if (data && fdFileno(data) >= 0) {
	/* XXX shorten data drain time wait */
	tosecs = data->rd_timeoutsecs;
	data->rd_timeoutsecs = 10;
	if (fdReadable(data, data->rd_timeoutsecs) > 0) {
	    while (timedRead(data, u->buf, u->bufAlloced) > 0)
		;
	}
	data->rd_timeoutsecs = tosecs;
	/* XXX ftp abort needs to close the data channel to receive status */
	shutdown(fdFileno(data), SHUT_RDWR);
	close(fdFileno(data));
	data->fps[0].fdno = -1;	/* XXX WRONG but expedient */
    }

    /* XXX shorten ctrl drain time wait */
    tosecs = u->ctrl->rd_timeoutsecs;
    u->ctrl->rd_timeoutsecs = 10;
    if ((rc = ftpCheckResponse(u, NULL)) == FTPERR_NIC_ABORT_IN_PROGRESS) {
	rc = ftpCheckResponse(u, NULL);
    }
    rc = ftpCheckResponse(u, NULL);
    u->ctrl->rd_timeoutsecs = tosecs;

    return rc;
}

static int ftpFileDone(urlinfo u, FD_t data)
{
    int rc = 0;

    URLSANE(u);
    assert(data->ftpFileDoneNeeded);

    if (data->ftpFileDoneNeeded) {
	data->ftpFileDoneNeeded = 0;
	u->ctrl = fdFree(u->ctrl, "open data (ftpFileDone)");
	u->ctrl = fdFree(u->ctrl, "grab data (ftpFileDone)");
	rc = ftpCheckResponse(u, NULL);
    }
    return rc;
}

static int httpResp(urlinfo u, FD_t ctrl, /*@out@*/ char ** str)
{
    int ec = 0;
    int rc;

    URLSANE(u);
    rc = checkResponse(u, ctrl, &ec, str);

if (_ftp_debug && !(rc == 0 && ec == 200))
fprintf(stderr, "*** httpResp: rc %d ec %d\n", rc, ec);

    switch (ec) {
    case 200:
	break;
    default:
	rc = FTPERR_FILE_NOT_FOUND;
	break;
    }

    return rc;
}

static int httpReq(FD_t ctrl, const char * httpCmd, const char * httpArg)
{
    urlinfo u = ctrl->url;
    const char * host;
    const char * path;
    int port;
    int rc;
    char * req;
    size_t len;
    int retrying = 0;

    URLSANE(u);
    assert(ctrl != NULL);

    if (((host = (u->proxyh ? u->proxyh : u->host)) == NULL))
	return FTPERR_BAD_HOSTNAME;

    if ((port = (u->proxyp > 0 ? u->proxyp : u->port)) < 0) port = 80;
    path = (u->proxyh || u->proxyp > 0) ? u->url : httpArg;

reopen:
    if (fdFileno(ctrl) >= 0 && (rc = fdWritable(ctrl, 0)) < 1) {
#ifdef	DYING
if (_ftp_debug)
fprintf(stderr, "*** httpReq closing ctrl fdno %d rc %d\n", fdFileno(ctrl), rc);
#endif
	fdClose(ctrl);
    }

    if (fdFileno(ctrl) < 0) {
	rc = tcpConnect(ctrl, host, port);
	if (rc < 0)
	    goto errxit2;
	ctrl = fdLink(ctrl, "open ctrl (httpReq)");
    }

    len = sizeof("\
req x HTTP/1.0\r\n\
User-Agent: rpm/3.0.4\r\n\
Host: y:z\r\n\
Accept: text/plain\r\n\
Transfer-Encoding: chunked\r\n\
\r\n\
") + strlen(httpCmd) + strlen(path) + sizeof(VERSION) + strlen(host) + 20;

    req = alloca(len);
    *req = '\0';

  if (!strcmp(httpCmd, "PUT")) {
    sprintf(req, "\
%s %s HTTP/1.%d\r\n\
User-Agent: rpm/%s\r\n\
Host: %s:%d\r\n\
Accept: text/plain\r\n\
Transfer-Encoding: chunked\r\n\
\r\n\
",	httpCmd, path, (u->httpVersion ? 1 : 0), VERSION, host, port);
} else {
    sprintf(req, "\
%s %s HTTP/1.%d\r\n\
User-Agent: rpm/%s\r\n\
Host: %s:%d\r\n\
Accept: text/plain\r\n\
\r\n\
",	httpCmd, path, (u->httpVersion ? 1 : 0), VERSION, host, port);
}

if (_ftp_debug)
fprintf(stderr, "-> %s", req);

    len = strlen(req);
    if (fdWrite(ctrl, req, len) != len) {
	rc = FTPERR_SERVER_IO_ERROR;
	goto errxit;
    }

    if (!strcmp(httpCmd, "PUT")) {
	ctrl->wr_chunked = 1;
    } else {

	rc = httpResp(u, ctrl, NULL);

	if (rc) {
	    if (!retrying) {	/* not HTTP_OK */
#ifdef DYING
if (_ftp_debug)
fprintf(stderr, "*** httpReq ctrl %p reopening ...\n", ctrl);
#endif
		retrying = 1;
		fdClose(ctrl);
		goto reopen;
	    }
	    goto errxit;
	}
    }

    ctrl = fdLink(ctrl, "open data (httpReq)");
    return 0;

errxit:
    fdSetSyserrno(ctrl, errno, ftpStrerror(rc));
errxit2:
    if (fdFileno(ctrl) >= 0)
	fdClose(ctrl);
    return rc;
}

/* XXX DYING: unused */
void * ufdGetUrlinfo(FD_t fd) {
    FDSANE(fd);
    if (fd->url == NULL)
	return NULL;
    return urlLink(fd->url, "ufdGetUrlinfo");
}

/* =============================================================== */
static ssize_t ufdRead(void * cookie, /*@out@*/ char * buf, size_t count) {
    FD_t fd = c2f(cookie);
    int bytesRead;
    int total;

    /* XXX preserve timedRead() behavior */
    if (fdGetIo(fd) == fdio) {
	struct stat sb;
	int fdno = fdFileno(fd);
	fstat(fdno, &sb);
	if (S_ISREG(sb.st_mode))
	    return fdRead(fd, buf, count);
    }

    UFDONLY(fd);
    assert(fd->rd_timeoutsecs >= 0);

    for (total = 0; total < count; total += bytesRead) {

	int rc;

	bytesRead = 0;

	/* Is there data to read? */
	if (fd->bytesRemain == 0) return total;	/* XXX simulate EOF */
	rc = fdReadable(fd, fd->rd_timeoutsecs);

	switch (rc) {
	case -1:	/* error */
	case  0:	/* timeout */
	    return total;
	    /*@notreached@*/ break;
	default:	/* data to read */
	    break;
	}

	rc = fdRead(fd, buf + total, count - total);

	if (rc < 0) {
	    switch (errno) {
	    case EWOULDBLOCK:
		continue;
		/*@notreached@*/ break;
	    default:
		break;
	    }
if (_rpmio_debug)
fprintf(stderr, "*** read: rc %d errno %d %s \"%s\"\n", rc, errno, strerror(errno), buf);
	    return rc;
	    /*@notreached@*/ break;
	} else if (rc == 0) {
	    return total;
	    /*@notreached@*/ break;
	}
	bytesRead = rc;
    }

    return count;
}

static ssize_t ufdWrite(void * cookie, const char * buf, size_t count)
{
    FD_t fd = c2f(cookie);
    int bytesWritten;
    int total = 0;

#ifdef	NOTYET
    if (fdGetIo(fd) == fdio) {
	struct stat sb;
	fstat(fdGetFdno(fd), &sb);
	if (S_ISREG(sb.st_mode))
	    return fdWrite(fd, buf, count);
    }
#endif

    UFDONLY(fd);

    for (total = 0; total < count; total += bytesWritten) {

	int rc;

	bytesWritten = 0;

	/* Is there room to write data? */
	if (fd->bytesRemain == 0) {
fprintf(stderr, "*** ufdWrite fd %p WRITE PAST END OF CONTENT\n", fd);
	    return total;	/* XXX simulate EOF */
	}
	rc = fdWritable(fd, 2);		/* XXX configurable? */

	switch (rc) {
	case -1:	/* error */
	case  0:	/* timeout */
	    return total;
	    /*@notreached@*/ break;
	default:	/* data to write */
	    break;
	}

	rc = fdWrite(fd, buf + total, count - total);

	if (rc < 0) {
	    switch (errno) {
	    case EWOULDBLOCK:
		continue;
		/*@notreached@*/ break;
	    default:
		break;
	    }
if (_rpmio_debug)
fprintf(stderr, "*** write: rc %d errno %d %s \"%s\"\n", rc, errno, strerror(errno), buf);
	    return rc;
	    /*@notreached@*/ break;
	} else if (rc == 0) {
	    return total;
	    /*@notreached@*/ break;
	}
	bytesWritten = rc;
    }

    return count;
}

#ifdef USE_COOKIE_SEEK_POINTER
static inline int ufdSeek(void * cookie, _IO_off64_t *pos, int whence) {
#else
static inline int ufdSeek(void * cookie, off_t pos, int whence) {
#endif
    FD_t fd = c2f(cookie);

    switch (fd->urlType) {
    case URL_IS_UNKNOWN:
    case URL_IS_PATH:
	break;
    case URL_IS_DASH:
    case URL_IS_FTP:
    case URL_IS_HTTP:
    default:
        return -2;
	/*@notreached@*/ break;
    }
    return fdSeek(cookie, pos, whence);
}

static int ufdClose( /*@only@*/ void * cookie)
{
    FD_t fd = c2f(cookie);

    UFDONLY(fd);

    if (fd->url) {
	urlinfo u = fd->url;

	if (fd == u->data)
		fd = u->data = fdFree(fd, "grab data (ufdClose persist)");
	else
		fd = fdFree(fd, "grab data (ufdClose)");
	(void) urlFree(fd->url, "url (ufdClose)");
	fd->url = NULL;
	u->ctrl = fdFree(u->ctrl, "grab ctrl (ufdClose)");

	if (u->urltype == URL_IS_FTP) {

	    /* XXX if not using libio, lose the fp from fpio */
	    {   FILE * fp = fdGetFILE(fd);
		if (noLibio && fp)
		    fdSetFp(fd, NULL);
	    }

	    /*
	     * Normal FTP has 4 refs on the data fd:
	     *	"persist data (ufdOpen FTP)"		rpmio.c:888
	     *	"grab data (ufdOpen FTP)"		rpmio.c:892
	     *	"open data (ftpReq)"			ftp.c:633
	     *	"fopencookie"				rpmio.c:1507
	     */

	    /*
	     * Normal FTP has 5 refs on the ctrl fd:
	     *	"persist ctrl"				url.c:176
	     *	"grab ctrl (urlConnect FTP)"		rpmio.c:404
	     *	"open ctrl"				ftp.c:504
	     *	"grab data (ftpReq)"			ftp.c:661
	     *	"open data (ftpReq)"			ftp.c:662
	     */
	    if (fd->bytesRemain > 0) {
		if (fd->ftpFileDoneNeeded) {
		    if (fdReadable(u->ctrl, 0) > 0)
			ftpFileDone(u, fd);
		    else
			ftpAbort(u, fd);
		}
	    } else {
		int rc;
		/* XXX STOR et al require close before ftpFileDone */
		rc = fdClose(fd);
#if 0	/* XXX error exit from ufdOpen does not have this set */
		assert(fd->ftpFileDoneNeeded != 0);
#endif
		if (fd->ftpFileDoneNeeded)
		    ftpFileDone(u, fd);
		return rc;
	    }
	}

	if (!strcmp(u->service, "http")) {
	    if (fd->wr_chunked) {
		int rc;
	    /* XXX HTTP PUT requires terminating 0 length chunk. */
		fdWrite(fd, NULL, 0);
		fd->wr_chunked = 0;
	    /* XXX HTTP PUT requires terminating entity-header. */
if (_ftp_debug)
fprintf(stderr, "-> \r\n");
		(void) fdWrite(fd, "\r\n", sizeof("\r\n")-1);
		rc = httpResp(u, fd, NULL);
	    }

	    if (fd == u->ctrl)
		fd = u->ctrl = fdFree(fd, "open data (ufdClose HTTP persist ctrl)");
	    else if (fd == u->data)
		fd = u->data = fdFree(fd, "open data (ufdClose HTTP persist data)");
	    else
		fd = fdFree(fd, "open data (ufdClose HTTP)");

	    /*
	     * HTTP has 4 (or 5 if persistent malloc) refs on the fd:
	     *	"persist ctrl"				url.c:177
	     *	"grab ctrl (ufdOpen HTTP)"		rpmio.c:924
	     *	"grab data (ufdOpen HTTP)"		rpmio.c:928
	     *	"open ctrl (httpReq)"			ftp.c:382
	     *	"open data (httpReq)"			ftp.c:435
	     */

	    /* XXX if not using libio, lose the fp from fpio */
	    {   FILE * fp = fdGetFILE(fd);
		if (noLibio && fp)
		    fdSetFp(fd, NULL);
	    }

	    if (fd->persist && u->httpVersion &&
		(fd == u->ctrl || fd == u->data) && fd->bytesRemain == 0) {
		fd->contentLength = fd->bytesRemain = -1;
		return 0;
	    } else {
		fd->contentLength = fd->bytesRemain = -1;
	    }
	}
    }
    return fdClose(fd);
}

static /*@null@*/ FD_t ftpOpen(const char *url, /*@unused@*/ int flags,
		/*@unused@*/ mode_t mode, /*@out@*/ urlinfo *uret)
{
    urlinfo u = NULL;
    FD_t fd = NULL;

#if 0	/* XXX makeTempFile() heartburn */
    assert(!(flags & O_RDWR));
#endif
    if (urlConnect(url, &u) < 0)
	goto exit;

    if (u->data == NULL)
	u->data = fdNew("persist data (ftpOpen)");

    if (u->data->url == NULL)
	fd = fdLink(u->data, "grab data (ftpOpen persist data)");
    else
	fd = fdNew("grab data (ftpOpen)");

    if (fd) {
	fdSetIo(fd, ufdio);
	fd->ftpFileDoneNeeded = 0;
	fd->rd_timeoutsecs = ftpTimeoutSecs;
	fd->contentLength = fd->bytesRemain = -1;
	fd->url = urlLink(u, "url (ufdOpen FTP)");
	fd->urlType = URL_IS_FTP;
    }

exit:
    if (uret)
	*uret = u;
    return fd;
}

static /*@null@*/ FD_t httpOpen(const char *url, int flags, mode_t mode,
		/*@out@*/ urlinfo *uret)
{
    urlinfo u = NULL;
    FD_t fd = NULL;

#if 0	/* XXX makeTempFile() heartburn */
    assert(!(flags & O_RDWR));
#endif
    if (urlSplit(url, &u))
	goto exit;

    if (u->ctrl == NULL)
	u->ctrl = fdNew("persist ctrl (httpOpen)");
    if (u->ctrl->nrefs > 2 && u->data == NULL)
	u->data = fdNew("persist data (httpOpen)");

    if (u->ctrl->url == NULL)
	fd = fdLink(u->ctrl, "grab ctrl (httpOpen persist ctrl)");
    else if (u->data->url == NULL)
	fd = fdLink(u->data, "grab ctrl (httpOpen persist data)");
    else
	fd = fdNew("grab ctrl (httpOpen)");

    if (fd) {
	fdSetIo(fd, ufdio);
	fd->ftpFileDoneNeeded = 0;
	fd->rd_timeoutsecs = httpTimeoutSecs;
	fd->contentLength = fd->bytesRemain = -1;
	fd->url = urlLink(u, "url (httpOpen)");
	fd = fdLink(fd, "grab data (httpOpen)");
	fd->urlType = URL_IS_HTTP;
    }

exit:
    if (uret)
	*uret = u;
    return fd;
}

static /*@null@*/ FD_t ufdOpen(const char *url, int flags, mode_t mode)
{
    FD_t fd = NULL;
    const char * cmd;
    urlinfo u;
    const char * path;
    urltype urlType = urlPath(url, &path);

if (_rpmio_debug)
fprintf(stderr, "*** ufdOpen(%s,0x%x,0%o)\n", url, flags, (unsigned)mode);

    switch (urlType) {
    case URL_IS_FTP:
	fd = ftpOpen(url, flags, mode, &u);
	if (fd == NULL || u == NULL)
	    break;

	/* XXX W2DO? use STOU rather than STOR to prevent clobbering */
	cmd = ((flags & O_WRONLY) 
		?  ((flags & O_APPEND) ? "APPE" :
		   ((flags & O_CREAT) ? "STOR" : "STOR"))
		:  ((flags & O_CREAT) ? "STOR" : "RETR"));
	u->openError = ftpReq(fd, cmd, path);
	if (u->openError < 0) {
	    /* XXX make sure that we can exit through ufdClose */
	    fd = fdLink(fd, "error data (ufdOpen FTP)");
	} else {
	    fd->bytesRemain = ((!strcmp(cmd, "RETR"))
		?  fd->contentLength : -1);
	    fd->wr_chunked = 0;
	}
	break;
    case URL_IS_HTTP:
	fd = httpOpen(url, flags, mode, &u);
	if (fd == NULL || u == NULL)
	    break;

	cmd = ((flags & O_WRONLY)
		?  ((flags & O_APPEND) ? "PUT" :
		   ((flags & O_CREAT) ? "PUT" : "PUT"))
		: "GET");
	u->openError = httpReq(fd, cmd, path);
	if (u->openError < 0) {
	    /* XXX make sure that we can exit through ufdClose */
	    fd = fdLink(fd, "error ctrl (ufdOpen HTTP)");
	    fd = fdLink(fd, "error data (ufdOpen HTTP)");
	} else {
	    fd->bytesRemain = ((!strcmp(cmd, "GET"))
		?  fd->contentLength : -1);
	    fd->wr_chunked = ((!strcmp(cmd, "PUT"))
		?  fd->wr_chunked : 0);
	}
	break;
    case URL_IS_DASH:
	assert(!(flags & O_RDWR));
	fd = fdDup( ((flags & O_WRONLY) ? STDOUT_FILENO : STDIN_FILENO) );
	if (fd) {
	    fdSetIo(fd, ufdio);
	    fd->rd_timeoutsecs = 600;	/* XXX W2DO? 10 mins? */
	    fd->contentLength = fd->bytesRemain = -1;
	}
	break;
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
    default:
	fd = fdOpen(path, flags, mode);
	if (fd) {
	    fdSetIo(fd, ufdio);
	    fd->rd_timeoutsecs = 1;
	    fd->contentLength = fd->bytesRemain = -1;
	}
	break;
    }

    if (fd == NULL) return NULL;
    fd->urlType = urlType;
    if (Fileno(fd) < 0) {
	ufdClose(fd);
	return NULL;
    }
DBGIO(fd, (stderr, "==>\tufdOpen(\"%s\",%x,0%o) %s\n", url, flags, (unsigned)mode, fdbg(fd)));
    return fd;
}

static struct FDIO_s ufdio_s = {
  ufdRead, ufdWrite, ufdSeek, ufdClose, XfdLink, XfdFree, XfdNew, fdFileno,
  ufdOpen, NULL, fdGetFp, NULL,	Mkdir, Chdir, Rmdir, Rename, Unlink
};
FDIO_t ufdio = /*@-compmempass@*/ &ufdio_s /*@=compmempass@*/ ;

/* =============================================================== */
/* Support for first fit File Allocation I/O.
 */
long int fadGetFileSize(FD_t fd) {
    FDSANE(fd);
    return fd->fileSize;
}

void fadSetFileSize(FD_t fd, long int fileSize) {
    FDSANE(fd);
    fd->fileSize = fileSize;
}

unsigned int fadGetFirstFree(FD_t fd) {
    FDSANE(fd);
    return fd->firstFree;
}

void fadSetFirstFree(FD_t fd, unsigned int firstFree) {
    FDSANE(fd);
    fd->firstFree = firstFree;
}

/* =============================================================== */
#ifdef	DYING
extern fdio_open_function_t fadOpen;
static struct FDIO_s fadio_s = {
  fdRead, fdWrite, fdSeek, fdClose, XfdLink, XfdFree, XfdNew, fdFileno,
  fadOpen, NULL, fdGetFp,	NULL, NULL, NULL, NULL, NULL
};
FDIO_t fadio = /*@-compmempass@*/ &fadio_s /*@=compmempass@*/ ;
#else
extern FDIO_t fadio;
#endif

/* =============================================================== */
/* Support for GZIP library.
 */
#ifdef	HAVE_ZLIB_H

#include <zlib.h>

static inline /*@dependent@*/ /*@null@*/ void * gzdFileno(FD_t fd) {
    void * rc = NULL;
    int i;

    FDSANE(fd);
    for (i = fd->nfps; i >= 0; i--) {
	FDSTACK_t * fps = &fd->fps[i];
	if (fps->io != gzdio)
	    continue;
	rc = fps->fp;
	break;
    }
    
    return rc;
}

static /*@null@*/ FD_t gzdOpen(const char *path, const char *fmode) {
    FD_t fd;
    gzFile *gzfile;
    if ((gzfile = gzopen(path, fmode)) == NULL)
	return NULL;
    fd = fdNew("open (gzdOpen)");
    fdPop(fd); fdPush(fd, gzdio, gzfile, -1);
    
DBGIO(fd, (stderr, "==>\tgzdOpen(\"%s\", \"%s\") fd %p %s\n", path, fmode, fd, fdbg(fd)));
    return fdLink(fd, "gzdOpen");
}

static /*@null@*/ FD_t gzdFdopen(void * cookie, const char *fmode) {
    FD_t fd = c2f(cookie);
    int fdno;
    gzFile *gzfile;

    if (fmode == NULL) return NULL;
    fdno = fdFileno(fd);
    fdSetFdno(fd, -1);		/* XXX skip the fdio close */
    if (fdno < 0) return NULL;
    gzfile = gzdopen(fdno, fmode);
    if (gzfile == NULL) return NULL;

    fdPush(fd, gzdio, gzfile, fdno);		/* Push gzdio onto stack */

    return fdLink(fd, "gzdFdopen");
}

static int gzdFlush(FD_t fd) {
    return gzflush(gzdFileno(fd), Z_SYNC_FLUSH);	/* XXX W2DO? */
}

/* =============================================================== */
static ssize_t gzdRead(void * cookie, /*@out@*/ char * buf, size_t count) {
    FD_t fd = c2f(cookie);
    gzFile *gzfile;
    ssize_t rc;

    if (fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */
    gzfile = gzdFileno(fd);
    fdstat_enter(fd, FDSTAT_READ);
    rc = gzread(gzfile, buf, count);
DBGIO(fd, (stderr, "==>\tgzdRead(%p,%p,%u) rc %lx %s\n", cookie, buf, (unsigned)count, (long)rc, fdbg(fd)));
    if (rc < 0) {
	int zerror = 0;
	fd->errcookie = gzerror(gzfile, &zerror);
	if (zerror == Z_ERRNO) {
	    fd->syserrno = errno;
	    fd->errcookie = strerror(fd->syserrno);
	}
    } else if (rc >= 0) {
	fdstat_exit(fd, FDSTAT_READ, rc);
    }
    return rc;
}

static ssize_t gzdWrite(void * cookie, const char * buf, size_t count) {
    FD_t fd = c2f(cookie);
    gzFile *gzfile;
    ssize_t rc;

    if (fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */
    gzfile = gzdFileno(fd);
    fdstat_enter(fd, FDSTAT_WRITE);
    rc = gzwrite(gzfile, (void *)buf, count);
DBGIO(fd, (stderr, "==>\tgzdWrite(%p,%p,%u) rc %lx %s\n", cookie, buf, (unsigned)count, (long)rc, fdbg(fd)));
    if (rc < 0) {
	int zerror = 0;
	fd->errcookie = gzerror(gzfile, &zerror);
	if (zerror == Z_ERRNO) {
	    fd->syserrno = errno;
	    fd->errcookie = strerror(fd->syserrno);
	}
    } else if (rc > 0) {
	fdstat_exit(fd, FDSTAT_WRITE, rc);
    }
    return rc;
}

/* XXX zlib-1.0.4 has not */
#ifdef USE_COOKIE_SEEK_POINTER
static inline int gzdSeek(void * cookie, _IO_off64_t *pos, int whence) {
    _IO_off64_t p = *pos;
#else
static inline int gzdSeek(void * cookie, off_t p, int whence) {
#endif
    int rc;
#if HAVE_GZSEEK
    FD_t fd = c2f(cookie);
    gzFile *gzfile;

    assert(fd->bytesRemain == -1);	/* XXX FIXME */
    gzfile = gzdFileno(fd);
    fdstat_enter(fd, FDSTAT_SEEK);
    rc = gzseek(gzfile, p, whence);
DBGIO(fd, (stderr, "==>\tgzdSeek(%p,%ld,%d) rc %lx %s\n", cookie, (long)p, whence, (long)rc, fdbg(fd)));
    if (rc < 0) {
	int zerror = 0;
	fd->errcookie = gzerror(gzfile, &zerror);
	if (zerror == Z_ERRNO) {
	    fd->syserrno = errno;
	    fd->errcookie = strerror(fd->syserrno);
	}
    } else if (rc >= 0) {
	fdstat_exit(fd, FDSTAT_SEEK, rc);
    }
#else
    rc = -2;
#endif
    return rc;
}

static int gzdClose( /*@only@*/ void * cookie) {
    FD_t fd = c2f(cookie);
    gzFile *gzfile;
    int rc;

    gzfile = gzdFileno(fd);

    if (gzfile == NULL) return -2;
    fdstat_enter(fd, FDSTAT_CLOSE);
    rc = gzclose(gzfile);

    /* XXX TODO: preserve fd if errors */

    if (fd) {
DBGIO(fd, (stderr, "==>\tgzdClose(%p) zerror %d %s\n", cookie, rc, fdbg(fd)));
	if (rc < 0) {
	    fd->errcookie = gzerror(gzfile, &rc);
	    if (rc == Z_ERRNO) {
		fd->syserrno = errno;
		fd->errcookie = strerror(fd->syserrno);
	    }
	} else if (rc >= 0) {
	    fdstat_exit(fd, FDSTAT_CLOSE, rc);
	}
    }

DBGIO(fd, (stderr, "==>\tgzdClose(%p) rc %lx %s\n", cookie, (long)rc, fdbg(fd)));

    if (_rpmio_debug || rpmIsDebug()) fdstat_print(fd, "GZDIO", stderr);
    if (rc == 0)
	fd = fdFree(fd, "open (gzdClose)");
    return rc;
}

static struct FDIO_s gzdio_s = {
  gzdRead, gzdWrite, gzdSeek, gzdClose, XfdLink, XfdFree, XfdNew, fdFileno,
  NULL, gzdOpen, gzdFileno, gzdFlush,	NULL, NULL, NULL, NULL, NULL
};
FDIO_t gzdio = /*@-compmempass@*/ &gzdio_s /*@=compmempass@*/ ;

#endif	/* HAVE_ZLIB_H */

/* =============================================================== */
/* Support for BZIP2 library.
 */
#if HAVE_BZLIB_H

#include <bzlib.h>

#ifdef HAVE_BZ2_1_0
# define bzopen  BZ2_bzopen
# define bzclose BZ2_bzclose
# define bzdopen BZ2_bzdopen
# define bzerror BZ2_bzerror
# define bzflush BZ2_bzflush
# define bzread  BZ2_bzread
# define bzwrite BZ2_bzwrite
#endif /* HAVE_BZ2_1_0 */

static inline /*@dependent@*/ /*@null@*/ void * bzdFileno(FD_t fd) {
    void * rc = NULL;
    int i;

    FDSANE(fd);
    for (i = fd->nfps; i >= 0; i--) {
	FDSTACK_t * fps = &fd->fps[i];
	if (fps->io != bzdio)
	    continue;
	rc = fps->fp;
	break;
    }
    
    return rc;
}

static /*@null@*/ FD_t bzdOpen(const char *path, const char *mode) {
    FD_t fd;
    BZFILE *bzfile;;
    if ((bzfile = bzopen(path, mode)) == NULL)
	return NULL;
    fd = fdNew("open (bzdOpen)");
    fdPop(fd); fdPush(fd, bzdio, bzfile, -1);
    return fdLink(fd, "bzdOpen");
}

static /*@null@*/ FD_t bzdFdopen(void * cookie, const char * fmode) {
    FD_t fd = c2f(cookie);
    int fdno;
    BZFILE *bzfile;

    if (fmode == NULL) return NULL;
    fdno = fdFileno(fd);
    fdSetFdno(fd, -1);		/* XXX skip the fdio close */
    if (fdno < 0) return NULL;
    bzfile = bzdopen(fdno, fmode);
    if (bzfile == NULL) return NULL;

    fdPush(fd, bzdio, bzfile, fdno);		/* Push bzdio onto stack */

    return fdLink(fd, "bzdFdopen");
}

static int bzdFlush(FD_t fd) {
    return bzflush(bzdFileno(fd));
}

/* =============================================================== */
static ssize_t bzdRead(void * cookie, /*@out@*/ char * buf, size_t count) {
    FD_t fd = c2f(cookie);
    BZFILE *bzfile;
    ssize_t rc;

    if (fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */
    bzfile = bzdFileno(fd);
    fdstat_enter(fd, FDSTAT_READ);
    rc = bzread(bzfile, buf, count);
    if (rc == -1) {
	int zerror = 0;
	fd->errcookie = bzerror(bzfile, &zerror);
    } else if (rc >= 0) {
	fdstat_exit(fd, FDSTAT_READ, rc);
    }
    return rc;
}

static ssize_t bzdWrite(void * cookie, const char * buf, size_t count) {
    FD_t fd = c2f(cookie);
    BZFILE *bzfile;
    ssize_t rc;

    if (fd->bytesRemain == 0) return 0;	/* XXX simulate EOF */
    bzfile = bzdFileno(fd);
    fdstat_enter(fd, FDSTAT_WRITE);
    rc = bzwrite(bzfile, (void *)buf, count);
    if (rc == -1) {
	int zerror = 0;
	fd->errcookie = bzerror(bzfile, &zerror);
    } else if (rc > 0) {
	fdstat_exit(fd, FDSTAT_WRITE, rc);
    }
    return rc;
}

#ifdef USE_COOKIE_SEEK_POINTER
static inline int bzdSeek(void * cookie, _IO_off64_t *pos, int whence) {
#else
static inline int bzdSeek(void * cookie, off_t p, int whence) {
#endif
    FD_t fd = c2f(cookie);

    BZDONLY(fd);
    return -2;
}

static int bzdClose( /*@only@*/ void * cookie) {
    FD_t fd = c2f(cookie);
    BZFILE *bzfile;
    int rc;

    bzfile = bzdFileno(fd);

    if (bzfile == NULL) return -2;
    fdstat_enter(fd, FDSTAT_CLOSE);
    bzclose(bzfile);
    rc = 0;	/* XXX FIXME */

    /* XXX TODO: preserve fd if errors */

    if (fd) {
	if (rc == -1) {
	    int zerror = 0;
	    fd->errcookie = bzerror(bzfile, &zerror);
	} else if (rc >= 0) {
	    fdstat_exit(fd, FDSTAT_CLOSE, rc);
	}
    }

DBGIO(fd, (stderr, "==>\tbzdClose(%p) rc %lx %s\n", cookie, (long)rc, fdbg(fd)));

    if (_rpmio_debug || rpmIsDebug()) fdstat_print(fd, "BZDIO", stderr);
    if (rc == 0)
	fd = fdFree(fd, "open (bzdClose)");
    return rc;
}

static struct FDIO_s bzdio_s = {
  bzdRead, bzdWrite, bzdSeek, bzdClose, XfdLink, XfdFree, XfdNew, fdFileno,
  NULL, bzdOpen, bzdFileno, bzdFlush,	NULL, NULL, NULL, NULL, NULL
};
FDIO_t bzdio = /*@-compmempass@*/ &bzdio_s /*@=compmempass@*/ ;

#endif	/* HAVE_BZLIB_H */

/* =============================================================== */
/*@observer@*/ static const char * getFdErrstr (FD_t fd) {
    const char *errstr = NULL;

#ifdef	HAVE_ZLIB_H
    if (fdGetIo(fd) == gzdio) {
	errstr = fd->errcookie;
    } else
#endif	/* HAVE_ZLIB_H */

#ifdef	HAVE_BZLIB_H
    if (fdGetIo(fd) == bzdio) {
	errstr = fd->errcookie;
    } else
#endif	/* HAVE_BZLIB_H */

    {
	errstr = strerror(fd->syserrno);
    }

    return errstr;
}

/* =============================================================== */

const char *Fstrerror(FD_t fd) {
    if (fd == NULL)
	return strerror(errno);
    FDSANE(fd);
    return getFdErrstr(fd);
}

#define	FDIOVEC(_fd, _vec)	\
  ((fdGetIo(_fd) && fdGetIo(_fd)->_vec) ? fdGetIo(_fd)->_vec : NULL)

size_t Fread(void *buf, size_t size, size_t nmemb, FD_t fd) {
    fdio_read_function_t *_read;
    int rc;

    FDSANE(fd);
#ifdef __LCLINT__
    *(char *)buf = '\0';
#endif
DBGIO(fd, (stderr, "==> Fread(%p,%u,%u,%p) %s\n", buf, (unsigned)size, (unsigned)nmemb, fd, fdbg(fd)));

    if (fdGetIo(fd) == fpio) {
	rc = fread(buf, size, nmemb, fdGetFILE(fd));
	return rc;
    }

    _read = FDIOVEC(fd, read);

    rc = (_read ? (*_read) (fd, buf, size * nmemb) : -2);
    return rc;
}

size_t Fwrite(const void *buf, size_t size, size_t nmemb, FD_t fd) {
    fdio_write_function_t *_write;
    int rc;

    FDSANE(fd);
DBGIO(fd, (stderr, "==> Fwrite(%p,%u,%u,%p) %s\n", buf, (unsigned)size, (unsigned)nmemb, fd, fdbg(fd)));

    if (fdGetIo(fd) == fpio) {
	rc = fwrite(buf, size, nmemb, fdGetFILE(fd));
	return rc;
    }

    _write = FDIOVEC(fd, write);

    rc = (_write ? _write(fd, buf, size * nmemb) : -2);
    return rc;
}

#ifdef USE_COOKIE_SEEK_POINTER
int Fseek(FD_t fd, _IO_off64_t offset, int whence) {
#else 
int Fseek(FD_t fd, off_t offset, int whence) {
#endif
    fdio_seek_function_t *_seek;
    long int rc;

    FDSANE(fd);
DBGIO(fd, (stderr, "==> Fseek(%p,%ld,%d) %s\n", fd, (long)offset, whence, fdbg(fd)));

    if (fdGetIo(fd) == fpio) {
	FILE *f;

	f = fdGetFILE(fd);
	rc = fseek(f, offset, whence);
	return rc;
    }

    _seek = FDIOVEC(fd, seek);

#ifdef USE_COOKIE_SEEK_POINTER
    rc = (_seek ? _seek(fd, &offset, whence) : -2);
#else
    rc = (_seek ? _seek(fd, offset, whence) : -2);
#endif
    return rc;
}

int Fclose(FD_t fd) {
    int rc, ec = 0;

    FDSANE(fd);
DBGIO(fd, (stderr, "==> Fclose(%p) %s\n", fd, fdbg(fd)));

    fd = fdLink(fd, "Fclose");
    while (fd->nfps >= 0) {
	FDSTACK_t * fps = &fd->fps[fd->nfps];
	
	if (fps->io == fpio) {
	    FILE *fp = fdGetFILE(fd);
	    int fpno = fileno(fp);

	/* XXX persistent HTTP/1.1 returns the previously opened fp */
	    if (fd->nfps > 0 && fpno == -1 &&
		fd->fps[fd->nfps-1].io == ufdio &&
		fd->fps[fd->nfps-1].fp == fp &&
		fd->fps[fd->nfps-1].fdno >= 0)
	    {
		fflush(fp);
		fd->nfps--;
		rc = ufdClose(fd);
		if (fdGetFdno(fd) >= 0)
		    break;
		fdSetFp(fd, NULL);
		fd->nfps++;
		rc = fclose(fp);
		fdPop(fd);
		if (noLibio)
		    fdSetFp(fd, NULL);
	    } else {
		rc = fclose(fp);
		if (fpno == -1) {
		    fd = fdFree(fd, "fopencookie (Fclose)");
		    fdPop(fd);
		}
	    }
	} else {
	    fdio_close_function_t * _close = FDIOVEC(fd, close);
	    rc = _close(fd);
	}
	if (fd->nfps == 0)
	    break;
	if (ec == 0 && rc)
	    ec = rc;
	fdPop(fd);
    }
    fd = fdFree(fd, "Fclose");
    return ec;
}

/*
 * Convert stdio fmode to open(2) mode, filtering out zlib/bzlib flags.
 *	returns stdio[0] = '\0' on error.
 *
 * gzopen:	[0-9] is compession level
 * gzopen:	'f' is filtered (Z_FILTERED)
 * gzopen:	'h' is Huffman encoding (Z_HUFFMAN_ONLY)
 * bzopen:	[1-9] is block size (modulo 100K)
 * bzopen:	's' is smallmode
 * HACK:	'.' terminates, rest is type of I/O
 */
static inline void cvtfmode (const char *m,
				/*@out@*/ char *stdio, size_t nstdio,
				/*@out@*/ char *other, size_t nother,
				/*@out@*/ const char **end, /*@out@*/ int * f)
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
	/*@notreached@*/ break;
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
	case 'b':
	    if (--nstdio > 0) *stdio++ = c;
	    continue;
	case 'x':
	    flags |= O_EXCL;
	    if (--nstdio > 0) *stdio++ = c;
	    continue;
	default:
	    if (--nother > 0) *other++ = c;
	    continue;
	}
	break;
    }

    *stdio = *other = '\0';
    if (end)
	*end = (*m ? m : NULL);
    if (f)
	*f = flags;
}

#if _USE_LIBIO
#if defined(__GLIBC__) && __GLIBC__ == 2 && __GLIBC_MINOR__ == 0
/* XXX retrofit glibc-2.1.x typedef on glibc-2.0.x systems */
typedef _IO_cookie_io_functions_t cookie_io_functions_t;
#endif
#endif

FD_t Fdopen(FD_t ofd, const char *fmode)
{
    char stdio[20], other[20], zstdio[20];
    const char *end = NULL;
    FDIO_t iof = NULL;
    FD_t fd = ofd;

if (_rpmio_debug)
fprintf(stderr, "*** Fdopen(%p,%s) %s\n", fd, fmode, fdbg(fd));
    FDSANE(fd);

    if (fmode == NULL)
	return NULL;

    cvtfmode(fmode, stdio, sizeof(stdio), other, sizeof(other), &end, NULL);
    if (stdio[0] == '\0')
	return NULL;
    zstdio[0] = '\0';
    strncat(zstdio, stdio, sizeof(zstdio) - strlen(zstdio));
    strncat(zstdio, other, sizeof(zstdio) - strlen(zstdio));

    if (end == NULL && other[0] == '\0')
	return fd;

    if (end && *end) {
	if (!strcmp(end, "fdio")) {
	    iof = fdio;
	} else if (!strcmp(end, "gzdio")) {
	    iof = gzdio;
	    fd = gzdFdopen(fd, zstdio);
#if HAVE_BZLIB_H
	} else if (!strcmp(end, "bzdio")) {
	    iof = bzdio;
	    fd = bzdFdopen(fd, zstdio);
#endif
	} else if (!strcmp(end, "ufdio")) {
	    iof = ufdio;
	} else if (!strcmp(end, "fadio")) {
	    iof = fadio;
	} else if (!strcmp(end, "fpio")) {
	    iof = fpio;
	    if (noLibio) {
		int fdno = Fileno(fd);
		FILE * fp = fdopen(fdno, stdio);
if (_rpmio_debug)
fprintf(stderr, "*** Fdopen fpio fp %p\n", fp);
		if (fp == NULL)
		    return NULL;
		/* XXX gzdio/bzdio use fp for private data */
		if (fdGetFp(fd) == NULL)
		    fdSetFp(fd, fp);
		fdPush(fd, fpio, fp, fdno);	/* Push fpio onto stack */
	    }
	}
    } else if (other[0]) {
	for (end = other; *end && strchr("0123456789fh", *end); end++)
	    ;
	if (*end == '\0') {
	    iof = gzdio;
	    fd = gzdFdopen(fd, zstdio);
	}
    }
    if (iof == NULL)
	return fd;

    if (!noLibio) {
	FILE * fp = NULL;

#if _USE_LIBIO
	{   cookie_io_functions_t ciof;
	    ciof.read = iof->read;
	    ciof.write = iof->write;
	    ciof.seek = iof->seek;
	    ciof.close = iof->close;
	    fp = fopencookie(fd, stdio, ciof);
DBGIO(fd, (stderr, "==> fopencookie(%p,\"%s\",*%p) returns fp %p\n", fd, stdio, iof, fp));
	}
#endif

	if (fp) {
	    /* XXX gzdio/bzdio use fp for private data */
	    if (fdGetFp(fd) == NULL)
		fdSetFp(fd, fp);
	    fdPush(fd, fpio, fp, fileno(fp));	/* Push fpio onto stack */
	    fd = fdLink(fd, "fopencookie");
	}
    }

DBGIO(fd, (stderr, "==> Fdopen(%p,\"%s\") returns fd %p %s\n", ofd, fmode, fd, fdbg(fd)));
    return fd;
}

FD_t Fopen(const char *path, const char *fmode)
{
    char stdio[20], other[20];
    const char *end = NULL;
    mode_t perms = 0666;
    int flags;
    FD_t fd;

    if (path == NULL || fmode == NULL)
	return NULL;

    cvtfmode(fmode, stdio, sizeof(stdio), other, sizeof(other), &end, &flags);
    if (stdio[0] == '\0')
	return NULL;

    if (end == NULL || !strcmp(end, "fdio")) {
if (_rpmio_debug)
fprintf(stderr, "*** Fopen fdio path %s fmode %s\n", path, fmode);
	fd = fdOpen(path, flags, perms);
	if (fdFileno(fd) < 0) {
	    fdClose(fd);
	    return NULL;
	}
    } else if (!strcmp(end, "fadio")) {
if (_rpmio_debug)
fprintf(stderr, "*** Fopen fadio path %s fmode %s\n", path, fmode);
	fd = fadio->_open(path, flags, perms);
	if (fdFileno(fd) < 0) {
	    fdClose(fd);
	    return NULL;
	}
    } else {
	FILE *fp;
	int fdno;
	int isHTTP = 0;

	/* XXX gzdio and bzdio here too */

	switch (urlIsURL(path)) {
	case URL_IS_HTTP:
	    isHTTP = 1;
	    /*@fallthrough@*/
	case URL_IS_PATH:
	case URL_IS_DASH:
	case URL_IS_FTP:
	case URL_IS_UNKNOWN:
if (_rpmio_debug)
fprintf(stderr, "*** Fopen ufdio path %s fmode %s\n", path, fmode);
	    fd = ufdOpen(path, flags, perms);
	    if (fd == NULL || fdFileno(fd) < 0)
		return fd;
	    break;
	default:
if (_rpmio_debug)
fprintf(stderr, "*** Fopen WTFO path %s fmode %s\n", path, fmode);
	    return NULL;
	    /*@notreached@*/ break;
	}

	/* XXX persistent HTTP/1.1 returns the previously opened fp */
	if (isHTTP && ((fp = fdGetFp(fd)) != NULL) && ((fdno = fdGetFdno(fd)) >= 0)) {
	    fdPush(fd, fpio, fp, fileno(fp));	/* Push fpio onto stack */
	    return fd;
	}
    }

    fd = Fdopen(fd, fmode);
    return fd;
}

int Fflush(FD_t fd)
{
    if (fd == NULL) return -1;
    if (fdGetIo(fd) == fpio)
	return fflush(fdGetFILE(fd));
    if (fdGetIo(fd) == gzdio)
	return gzdFlush(fdGetFp(fd));
#if HAVE_BZLIB_H
    if (fdGetIo(fd) == bzdio)
	return bzdFlush(fdGetFp(fd));
#endif
    return 0;
}

int Ferror(FD_t fd) {
    int i, rc = 0;

    if (fd == NULL) return -1;
    for (i = fd->nfps; rc == 0 && i >= 0; i--) {
	FDSTACK_t * fps = &fd->fps[i];
	int ec;
	
	if (fps->io == fpio) {
	    ec = ferror(fdGetFILE(fd));
	} else if (fps->io == gzdio) {
	    ec = (fd->syserrno  || fd->errcookie != NULL) ? -1 : 0;
#if HAVE_BZLIB_H
	} else if (fps->io == bzdio) {
	    ec = (fd->syserrno  || fd->errcookie != NULL) ? -1 : 0;
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

int Fileno(FD_t fd) {
    int i, rc = -1;

    for (i = fd->nfps ; rc == -1 && i >= 0; i--) {
	rc = fd->fps[i].fdno;
    }
DBGIO(fd, (stderr, "==> Fileno(%p) rc %d %s\n", fd, rc, fdbg(fd)));
    return rc;
}

/* XXX this is naive */
int Fcntl(FD_t fd, int op, void *lip) {
    return fcntl(Fileno(fd), op, lip);
}

/* =============================================================== */
/* Helper routines that may be generally useful.
 */

/* XXX falloc.c: analogues to pread(3)/pwrite(3). */
#ifdef USE_COOKIE_SEEK_POINTER
ssize_t Pread(FD_t fd, void * buf, size_t count, _IO_off64_t offset) {
#else
ssize_t Pread(FD_t fd, void * buf, size_t count, off_t offset) {
#endif
    if (Fseek(fd, offset, SEEK_SET) < 0)
	return -1;
    return Fread(buf, sizeof(char), count, fd);
}

#ifdef USE_COOKIE_SEEK_POINTER
ssize_t Pwrite(FD_t fd, const void * buf, size_t count, _IO_off64_t offset) {
#else
ssize_t Pwrite(FD_t fd, const void * buf, size_t count, off_t offset) {
#endif
    if (Fseek(fd, offset, SEEK_SET) < 0)
	return -1;
    return Fwrite(buf, sizeof(char), count, fd);
}

/* XXX rebuilddb.c: analogues to mkdir(2)/rmdir(2). */
int Mkdir (const char *path, mode_t mode) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

    switch (ut) {
    case URL_IS_FTP:
	return ftpMkdir(path, mode);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return mkdir(path, mode);
}

int Chdir (const char *path) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

    switch (ut) {
    case URL_IS_FTP:
	return ftpChdir(path);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return chdir(path);
}

int Rmdir (const char *path) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

    switch (ut) {
    case URL_IS_FTP:
	return ftpRmdir(path);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return rmdir(path);
}

/* XXX rpmdb.c: analogue to rename(2). */

int Rename (const char *oldpath, const char * newpath) {
    const char *oe = NULL;
    const char *ne = NULL;
    int oldut, newut;

    /* XXX lib/install.c used to rely on this behavior. */
    if (!strcmp(oldpath, newpath)) return 0;

    oldut = urlPath(oldpath, &oe);
    switch (oldut) {
    case URL_IS_FTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }

    newut = urlPath(newpath, &ne);
    switch (newut) {
    case URL_IS_FTP:
if (_rpmio_debug)
fprintf(stderr, "*** rename old %*s new %*s\n", (int)(oe - oldpath), oldpath, (int)(ne - newpath), newpath);
	if (!(oldut == newut && oe && ne && (oe - oldpath) == (ne - newpath) &&
	    !strncasecmp(oldpath, newpath, (oe - oldpath))))
	    return -2;
	return ftpRename(oldpath, newpath);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	oldpath = oe;
	newpath = ne;
	break;
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return rename(oldpath, newpath);
}

int Link (const char *oldpath, const char * newpath) {
    const char *oe = NULL;
    const char *ne = NULL;
    int oldut, newut;

    oldut = urlPath(oldpath, &oe);
    switch (oldut) {
    case URL_IS_FTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }

    newut = urlPath(newpath, &ne);
    switch (newut) {
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_FTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
if (_rpmio_debug)
fprintf(stderr, "*** link old %*s new %*s\n", (int)(oe - oldpath), oldpath, (int)(ne - newpath), newpath);
	if (!(oldut == newut && oe && ne && (oe - oldpath) == (ne - newpath) &&
	    !strncasecmp(oldpath, newpath, (oe - oldpath))))
	    return -2;
	oldpath = oe;
	newpath = ne;
	break;
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return link(oldpath, newpath);
}

/* XXX build/build.c: analogue to unlink(2). */

int Unlink(const char * path) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

    switch (ut) {
    case URL_IS_FTP:
	return ftpUnlink(path);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return unlink(path);
}

/* XXX swiped from mc-4.5.39-pre9 vfs/ftpfs.c */

#define	g_strdup	xstrdup
#define	g_free		xfree

/*
 * FIXME: this is broken. It depends on mc not crossing border on month!
 */
static int current_mday;
static int current_mon;
static int current_year;

/* Following stuff (parse_ls_lga) is used by ftpfs and extfs */
#define MAXCOLS		30

static char *columns [MAXCOLS];	/* Points to the string in column n */
static int   column_ptr [MAXCOLS]; /* Index from 0 to the starting positions of the columns */

static int
vfs_split_text (char *p)
{
    char *original = p;
    int  numcols;


    for (numcols = 0; *p && numcols < MAXCOLS; numcols++){
	while (*p == ' ' || *p == '\r' || *p == '\n'){
	    *p = 0;
	    p++;
	}
	columns [numcols] = p;
	column_ptr [numcols] = p - original;
	while (*p && *p != ' ' && *p != '\r' && *p != '\n')
	    p++;
    }
    return numcols;
}

static int
is_num (int idx)
{
    if (!columns [idx] || columns [idx][0] < '0' || columns [idx][0] > '9')
	return 0;
    return 1;
}

static int
is_dos_date(char *str)
{
    if (strlen(str) == 8 && str[2] == str[5] && strchr("\\-/", (int)str[2]) != NULL)
	return (1);

    return (0);
}

static int
is_week (char *str, struct tm *tim)
{
    static char *week = "SunMonTueWedThuFriSat";
    char *pos;

    if((pos=strstr(week, str)) != NULL){
        if(tim != NULL)
	    tim->tm_wday = (pos - week)/3;
	return (1);
    }
    return (0);    
}

static int
is_month (char *str, struct tm *tim)
{
    static char *month = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char *pos;
    
    if((pos=strstr(month, str)) != NULL){
        if(tim != NULL)
	    tim->tm_mon = (pos - month)/3;
	return (1);
    }
    return (0);
}

static int
is_time (char *str, struct tm *tim)
{
    char *p, *p2;

    if ((p=strchr(str, ':')) && (p2=strrchr(str, ':'))) {
	if (p != p2) {
    	    if (sscanf (str, "%2d:%2d:%2d", &tim->tm_hour, &tim->tm_min, &tim->tm_sec) != 3)
		return (0);
	}
	else {
	    if (sscanf (str, "%2d:%2d", &tim->tm_hour, &tim->tm_min) != 2)
		return (0);
	}
    }
    else 
        return (0);
    
    return (1);
}

static int is_year(char *str, struct tm *tim)
{
    long year;

    if (strchr(str,':'))
        return (0);

    if (strlen(str)!=4)
        return (0);

    if (sscanf(str, "%ld", &year) != 1)
        return (0);

    if (year < 1900 || year > 3000)
        return (0);

    tim->tm_year = (int) (year - 1900);

    return (1);
}

/*
 * FIXME: this is broken. Consider following entry:
 * -rwx------   1 root     root            1 Aug 31 10:04 2904 1234
 * where "2904 1234" is filename. Well, this code decodes it as year :-(.
 */

static int
vfs_parse_filetype (char c)
{
    switch (c){
        case 'd': return S_IFDIR; 
        case 'b': return S_IFBLK;
        case 'c': return S_IFCHR;
        case 'l': return S_IFLNK;
        case 's':
#ifdef IS_IFSOCK /* And if not, we fall through to IFIFO, which is pretty close */
	          return S_IFSOCK;
#endif
        case 'p': return S_IFIFO;
        case 'm': case 'n':		/* Don't know what these are :-) */
        case '-': case '?': return S_IFREG;
        default: return -1;
    }
}

static int vfs_parse_filemode (char *p)
{	/* converts rw-rw-rw- into 0666 */
    int res = 0;
    switch (*(p++)){
	case 'r': res |= 0400; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'w': res |= 0200; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'x': res |= 0100; break;
	case 's': res |= 0100 | S_ISUID; break;
	case 'S': res |= S_ISUID; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'r': res |= 0040; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'w': res |= 0020; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'x': res |= 0010; break;
	case 's': res |= 0010 | S_ISGID; break;
        case 'l': /* Solaris produces these */
	case 'S': res |= S_ISGID; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'r': res |= 0004; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'w': res |= 0002; break;
	case '-': break;
	default: return -1;
    }
    switch (*(p++)){
	case 'x': res |= 0001; break;
	case 't': res |= 0001 | S_ISVTX; break;
	case 'T': res |= S_ISVTX; break;
	case '-': break;
	default: return -1;
    }
    return res;
}

static int vfs_parse_filedate(int idx, time_t *t)
{	/* This thing parses from idx in columns[] array */

    char *p;
    struct tm tim;
    int d[3];
    int	got_year = 0;

    /* Let's setup default time values */
    tim.tm_year = current_year;
    tim.tm_mon  = current_mon;
    tim.tm_mday = current_mday;
    tim.tm_hour = 0;
    tim.tm_min  = 0;
    tim.tm_sec  = 0;
    tim.tm_isdst = -1; /* Let mktime() try to guess correct dst offset */
    
    p = columns [idx++];
    
    /* We eat weekday name in case of extfs */
    if(is_week(p, &tim))
	p = columns [idx++];

    /* Month name */
    if(is_month(p, &tim)){
	/* And we expect, it followed by day number */
	if (is_num (idx))
    	    tim.tm_mday = (int)atol (columns [idx++]);
	else
	    return 0; /* No day */

    } else {
        /* We usually expect:
           Mon DD hh:mm
           Mon DD  YYYY
	   But in case of extfs we allow these date formats:
           Mon DD YYYY hh:mm
	   Mon DD hh:mm YYYY
	   Wek Mon DD hh:mm:ss YYYY
           MM-DD-YY hh:mm
           where Mon is Jan-Dec, DD, MM, YY two digit day, month, year,
           YYYY four digit year, hh, mm, ss two digit hour, minute or second. */

	/* Here just this special case with MM-DD-YY */
        if (is_dos_date(p)){
            p[2] = p[5] = '-';
	    
	    if(sscanf(p, "%2d-%2d-%2d", &d[0], &d[1], &d[2]) == 3){
	    /*  We expect to get:
		1. MM-DD-YY
		2. DD-MM-YY
		3. YY-MM-DD
		4. YY-DD-MM  */
		
		/* Hmm... maybe, next time :)*/
		
		/* At last, MM-DD-YY */
		d[0]--; /* Months are zerobased */
		/* Y2K madness */
		if(d[2] < 70)
		    d[2] += 100;

        	tim.tm_mon  = d[0];
        	tim.tm_mday = d[1];
        	tim.tm_year = d[2];
		got_year = 1;
	    } else
		return 0; /* sscanf failed */
        } else
            return 0; /* unsupported format */
    }

    /* Here we expect to find time and/or year */
    
    if (is_num (idx)) {
        if(is_time(columns[idx], &tim) || (got_year = is_year(columns[idx], &tim))) {
	idx++;

	/* This is a special case for ctime() or Mon DD YYYY hh:mm */
	if(is_num (idx) && 
	    ((got_year = is_year(columns[idx], &tim)) || is_time(columns[idx], &tim)))
		idx++; /* time & year or reverse */
	} /* only time or date */
    }
    else 
        return 0; /* Nor time or date */

    /*
     * If the date is less than 6 months in the past, it is shown without year
     * other dates in the past or future are shown with year but without time
     * This does not check for years before 1900 ... I don't know, how
     * to represent them at all
     */
    if (!got_year &&
	current_mon < 6 && current_mon < tim.tm_mon && 
	tim.tm_mon - current_mon >= 6)

	tim.tm_year--;

    if ((*t = mktime(&tim)) < 0)
        *t = 0;
    return idx;
}

static int
vfs_parse_ls_lga (char *p, struct stat *s, char **filename, char **linkname)
{
    int idx, idx2, num_cols;
    int i;
    char *p_copy;
    
    if (strncmp (p, "total", 5) == 0)
        return 0;

    p_copy = g_strdup(p);
/* XXX FIXME: parse out inode number from "NLST -lai ." */
/* XXX FIXME: parse out sizein blocks from "NLST -lais ." */

    if ((i = vfs_parse_filetype(*(p++))) == -1)
        goto error;

    s->st_mode = i;
    if (*p == ' ')	/* Notwell 4 */
        p++;
    if (*p == '['){
	if (strlen (p) <= 8 || p [8] != ']')
	    goto error;
	/* Should parse here the Notwell permissions :) */
	if (S_ISDIR (s->st_mode))
	    s->st_mode |= (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IXUSR | S_IXGRP | S_IXOTH);
	else
	    s->st_mode |= (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
	p += 9;
    } else {
	if ((i = vfs_parse_filemode(p)) == -1)
	    goto error;
        s->st_mode |= i;
	p += 9;

        /* This is for an extra ACL attribute (HP-UX) */
        if (*p == '+')
            p++;
    }

    g_free(p_copy);
    p_copy = g_strdup(p);
    num_cols = vfs_split_text (p);

    s->st_nlink = atol (columns [0]);
    if (s->st_nlink < 0)
        goto error;

    if (!is_num (1))
#ifdef	HACK
	s->st_uid = finduid (columns [1]);
#else
	unameToUid (columns [1], &s->st_uid);
#endif
    else
        s->st_uid = (uid_t) atol (columns [1]);

    /* Mhm, the ls -lg did not produce a group field */
    for (idx = 3; idx <= 5; idx++) 
        if (is_month(columns [idx], NULL) || is_week(columns [idx], NULL) || is_dos_date(columns[idx]))
            break;

    if (idx == 6 || (idx == 5 && !S_ISCHR (s->st_mode) && !S_ISBLK (s->st_mode)))
	goto error;

    /* We don't have gid */	
    if (idx == 3 || (idx == 4 && (S_ISCHR(s->st_mode) || S_ISBLK (s->st_mode))))
        idx2 = 2;
    else { 
	/* We have gid field */
	if (is_num (2))
	    s->st_gid = (gid_t) atol (columns [2]);
	else
#ifdef	HACK
	    s->st_gid = findgid (columns [2]);
#else
	    gnameToGid (columns [1], &s->st_gid);
#endif
	idx2 = 3;
    }

    /* This is device */
    if (S_ISCHR (s->st_mode) || S_ISBLK (s->st_mode)){
	int maj, min;
	
	if (!is_num (idx2) || sscanf(columns [idx2], " %d,", &maj) != 1)
	    goto error;
	
	if (!is_num (++idx2) || sscanf(columns [idx2], " %d", &min) != 1)
	    goto error;
	
#ifdef HAVE_ST_RDEV
	s->st_rdev = ((maj & 0xff) << 8) | (min & 0xffff00ff);
#endif
	s->st_size = 0;
	
    } else {
	/* Common file size */
	if (!is_num (idx2))
	    goto error;
	
	s->st_size = (size_t) atol (columns [idx2]);
#ifdef HAVE_ST_RDEV
	s->st_rdev = 0;
#endif
    }

    idx = vfs_parse_filedate(idx, &s->st_mtime);
    if (!idx)
        goto error;
    /* Use resulting time value */
    s->st_atime = s->st_ctime = s->st_mtime;
    s->st_dev = 0;
    s->st_ino = 0;
#ifdef HAVE_ST_BLKSIZE
    s->st_blksize = 512;
#endif
#ifdef HAVE_ST_BLOCKS
    s->st_blocks = (s->st_size + 511) / 512;
#endif

    for (i = idx + 1, idx2 = 0; i < num_cols; i++ ) 
	if (strcmp (columns [i], "->") == 0){
	    idx2 = i;
	    break;
	}
    
    if (((S_ISLNK (s->st_mode) || 
        (num_cols == idx + 3 && s->st_nlink > 1))) /* Maybe a hardlink? (in extfs) */
        && idx2){
	int p;
	char *s;
	    
	if (filename){
#ifdef HACK
	    s = g_strndup (p_copy + column_ptr [idx], column_ptr [idx2] - column_ptr [idx] - 1);
#else
	    int nb = column_ptr [idx2] - column_ptr [idx] - 1;
	    s = xmalloc(nb+1);
	    strncpy(s, p_copy + column_ptr [idx], nb);
#endif
	    *filename = s;
	}
	if (linkname){
	    s = g_strdup (p_copy + column_ptr [idx2+1]);
	    p = strlen (s);
	    if (s [p-1] == '\r' || s [p-1] == '\n')
		s [p-1] = 0;
	    if (s [p-2] == '\r' || s [p-2] == '\n')
		s [p-2] = 0;
		
	    *linkname = s;
	}
    } else {
	/* Extract the filename from the string copy, not from the columns
	 * this way we have a chance of entering hidden directories like ". ."
	 */
	if (filename){
	    /* 
	    *filename = g_strdup (columns [idx++]);
	    */
	    int p;
	    char *s;
	    
	    s = g_strdup (p_copy + column_ptr [idx++]);
	    p = strlen (s);
	    /* g_strchomp(); */
	    if (s [p-1] == '\r' || s [p-1] == '\n')
	        s [p-1] = 0;
	    if (s [p-2] == '\r' || s [p-2] == '\n')
		s [p-2] = 0;
	    
	    *filename = s;
	}
	if (linkname)
	    *linkname = NULL;
    }
    g_free (p_copy);
    return 1;

error:
#ifdef	HACK
    {
      static int errorcount = 0;

      if (++errorcount < 5) {
	message_1s (1, "Could not parse:", p_copy);
      } else if (errorcount == 5)
	message_1s (1, "More parsing errors will be ignored.", "(sorry)" );
    }
#endif

    if (p_copy != p)		/* Carefull! */
	g_free (p_copy);
    return 0;
}

typedef enum {
	DO_FTP_STAT	= 1,
	DO_FTP_LSTAT	= 2,
	DO_FTP_READLINK	= 3,
	DO_FTP_ACCESS	= 4,
	DO_FTP_GLOB	= 5
} ftpSysCall_t;
static size_t ftpBufAlloced = 0;
static char * ftpBuf = NULL;
	
#define alloca_strdup(_s)       strcpy(alloca(strlen(_s)+1), (_s))

static int ftpNLST(const char * url, ftpSysCall_t ftpSysCall,
	struct stat * st, char * rlbuf, size_t rlbufsiz)
{
    FD_t fd;
    const char * path;
    int bufLength, moretodo;
    const char *n, *ne, *o, *oe;
    char * s;
    char * se;
    const char * urldn;
    char * bn = NULL;
    int nbn = 0;
    urlinfo u;
    int rc;

    n = ne = o = oe = NULL;
    (void) urlPath(url, &path);
    if (*path == '\0')
	return -2;

    switch (ftpSysCall) {
    case DO_FTP_GLOB:
	fd = ftpOpen(url, 0, 0, &u);
	if (fd == NULL || u == NULL)
	    return -1;

	u->openError = ftpReq(fd, "NLST", path);
	break;
    default:
	urldn = alloca_strdup(url);
	if ((bn = strrchr(urldn, '/')) == NULL)
	    return -2;
	else if (bn == path)
	    bn = ".";
	else
	    *bn++ = '\0';
	nbn = strlen(bn);

	rc = ftpChdir(urldn);		/* XXX don't care about CWD */
	if (rc < 0)
	    return rc;

	fd = ftpOpen(url, 0, 0, &u);
	if (fd == NULL || u == NULL)
	    return -1;

	/* XXX possibly should do "NLST -lais" to get st_ino/st_blocks also */
	u->openError = ftpReq(fd, "NLST", "-la");
	break;
    }

    if (u->openError < 0) {
	fd = fdLink(fd, "error data (ftpStat)");
	rc = -2;
	goto exit;
    }

    if (ftpBufAlloced == 0 || ftpBuf == NULL) {
        ftpBufAlloced = url_iobuf_size;
        ftpBuf = xcalloc(ftpBufAlloced, sizeof(ftpBuf[0]));
    }
    *ftpBuf = '\0';

    bufLength = 0;
    moretodo = 1;

    do {

	/* XXX FIXME: realloc ftpBuf is < ~128 chars remain */
	if ((ftpBufAlloced - bufLength) < (1024+80)) {
	    ftpBufAlloced <<= 2;
	    ftpBuf = xrealloc(ftpBuf, ftpBufAlloced);
	}
	s = se = ftpBuf + bufLength;
	*se = '\0';

	rc = fdFgets(fd, se, (ftpBufAlloced - bufLength));
	if (rc <= 0) {
	    moretodo = 0;
	    break;
	}
	if (ftpSysCall == DO_FTP_GLOB) {	/* XXX HACK */
	    bufLength += strlen(se);
	    continue;
	}

	for (s = se; *s != '\0'; s = se) {
	    int bingo;

	    while (*se && *se != '\n') se++;
	    if (se > s && se[-1] == '\r') se[-1] = '\0';
	    if (*se == '\0') break;
	    *se++ = '\0';

	    if (!strncmp(s, "total ", sizeof("total ")-1)) continue;

	    o = NULL;
	    for (bingo = 0, n = se; n >= s; n--) {
		switch (*n) {
		case '\0':
		    oe = ne = n;
		    break;
		case ' ':
		    if (o || !(n[-3] == ' ' && n[-2] == '-' && n[-1] == '>')) {
			while (*(++n) == ' ');
			bingo++;
			break;
		    }
		    for (o = n + 1; *o == ' '; o++);
		    n -= 3;
		    ne = n;
		    break;
		default:
		    break;
		}
		if (bingo) break;
	    }

	    if (nbn != (ne - n))	continue;	/* Same name length? */
	    if (strncmp(n, bn, nbn))	continue;	/* Same name? */

	    moretodo = 0;
	    break;
	}

        if (moretodo && se > s) {
            bufLength = se - s - 1;
            if (s != ftpBuf)
                memmove(ftpBuf, s, bufLength);
        } else {
	    bufLength = 0;
	}
    } while (moretodo);

    switch (ftpSysCall) {
    case DO_FTP_STAT:
	if (o && oe) {
	    /* XXX FIXME: symlink, replace urldn/bn from [o,oe) and restart */
	}
	/*@fallthrough@*/
    case DO_FTP_LSTAT:
	if (st == NULL || !(n && ne)) {
	    rc = -1;
	} else {
	    rc = ((vfs_parse_ls_lga(s, st, NULL, NULL) > 0) ? 0 : -1);
	}
	break;
    case DO_FTP_READLINK:
	if (rlbuf == NULL || !(o && oe)) {
	    rc = -1;
	} else {
	    rc = oe - o;
	    if (rc > rlbufsiz)
		rc = rlbufsiz;
	    memcpy(rlbuf, o, rc);
	    if (rc < rlbufsiz)
		rlbuf[rc] = '\0';
	}
	break;
    case DO_FTP_ACCESS:
	rc = 0;		/* XXX WRONG WRONG WRONG */
	break;
    case DO_FTP_GLOB:
	rc = 0;		/* XXX WRONG WRONG WRONG */
	break;
    }

exit:
    ufdClose(fd);
    return rc;
}

static int ftpStat(const char * path, struct stat *st)
{
    return ftpNLST(path, DO_FTP_STAT, st, NULL, 0);
}

static int ftpLstat(const char * path, struct stat *st) {
    int rc;
    rc = ftpNLST(path, DO_FTP_LSTAT, st, NULL, 0);
if (_rpmio_debug)
fprintf(stderr, "*** ftpLstat(%s) rc %d\n", path, rc);
    return rc;
}

static int ftpReadlink(const char * path, char * buf, size_t bufsiz) {
    return ftpNLST(path, DO_FTP_READLINK, NULL, buf, bufsiz);
}

static int ftpGlob(const char * path, int flags,
		int errfunc(const char * epath, int eerno), glob_t * pglob)
{
    int rc;

    if (pglob == NULL)
	return -2;
    rc = ftpNLST(path, DO_FTP_GLOB, NULL, NULL, 0);
if (_rpmio_debug)
fprintf(stderr, "*** ftpGlob(%s,0x%x,%p,%p) ftpNLST rc %d\n", path, flags, errfunc, pglob, rc);
    if (rc)
	return rc;
    rc = poptParseArgvString(ftpBuf, &pglob->gl_pathc, (const char ***)&pglob->gl_pathv);
    pglob->gl_offs = -1;	/* XXX HACK HACK HACK */
    return rc;
}

static void ftpGlobfree(glob_t * pglob) {
if (_rpmio_debug)
fprintf(stderr, "*** ftpGlobfree(%p)\n", pglob);
    if (pglob->gl_offs == -1)	/* XXX HACK HACK HACK */
	xfree(pglob->gl_pathv);
}

int Stat(const char * path, struct stat * st) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

if (_rpmio_debug)
fprintf(stderr, "*** Stat(%s,%p)\n", path, st);
    switch (ut) {
    case URL_IS_FTP:
	return ftpStat(path, st);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return stat(path, st);
}

int Lstat(const char * path, struct stat * st) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

if (_rpmio_debug)
fprintf(stderr, "*** Lstat(%s,%p)\n", path, st);
    switch (ut) {
    case URL_IS_FTP:
	return ftpLstat(path, st);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return lstat(path, st);
}

int Readlink(const char * path, char * buf, size_t bufsiz) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

    switch (ut) {
    case URL_IS_FTP:
	return ftpReadlink(path, buf, bufsiz);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return readlink(path, buf, bufsiz);
}

int Access(const char * path, int amode) {
    const char * lpath;
    int ut = urlPath(path, &lpath);

if (_rpmio_debug)
fprintf(stderr, "*** Access(%s,%d)\n", path, amode);
    switch (ut) {
    case URL_IS_FTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return access(path, amode);
}

int Glob(const char *path, int flags,
	int errfunc(const char * epath, int eerrno), glob_t *pglob)
{
    const char * lpath;
    int ut = urlPath(path, &lpath);

if (_rpmio_debug)
fprintf(stderr, "*** Glob(%s,0x%x,%p,%p)\n", path, flags, errfunc, pglob);
    switch (ut) {
    case URL_IS_FTP:		/* XXX WRONG WRONG WRONG */
	return ftpGlob(path, flags, errfunc, pglob);
	/*@notreached@*/ break;
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return -2;
	/*@notreached@*/ break;
    }
    return glob(path, flags, errfunc, pglob);
}

void Globfree(glob_t *pglob)
{
if (_rpmio_debug)
fprintf(stderr, "*** Globfree(%p)\n", pglob);
    if (pglob->gl_offs == -1) /* XXX HACK HACK HACK */
	ftpGlobfree(pglob);
    else
	globfree(pglob);
}

DIR * Opendir(const char * path)
{
    const char * lpath;
    int ut = urlPath(path, &lpath);

if (_rpmio_debug)
fprintf(stderr, "*** Opendir(%s)\n", path);
    switch (ut) {
    case URL_IS_FTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_HTTP:		/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
	path = lpath;
	/*@fallthrough@*/
    case URL_IS_UNKNOWN:
	break;
    case URL_IS_DASH:
    default:
	return NULL;
	/*@notreached@*/ break;
    }
    return opendir(path);
}

struct dirent * Readdir(DIR * dir)
{
if (_rpmio_debug)
fprintf(stderr, "*** Readdir(%p)\n", dir);
    return readdir(dir);
}

int Closedir(DIR * dir)
{
if (_rpmio_debug)
fprintf(stderr, "*** Closedir(%p)\n", dir);
    return closedir(dir);
}

static struct FDIO_s fpio_s = {
  ufdRead, ufdWrite, fdSeek, ufdClose, XfdLink, XfdFree, XfdNew, fdFileno,
  ufdOpen, NULL, fdGetFp, NULL,	Mkdir, Chdir, Rmdir, Rename, Unlink
};
FDIO_t fpio = /*@-compmempass@*/ &fpio_s /*@=compmempass@*/ ;
