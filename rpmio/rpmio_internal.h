#ifndef	H_RPMIO_INTERNAL
#define	H_RPMIO_INTERNAL

/** \ingroup rpmio
 * \file rpmio/rpmio_internal.h
 */

/*@-shadow@*/
static inline int fdFileno(void * cookie);
/*@=shadow@*/

#include <rpmio.h>
#include <rpmurl.h>

/** \ingroup rpmio
 */
typedef struct _FDSTACK_s {
	FDIO_t		io;
/*@dependent@*/ void *	fp;
	int		fdno;
} FDSTACK_t;

/** \ingroup rpmio
 * Cumulative statistics for an I/O operation.
 */
typedef struct {
	int		count;	/*!< Number of operations. */
	off_t		bytes;	/*!< Number of bytes transferred. */
	time_t		msecs;	/*!< Number of milli-seconds. */
} OPSTAT_t;

/** \ingroup rpmio
 * Identify per-desciptor I/O operation statistics.
 */
enum FDSTAT_e {
	FDSTAT_READ	= 0,	/*!< Read statistics index. */
	FDSTAT_WRITE	= 1,	/*!< Write statistics index. */
	FDSTAT_SEEK	= 2,	/*!< Seek statistics index. */
	FDSTAT_CLOSE	= 3	/*!< Close statistics. index */
};

/** \ingroup rpmio
 * Cumulative statistics for a descriptor.
 */
typedef	/*@abstract@*/ struct {
	struct timeval	create;	/*!< Structure creation time. */
	struct timeval	begin;	/*!< Operation start time. */
	OPSTAT_t	ops[4];	/*!< Cumulative statistics. */
} * FDSTAT_t;

/** \ingroup rpmio
 * Bit(s) to control digest operation.
 */
typedef enum rpmDigestFlags_e {
    RPMDIGEST_MD5	= (1 <<  0),	/*!< MD5 digest. */
    RPMDIGEST_SHA1	= (1 <<  1),	/*!< SHA1 digest. */
    RPMDIGEST_NATIVE	= (1 << 16),	/*!< Should bytes be reversed? */
} rpmDigestFlags;

typedef /*@abstract@*/ struct DIGEST_CTX_s * DIGEST_CTX;

/** \ingroup rpmio
 * Initialize digest.
 * Set bit count to 0 and buffer to mysterious initialization constants.
 * @param flags		bit(s) to control digest operation
 * @return		digest private data
 */
DIGEST_CTX rpmDigestInit(rpmDigestFlags flags);

/** \ingroup rpmio
 * Update context to with next plain text buffer.
 * @param private	private data
 * @param data		next data buffer
 * @param len		no. bytes of data
 */
void rpmDigestUpdate(DIGEST_CTX ctx, const void * data, size_t len);

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
void rpmDigestFinal(/*@only@*/ DIGEST_CTX ctx, /*@out@*/ void ** datap,
	/*@out@*/ size_t *lenp, int asAscii);

/** \ingroup rpmio
 * The FD_t File Handle data structure.
 */
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

	FDSTAT_t	stats;		/* I/O statistics */
/*@owned@*/ DIGEST_CTX	digest;		/* Digest private data */

	int		ftpFileDoneNeeded; /* ufdio: (FTP) */
	unsigned int	firstFree;	/* fadio: */
	long int	fileSize;	/* fadio: */
	long int	fd_cpioPos;	/* cpio: */
};
/*@access FD_t@*/

#define	FDSANE(fd)	assert(fd && fd->magic == FDMAGIC)

extern int _rpmio_debug;

#define DBG(_f, _m, _x) \
    if ((_rpmio_debug | ((_f) ? ((FD_t)(_f))->flags : 0)) & (_m)) fprintf _x

#define DBGIO(_f, _x)   DBG((_f), RPMIO_DEBUG_IO, _x)
#define DBGREFS(_f, _x) DBG((_f), RPMIO_DEBUG_REFS, _x)

