#ifndef	H_RPMIO_INTERNAL
#define	H_RPMIO_INTERNAL

#include <rpmio.h>
#include <rpmurl.h>
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
#define	FDMAGIC		0x04463138
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
/*@access FD_t */

#define	FDSANE(fd)	assert(fd && fd->magic == FDMAGIC)

extern int _rpmio_debug;

#define DBG(_f, _m, _x) \
    if ((_rpmio_debug | ((_f) ? ((FD_t)(_f))->flags : 0)) & (_m)) fprintf _x

#define DBGIO(_f, _x)   DBG((_f), RPMIO_DEBUG_IO, _x)
#define DBGREFS(_f, _x) DBG((_f), RPMIO_DEBUG_REFS, _x)

#define xfree(_p)       free((void *)_p)

#ifdef __cplusplus
extern "C" {
#endif

int fdFgets(FD_t fd, char * buf, size_t len);

/*@null@*/ FD_t ftpOpen(const char *url, /*@unused@*/ int flags,
                /*@unused@*/ mode_t mode, /*@out@*/ urlinfo *uret);
int ftpReq(FD_t data, const char * ftpCmd, const char * ftpArg);
int ftpCmd(const char * cmd, const char * url, const char * arg2);

int ufdClose( /*@only@*/ void * cookie);

static inline /*@null@*/ const FDIO_t fdGetIo(FD_t fd) {
    FDSANE(fd);
    return fd->fps[fd->nfps].io;
}

static inline void fdSetIo(FD_t fd, FDIO_t io) {
    FDSANE(fd);
    fd->fps[fd->nfps].io = io;
}

static inline /*@dependent@*/ /*@null@*/ FILE * fdGetFILE(FD_t fd) {
    FDSANE(fd);
    return ((FILE *)fd->fps[fd->nfps].fp);
}

static inline /*@dependent@*/ /*@null@*/ void * fdGetFp(FD_t fd) {
    FDSANE(fd);
    return fd->fps[fd->nfps].fp;
}

static inline void fdSetFp(FD_t fd, /*@keep@*/ void * fp) {
    FDSANE(fd);
    fd->fps[fd->nfps].fp = fp;
}

static inline int fdGetFdno(FD_t fd) {
    FDSANE(fd);
    return fd->fps[fd->nfps].fdno;
}

static inline void fdSetFdno(FD_t fd, int fdno) {
    FDSANE(fd);
    fd->fps[fd->nfps].fdno = fdno;
}

static inline void fdSetContentLength(FD_t fd, ssize_t contentLength)
{
    FDSANE(fd);
    fd->contentLength = fd->bytesRemain = contentLength;
}

static inline void fdPush(FD_t fd, FDIO_t io, void * fp, int fdno) {
    FDSANE(fd);
    if (fd->nfps >= (sizeof(fd->fps)/sizeof(fd->fps[0]) - 1))
	return;
    fd->nfps++;
    fdSetIo(fd, io);
    fdSetFp(fd, fp);
    fdSetFdno(fd, fdno);
}

static inline void fdPop(FD_t fd) {
    FDSANE(fd);
    if (fd->nfps < 0) return;
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

static inline void fdstat_print(FD_t fd, const char * msg, FILE * fp) {
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

static inline void fdSetSyserrno(FD_t fd, int syserrno, const void * errcookie) {
    FDSANE(fd);
    fd->syserrno = syserrno;
    fd->errcookie = errcookie;
}

static inline int fdGetRdTimeoutSecs(FD_t fd) {
    FDSANE(fd);
    return fd->rd_timeoutsecs;
}

static inline long int fdGetCpioPos(FD_t fd) {
    FDSANE(fd);
    return fd->fd_cpioPos;
}

static inline void fdSetCpioPos(FD_t fd, long int cpioPos) {
    FDSANE(fd);
    fd->fd_cpioPos = cpioPos;
}

static inline FD_t c2f(void * cookie) {
    FD_t fd = (FD_t) cookie;
    FDSANE(fd);
    return fd;
}

#undef	fdFileno
static inline int fdFileno(void * cookie) {
    FD_t fd;
    if (cookie == NULL) return -2;
    fd = c2f(cookie);
    return fd->fps[0].fdno;
}

static inline long int fadGetFileSize(FD_t fd) {
    FDSANE(fd);
    return fd->fileSize;
}

static inline void fadSetFileSize(FD_t fd, long int fileSize) {
    FDSANE(fd);
    fd->fileSize = fileSize;
}

static inline unsigned int fadGetFirstFree(FD_t fd) {
    FDSANE(fd);
    return fd->firstFree;
}

static inline void fadSetFirstFree(FD_t fd, unsigned int firstFree) {
    FDSANE(fd);
    fd->firstFree = firstFree;
}

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIO_INTERNAL */
