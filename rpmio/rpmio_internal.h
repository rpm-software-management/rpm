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
};

#define	FDSANE(fd)	assert(fd && fd->magic == FDMAGIC)

#ifdef __cplusplus
extern "C" {
#endif

static inline
void fdSetBundle(FD_t fd, rpmDigestBundle bundle)
{
    FDSANE(fd);
    fd->digests = bundle;
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
