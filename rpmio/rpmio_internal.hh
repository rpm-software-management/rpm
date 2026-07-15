#ifndef	H_RPMIO_INTERNAL
#define	H_RPMIO_INTERNAL

/** \ingroup rpmio
 * \file rpmio_internal.h
 */

#include <rpm/rpmio.h>
#include <rpm/rpmpgp.h>

RPM_PRIVATE_API
void fdSetBundle(FD_t fd, rpmDigestBundle bundle);
RPM_PRIVATE_API
rpmDigestBundle fdGetBundle(FD_t fd, int create);

/** \ingroup rpmio
 * Attach digest to fd.
 */
RPM_PRIVATE_API
void fdInitDigest(FD_t fd, int hashalgo, rpmDigestFlags flags);

RPM_PRIVATE_API
void fdInitDigestID(FD_t fd, int hashalgo, int id, rpmDigestFlags flags);

/** \ingroup rpmio
 */
RPM_PRIVATE_API
void fdFiniDigest(FD_t fd, int id,
		void ** datap,
		size_t * lenp,
		int asAscii);

RPM_PRIVATE_API
DIGEST_CTX fdDupDigest(FD_t fd, int id);

/**
 * Read an entire file into a buffer.
 * @param fn		file name to read
 * @param[out] *bp		(malloc'd) buffer address
 * @param[out] *blenp	(malloc'd) buffer length
 * @return		0 on success
 */
RPM_PRIVATE_API
int rpmioSlurp(const char * fn,
                uint8_t ** bp, ssize_t * blenp);

/**
 * Set close-on-exec flag for all opened file descriptors, except
 * stdin/stdout/stderr.
 */
RPM_PRIVATE_API
void rpmSetCloseOnExec(void);

typedef const struct FDIO_s * FDIO_t;

extern const FDIO_t fdio;
extern const FDIO_t ufdio;
extern const FDIO_t gzdio;
#ifdef HAVE_BZLIB_H
extern const FDIO_t bzdio;
#endif
#ifdef HAVE_LZMA_H
extern const FDIO_t xzdio;
extern const FDIO_t lzdio;
#endif
#ifdef HAVE_ZSTD
extern const FDIO_t zstdio;
#endif

#endif	/* H_RPMIO_INTERNAL */
