#ifndef	H_RPMIO_INTERNAL
#define	H_RPMIO_INTERNAL

/** \ingroup rpmio
 * \file rpmio/rpmio_internal.h
 */


#include <rpmio.h>
#undef	fdFileno

#include <rpmurl.h>

/** \ingroup rpmio
 */
typedef struct _FDSTACK_s {
    FDIO_t		io;
/*@dependent@*/ void *	fp;
    int			fdno;
} FDSTACK_t;

/** \ingroup rpmio
 * Cumulative statistics for an I/O operation.
 */
typedef struct {
    int			count;	/*!< Number of operations. */
    off_t		bytes;	/*!< Number of bytes transferred. */
    time_t		msecs;	/*!< Number of milli-seconds. */
} OPSTAT_t;

/** \ingroup rpmio
 * Identify per-desciptor I/O operation statistics.
 */
enum FDSTAT_e {
    FDSTAT_READ		= 0,	/*!< Read statistics index. */
    FDSTAT_WRITE	= 1,	/*!< Write statistics index. */
    FDSTAT_SEEK		= 2,	/*!< Seek statistics index. */
    FDSTAT_CLOSE	= 3	/*!< Close statistics. index */
};

/** \ingroup rpmio
 * Cumulative statistics for a descriptor.
 */
typedef	/*@abstract@*/ struct {
    struct timeval	create;	/*!< Structure creation time. */
    struct timeval	begin;	/*!< Operation start time. */
    OPSTAT_t		ops[4];	/*!< Cumulative statistics. */
} * FDSTAT_t;

/** \ingroup rpmio
 * Bit(s) to control digest operation.
 */
typedef enum rpmDigestFlags_e {
    RPMDIGEST_MD5	= (1 <<  0),	/*!< MD5 digest. */
    RPMDIGEST_SHA1	= (1 <<  1),	/*!< SHA1 digest. */
    RPMDIGEST_REVERSE	= (1 << 16),	/*!< Should bytes be reversed? */
    RPMDIGEST_BCSWAP	= (1 << 17),	/*!< Should bit count be reversed? */
} rpmDigestFlags;

typedef /*@abstract@*/ struct DIGEST_CTX_s * DIGEST_CTX;

/** \ingroup rpmio
 * Initialize digest.
 * Set bit count to 0 and buffer to mysterious initialization constants.
 * @param flags		bit(s) to control digest operation
 * @return		digest private data
 */
DIGEST_CTX rpmDigestInit(rpmDigestFlags flags)
	/*@*/;

/** \ingroup rpmio
 * Update context to with next plain text buffer.
 * @param private	private data
 * @param data		next data buffer
 * @param len		no. bytes of data
 */
void rpmDigestUpdate(DIGEST_CTX ctx, const void * data, size_t len)
	/*@modifies ctx @*/;

/** \ingroup rpmio
 * Return digest and destroy context.
 * Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 *
 * @param private	private data
 * @retval datap	address of returned digest
 * @retval lenp		address of digest length
 * @param asAscii	return digest as ascii string?
 */
void rpmDigestFinal(/*@only@*/ DIGEST_CTX ctx,
	/*@null@*/ /*@out@*/ void ** datap,
	/*@null@*/ /*@out@*/ size_t * lenp, int asAscii)
		/*@modifies *datap, *lenp @*/;

/** \ingroup rpmio
 * The FD_t File Handle data structure.
 */
struct _FD_s {
/*@refs@*/ int	nrefs;
    int		flags;
#define	RPMIO_DEBUG_IO		0x40000000
#define	RPMIO_DEBUG_REFS	0x20000000
    int		magic;
#define	FDMAGIC			0x04463138
    int		nfps;
    FDSTACK_t	fps[8];
    int		urlType;	/* ufdio: */

/*@dependent@*/ void *	url;	/* ufdio: URL info */
    int		rd_timeoutsecs;	/* ufdRead: per FD_t timer */
    ssize_t	bytesRemain;	/* ufdio: */
    ssize_t	contentLength;	/* ufdio: */
    int		persist;	/* ufdio: */
    int		wr_chunked;	/* ufdio: */

    int		syserrno;	/* last system errno encountered */
/*@observer@*/ const void *errcookie;	/* gzdio/bzdio/ufdio: */

