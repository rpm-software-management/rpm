#ifndef	_H_BUILDIO_
#define	_H_BUILDIO_

/** \ingroup rpmbuild
 * \file build/buildio.h
 * Routines to read and write packages.
 * @deprecated this information will move elsewhere eventually.
 * @todo Eliminate, merge into rpmlib.
 */

#include "rpmbuild.h"

/**
 */
typedef /*@abstract@*/ struct cpioSourceArchive_s {
    unsigned int cpioArchiveSize;
    FD_t	cpioFdIn;
    TFI_t	cpioList;
/*@only@*/ struct rpmlead * lead;	/* XXX FIXME: exorcize lead/arch/os */
} * CSA_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read rpm package components from file.
 * @param fileName	file name of package (or NULL to use stdin)
 * @retval specp	spec structure to carry package header (or NULL)
 * @retval lead		package lead
 * @retval sigs		package signature
 * @param csa
 * @return		0 on success
 */
/*@unused@*/ int readRPM(/*@null@*/ const char * fileName,
		/*@out@*/ Spec * specp,
		/*@out@*/ struct rpmlead * lead,
		/*@out@*/ Header * sigs,
		CSA_t csa)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies *specp, *lead, *sigs, csa, csa->cpioFdIn,
		rpmGlobalMacroContext, fileSystem, internalState @*/;

/**
 * Write rpm package to file.
 *
 * @warning The first argument (header) is now passed by reference in order to
 * return a reloaded contiguous header to the caller.
 *
 * @retval hdrp		header to write (final header is returned).
 * @param fileName	file name of package
 * @param type		RPMLEAD_SOURCE/RPMLEAD_BINARY
 * @param csa
 * @param passPhrase
 * @retval cookie	generated cookie (i.e build host/time)
 * @return		0 on success
 */
int writeRPM(Header * hdrp,
		const char * fileName,
		int type,
		CSA_t csa,
		/*@null@*/ char * passPhrase,
		/*@out@*/ const char ** cookie)
	/*@globals rpmGlobalMacroContext,
		fileSystem, internalState @*/
	/*@modifies *hdrp, *cookie, csa, csa->cpioArchiveSize,
		rpmGlobalMacroContext, fileSystem, internalState @*/;

#ifdef __cplusplus
}
#endif

#endif	/* _H_BUILDIO_ */
