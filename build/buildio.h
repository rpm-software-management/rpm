#ifndef	_H_BUILDIO_
#define	_H_BUILDIO_

/** \ingroup rpmbuild
 * \file build/buildio.h
 * Routines to read and write packages.
 * @deprecated this information will move elsewhere eventually.
 * @todo Eliminate, merge into rpmlib.
 */

#include <rpm/rpmbuild.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
typedef struct cpioSourceArchive_s {
    rpm_loff_t	cpioArchiveSize;
    FD_t	cpioFdIn;
    rpmfi	cpioList;
} * CSA_t;

/**
 * Read rpm package components from file.
 * @param fileName	file name of package (or NULL to use stdin)
 * @retval specp	spec structure to carry package header (or NULL)
 * @retval sigs		package signature
 * @param csa
 * @return		RPMRC_OK on success
 */
rpmRC readRPM(const char * fileName,
		rpmSpec * specp,
		Header * sigs,
		CSA_t csa);

/**
 * Write rpm package to file.
 *
 * @warning The first argument (header) is now passed by reference in order to
 * return a reloaded contiguous header to the caller.
 *
 * @retval *hdrp	header to write (final header is returned).
 * @retval *pkgidp	header+payload MD5 of package (NULL to disable).
 * @param fileName	file name of package
 * @param csa
 * @param passPhrase
 * @retval cookie	generated cookie (i.e build host/time)
 * @return		RPMRC_OK on success
 */
rpmRC writeRPM(Header * hdrp, unsigned char ** pkgidp,
		const char * fileName,
		CSA_t csa,
		char * passPhrase,
		char ** cookie);

#ifdef __cplusplus
}
#endif

#endif	/* _H_BUILDIO_ */