    FDSTAT_t	stats;		/* I/O statistics */
/*@owned@*/ /*@null@*/ DIGEST_CTX digest;	/* Digest private data */

    int		ftpFileDoneNeeded; /* ufdio: (FTP) */
    unsigned int firstFree;	/* fadio: */
    long int	fileSize;	/* fadio: */
    long int	fd_cpioPos;	/* cpio: */
};
/*@access FD_t@*/

#define	FDSANE(fd)	assert(fd && fd->magic == FDMAGIC)

/*@-redecl@*/
extern int _rpmio_debug;
/*@=redecl@*/

#define DBG(_f, _m, _x) \
    if ((_rpmio_debug | ((_f) ? ((FD_t)(_f))->flags : 0)) & (_m)) fprintf _x

#define DBGIO(_f, _x)   DBG((_f), RPMIO_DEBUG_IO, _x)
#define DBGREFS(_f, _x) DBG((_f), RPMIO_DEBUG_REFS, _x)

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmio
 */
int fdFgets(FD_t fd, char * buf, size_t len)
	/*@modifies *buf, fd, fileSystem @*/;

/** \ingroup rpmio
 */
/*@null@*/ FD_t ftpOpen(const char *url, /*@unused@*/ int flags,
                /*@unused@*/ mode_t mode, /*@out@*/ urlinfo *uret)
	/*@modifies *uret, fileSystem @*/;

/** \ingroup rpmio
 */
int ftpReq(FD_t data, const char * ftpCmd, const char * ftpArg)
	/*@modifies data, fileSystem @*/;

/** \ingroup rpmio
 */
int ftpCmd(const char * cmd, const char * url, const char * arg2)
	/*@modifies fileSystem @*/;

/** \ingroup rpmio
 */
int ufdClose( /*@only@*/ void * cookie)
	/*@modified cookie, fileSystem @*/;

/** \ingroup rpmio
 */
/*@unused@*/ static inline
/*@null@*/ const FDIO_t fdGetIo(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    return fd->fps[fd->nfps].io;
}

/** \ingroup rpmio
 */
/*@-nullstate@*/ /* FIX: io may be NULL */
/*@unused@*/ static inline
void fdSetIo(FD_t fd, /*@kept@*/ /*@null@*/ FDIO_t io)
	/*@modifies fd @*/
{
    FDSANE(fd);
    /*@-assignexpose@*/
    fd->fps[fd->nfps].io = io;
    /*@=assignexpose@*/
}
/*@=nullstate@*/

/** \ingroup rpmio
 */