#ifdef __cplusplus
extern "C" {
#endif

int fdFgets(FD_t fd, char * buf, size_t len);

/*@null@*/ FD_t ftpOpen(const char *url, /*@unused@*/ int flags,
                /*@unused@*/ mode_t mode, /*@out@*/ urlinfo *uret);
int ftpReq(FD_t data, const char * ftpCmd, const char * ftpArg);
int ftpCmd(const char * cmd, const char * url, const char * arg2);

int ufdClose( /*@only@*/ void * cookie);

/** \ingroup rpmio
 */
/*@unused@*/ static inline /*@null@*/ const FDIO_t fdGetIo(FD_t fd) {
    FDSANE(fd);
    return fd->fps[fd->nfps].io;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline void fdSetIo(FD_t fd, FDIO_t io) {
    FDSANE(fd);
    fd->fps[fd->nfps].io = io;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline /*@dependent@*/ /*@null@*/ FILE * fdGetFILE(FD_t fd) {
    FDSANE(fd);
    /*@+voidabstract@*/
    return ((FILE *)fd->fps[fd->nfps].fp);
    /*@=voidabstract@*/
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline /*@dependent@*/ /*@null@*/ void * fdGetFp(FD_t fd) {
    FDSANE(fd);
    return fd->fps[fd->nfps].fp;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline void fdSetFp(FD_t fd, /*@kept@*/ void * fp) {
    FDSANE(fd);
    fd->fps[fd->nfps].fp = fp;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline int fdGetFdno(FD_t fd) {
    FDSANE(fd);
    return fd->fps[fd->nfps].fdno;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline void fdSetFdno(FD_t fd, int fdno) {
    FDSANE(fd);
    fd->fps[fd->nfps].fdno = fdno;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline void fdSetContentLength(FD_t fd, ssize_t contentLength)
{
    FDSANE(fd);
    fd->contentLength = fd->bytesRemain = contentLength;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline void fdPush(FD_t fd, FDIO_t io,
	void * fp, int fdno)
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
/*@unused@*/ static inline void fdPop(FD_t fd) {
    FDSANE(fd);
    if (fd->nfps < 0) return;
    fdSetIo(fd, NULL);
    fdSetFp(fd, NULL);
    fdSetFdno(fd, -1);
    fd->nfps--;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline void fdstat_enter(FD_t fd, int opx)
{
    if (fd->stats == NULL) return;
    fd->stats->ops[opx].count++;
    gettimeofday(&fd->stats->begin, NULL);
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline time_t tvsub(struct timeval *etv, struct timeval *btv) {
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

/** \ingroup rpmio
 */
/*@unused@*/ static inline void fdstat_exit(FD_t fd, int opx, ssize_t rc)
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

/** \ingroup rpmio
 */
/*@unused@*/ static inline void fdstat_print(FD_t fd, const char * msg, FILE * fp) {
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

/** \ingroup rpmio
 */
/*@unused@*/ static inline void fdSetSyserrno(FD_t fd, int syserrno, /*@kept@*/ const void * errcookie) {
    FDSANE(fd);
    fd->syserrno = syserrno;
    fd->errcookie = errcookie;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline int fdGetRdTimeoutSecs(FD_t fd) {
    FDSANE(fd);
    return fd->rd_timeoutsecs;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline long int fdGetCpioPos(FD_t fd) {
    FDSANE(fd);
    return fd->fd_cpioPos;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline void fdSetCpioPos(FD_t fd, long int cpioPos) {
    FDSANE(fd);
    fd->fd_cpioPos = cpioPos;
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline FD_t c2f(void * cookie) {
    FD_t fd = (FD_t) cookie;
    FDSANE(fd);
    /*@-refcounttrans@*/ return fd; /*@=refcounttrans@*/
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline void fdInitMD5(FD_t fd, int flags) {
    if (flags) flags = RPMDIGEST_NATIVE;
    flags |= RPMDIGEST_MD5;
    fd->digest = rpmDigestInit(flags);
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline void fdInitSHA1(FD_t fd) {
    fd->digest = rpmDigestInit(RPMDIGEST_SHA1);
}

/** \ingroup rpmio
 */
/*@unused@*/ static inline void fdFiniMD5(FD_t fd, void **datap, size_t *lenp, int asAscii) {
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
/*@unused@*/ static inline void fdFiniSHA1(FD_t fd, void **datap, size_t *lenp, int asAscii) {
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
/*@unused@*/ static inline int fdFileno(void * cookie) {
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
