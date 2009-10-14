#ifndef	H_RPMIO_INTERNAL
#define	H_RPMIO_INTERNAL

/** \ingroup rpmio
 * \file rpmio/rpmio_internal.h
 */

#include <assert.h>

#include <rpm/rpmio.h>
#include <rpm/rpmurl.h>

#include <rpm/rpmpgp.h>
#include <rpm/rpmsw.h>

/** \ingroup rpmio
 */
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
#define	RPMIO_DEBUG_REFS	0x20000000
    int		magic;
#define	FDMAGIC			0x04463138
    int		nfps;
    FDSTACK_t	fps[8];
    int		urlType;	/* ufdio: */

    int		rd_timeoutsecs;	/* ufdRead: per FD_t timer */
    ssize_t	bytesRemain;	/* ufdio: */

    int		syserrno;	/* last system errno encountered */
    const void *errcookie;	/* gzdio/bzdio/ufdio/xzdio: */

    FDSTAT_t	stats;		/* I/O statistics */

    rpmDigestBundle digests;

    rpm_loff_t	fd_cpioPos;	/* cpio: */
};

#define	FDSANE(fd)	assert(fd && fd->magic == FDMAGIC)

extern int _rpmio_debug;

#define DBG(_f, _m, _x) \
    \
    if ((_rpmio_debug | ((_f) ? ((FD_t)(_f))->flags : 0)) & (_m)) fprintf _x \

#define DBGIO(_f, _x)   DBG((_f), RPMIO_DEBUG_IO, _x)
#define DBGREFS(_f, _x) DBG((_f), RPMIO_DEBUG_REFS, _x)

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmio
 */
static inline
FDIO_t fdGetIo(FD_t fd)
{
    FDSANE(fd);
    return fd->fps[fd->nfps].io;
}

/** \ingroup rpmio
 */
/* FIX: io may be NULL */
static inline
void fdSetIo(FD_t fd, FDIO_t io)
{
    FDSANE(fd);
    fd->fps[fd->nfps].io = io;
}

/** \ingroup rpmio
 */
static inline
void * fdGetFp(FD_t fd)
{
    FDSANE(fd);
    return fd->fps[fd->nfps].fp;
}

/** \ingroup rpmio
 */
/* FIX: fp may be NULL */
static inline
void fdSetFp(FD_t fd, void * fp)
{
    FDSANE(fd);
    fd->fps[fd->nfps].fp = fp;
}

/** \ingroup rpmio
 */
static inline
int fdGetFdno(FD_t fd)
{
    FDSANE(fd);
    return fd->fps[fd->nfps].fdno;
}

/** \ingroup rpmio
 */
static inline
void fdSetFdno(FD_t fd, int fdno)
{
    FDSANE(fd);
    fd->fps[fd->nfps].fdno = fdno;
}

/** \ingroup rpmio
 */
static inline
void fdPush(FD_t fd, FDIO_t io, void * fp, int fdno)
{
    FDSANE(fd);
    if (fd->nfps >= (sizeof(fd->fps)/sizeof(fd->fps[0]) - 1))
	return;
    fd->nfps++;
    fdSetIo(fd, io);
    fdSetFp(fd, fp);
    fdSetFdno(fd, fdno);
}

/** \ingroup rpmio
 */
static inline
void fdPop(FD_t fd)
{
    FDSANE(fd);
    if (fd->nfps < 0) return;
    fdSetIo(fd, NULL);
    fdSetFp(fd, NULL);
    fdSetFdno(fd, -1);
    fd->nfps--;
}


/** \ingroup rpmio
 */
static inline
void fdSetSyserrno(FD_t fd, int syserrno, const void * errcookie)
{
    FDSANE(fd);
    fd->syserrno = syserrno;
    fd->errcookie = errcookie;
}

/** \ingroup rpmio
 */
static inline
int fdGetRdTimeoutSecs(FD_t fd)
{
    FDSANE(fd);
    return fd->rd_timeoutsecs;
}

/** \ingroup rpmio
 */
static inline
rpm_loff_t fdGetCpioPos(FD_t fd)
{
    FDSANE(fd);
    return fd->fd_cpioPos;
}

/** \ingroup rpmio
 */
static inline
void fdSetCpioPos(FD_t fd, rpm_loff_t cpioPos)
{
    FDSANE(fd);
    fd->fd_cpioPos = cpioPos;
}

static inline
void fdSetBundle(FD_t fd, rpmDigestBundle bundle)
{
    FDSANE(fd);
    fd->digests = bundle;
}

/** \ingroup rpmio
 */
static inline
FD_t c2f(void * cookie)
{
    FD_t fd = (FD_t) cookie;
    FDSANE(fd);
    return fd;
}

/** \ingroup rpmio
 * Attach digest to fd.
 */
void fdInitDigest(FD_t fd, pgpHashAlgo hashalgo, int flags);

/** \ingroup rpmio
 * Update digest(s) attached to fd.
 */
void fdUpdateDigests(FD_t fd, const unsigned char * buf, size_t buflen);

/** \ingroup rpmio
 */
void fdFiniDigest(FD_t fd, pgpHashAlgo hashalgo,
		void ** datap,
		size_t * lenp,
		int asAscii);

/**
 * Read an entire file into a buffer.
 * @param fn		file name to read
 * @retval *bp		(malloc'd) buffer address
 * @retval *blenp	(malloc'd) buffer length
 * @return		0 on success
 */
int rpmioSlurp(const char * fn,
                uint8_t ** bp, ssize_t * blenp);

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIO_INTERNAL */