/*@unused@*/ static inline
/*@exposed@*/ /*@dependent@*/ /*@null@*/ FILE * fdGetFILE(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    /*@+voidabstract@*/
    return ((FILE *)fd->fps[fd->nfps].fp);
    /*@=voidabstract@*/
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
/*@exposed@*/ /*@dependent@*/ /*@null@*/ void * fdGetFp(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    return fd->fps[fd->nfps].fp;
}

/** \ingroup rpmio
 */
/*@-nullstate@*/ /* FIX: fp may be NULL */
/*@unused@*/ static inline
void fdSetFp(FD_t fd, /*@kept@*/ /*@null@*/ void * fp)
	/*@modifies fd @*/
{
    FDSANE(fd);
    /*@-assignexpose@*/
    fd->fps[fd->nfps].fp = fp;
    /*@=assignexpose@*/
}
/*@=nullstate@*/

/** \ingroup rpmio
 */
/*@unused@*/ static inline
int fdGetFdno(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    return fd->fps[fd->nfps].fdno;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdSetFdno(FD_t fd, int fdno)
	/*@modifies fd @*/
{
    FDSANE(fd);
    fd->fps[fd->nfps].fdno = fdno;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdSetContentLength(FD_t fd, ssize_t contentLength)
	/*@modifies fd @*/
{
    FDSANE(fd);
    fd->contentLength = fd->bytesRemain = contentLength;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdPush(FD_t fd, FDIO_t io, void * fp, int fdno)
	/*@modifies fd @*/
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
/*@unused@*/ static inline void fdPop(FD_t fd)
	/*@modifies fd @*/
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
/*@unused@*/ static inline void fdstat_enter(/*@null@*/ FD_t fd, int opx)
	/*@modifies fd @*/
{
    if (fd == NULL || fd->stats == NULL) return;
    fd->stats->ops[opx].count++;
    (void) gettimeofday(&fd->stats->begin, NULL);
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
time_t tvsub(/*@null@*/ const struct timeval * etv,
		/*@null@*/ const struct timeval * btv)
	/*@*/
{
    time_t secs, usecs;
    if (etv == NULL  || btv == NULL) return 0;
    secs = etv->tv_sec - btv->tv_sec;
    for (usecs = etv->tv_usec - btv->tv_usec; usecs < 0; usecs += 1000000)
	secs++;
    return ((secs * 1000) + (usecs/1000));
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdstat_exit(/*@null@*/ FD_t fd, int opx, ssize_t rc)
	/*@modifies fd @*/
{
    struct timeval end;
    if (fd == NULL) return;
    if (rc == -1) fd->syserrno = errno;
    if (fd->stats == NULL) return;
    (void) gettimeofday(&end, NULL);
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

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdstat_print(/*@null@*/ FD_t fd, const char * msg, FILE * fp)
	/*@modifies *fp, fileSystem @*/
{
    int opx;
    if (fd == NULL || fd->stats == NULL) return;
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

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdSetSyserrno(FD_t fd, int syserrno, /*@kept@*/ const void * errcookie)
	/*@modifies fd @*/
{
    FDSANE(fd);
    fd->syserrno = syserrno;
    /*@-assignexpose@*/
    fd->errcookie = errcookie;
    /*@=assignexpose@*/
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
int fdGetRdTimeoutSecs(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    return fd->rd_timeoutsecs;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
long int fdGetCpioPos(FD_t fd)
	/*@*/
{
    FDSANE(fd);
    return fd->fd_cpioPos;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdSetCpioPos(FD_t fd, long int cpioPos)
	/*@modifies fd @*/
{
    FDSANE(fd);
    fd->fd_cpioPos = cpioPos;
}

/** \ingroup rpmio
 */
/*@mayexit@*/ /*@unused@*/ static inline
FD_t c2f(/*@null@*/ void * cookie)
	/*@*/
{
    /*@-castexpose@*/
    FD_t fd = (FD_t) cookie;
    /*@=castexpose@*/
    FDSANE(fd);
    /*@-refcounttrans -retalias@*/ return fd; /*@=refcounttrans =retalias@*/
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdInitMD5(FD_t fd, int flags)
	/*@modifies fd @*/
{
    if (flags) flags = RPMDIGEST_REVERSE;
    flags |= RPMDIGEST_MD5;
    fd->digest = rpmDigestInit(flags);
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdInitSHA1(FD_t fd, int flags)
	/*@modifies fd @*/
{
    if (flags) flags = RPMDIGEST_REVERSE;
    flags |= RPMDIGEST_SHA1;
    fd->digest = rpmDigestInit(flags);
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdFiniMD5(FD_t fd,
		/*@null@*/ /*@out@*/ void ** datap,
		/*@null@*/ /*@out@*/ size_t * lenp,
		int asAscii)
	/*@modifies fd, *datap, *lenp @*/
{
    if (fd->digest == NULL) {
	if (datap) *datap = NULL;
	if (lenp) *lenp = 0;
	return;
    }
    /*@-mayaliasunique@*/
    rpmDigestFinal(fd->digest, datap, lenp, asAscii);
    /*@=mayaliasunique@*/
    fd->digest = NULL;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline
void fdFiniSHA1(FD_t fd,
		/*@null@*/ /*@out@*/ void ** datap,
		/*@null@*/ /*@out@*/ size_t * lenp,
		int asAscii)
	/*@modifies fd, *datap, *lenp @*/
{
    if (fd->digest == NULL) {
	if (datap) *datap = NULL;
	if (lenp) *lenp = 0;
	return;
    }
    /*@-mayaliasunique@*/
    rpmDigestFinal(fd->digest, datap, lenp, asAscii);
    /*@=mayaliasunique@*/
    fd->digest = NULL;
}

/*@-shadow@*/
/** \ingroup rpmio
 */
/*@unused@*/ static inline
int fdFileno(/*@null@*/ void * cookie)
	/*@*/
{
    FD_t fd;
    if (cookie == NULL) return -2;
    fd = c2f(cookie);
    return fd->fps[0].fdno;
}
/*@=shadow@*/

#ifdef __cplusplus
}
#endif

#endif	/* H_RPMIO_INTERNAL */
