#ifndef H_SIGNATURE
#define	H_SIGNATURE

/** \ingroup signature
 * \file lib/signature.h
 * Generate and verify signatures.
 */

#include <header.h>

/** \ingroup signature
 * Signature types stored in rpm lead.
 */
typedef	enum sigType_e {
    RPMSIGTYPE_NONE	= 0,	/*!< unused, legacy. */
    RPMSIGTYPE_PGP262_1024 = 1,	/*!< unused, legacy. */
/*@-enummemuse@*/
    RPMSIGTYPE_BAD	= 2,	/*!< Unknown signature type. */
/*@=enummemuse@*/
    RPMSIGTYPE_MD5	= 3,	/*!< unused, legacy. */
    RPMSIGTYPE_MD5_PGP	= 4,	/*!< unused, legacy. */
    RPMSIGTYPE_HEADERSIG= 5,	/*!< Header style signature */
    RPMSIGTYPE_DISABLE	= 6,	/*!< Disable verification (debugging only) */
} sigType;

/** \ingroup signature
 * Identify PGP versions.
 * @note Greater than 0 is a valid PGP version.
 */
typedef enum pgpVersion_e {
    PGP_NOTDETECTED	= -1,
    PGP_UNKNOWN		= 0,
    PGP_2		= 2,
    PGP_5		= 5
} pgpVersion;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup signature
 * Return new, empty (signature) header instance.
 * @return		signature header
 */
Header rpmNewSignature(void)
	/*@*/;

/** \ingroup signature
 * Read (and verify header+archive size) signature header.
 * If an old-style signature is found, we emulate a new style one.
 * @param fd		file handle
 * @retval headerp	address of (signature) header (or NULL)
 * @param sig_type	type of signature header to read (from lead)
 * @return		rpmRC return code
 */
rpmRC rpmReadSignature(FD_t fd, /*@null@*/ /*@out@*/ Header *headerp,
		sigType sig_type)
	/*@globals fileSystem@*/
	/*@modifies fd, *headerp, fileSystem @*/;

/** \ingroup signature
 * Write signature header.
 * @param fd		file handle
 * @param h		(signature) header
 * @return		0 on success, 1 on error
 */
int rpmWriteSignature(FD_t fd, Header h)
	/*@globals fileSystem@*/
	/*@modifies fd, h, fileSystem @*/;

/** \ingroup signature
 *  Generate a signature of data in file, insert in header.
 */
int rpmAddSignature(Header h, const char * file,
		    int_32 sigTag, /*@null@*/ const char * passPhrase)
	/*@globals rpmGlobalMacroContext,
		fileSystem@*/
	/*@modifies h, fileSystem @*/;

/******************************************************************/

/* Possible actions for rpmLookupSignatureType() */
#define RPMLOOKUPSIG_QUERY	0	/* Lookup type in effect          */
#define RPMLOOKUPSIG_DISABLE	1	/* Disable (--sign was not given) */
#define RPMLOOKUPSIG_ENABLE	2	/* Re-enable %_signature          */

/** \ingroup signature
 * Return type of signature in effect for building.
 */
int rpmLookupSignatureType(int action)
	/*@globals rpmGlobalMacroContext,
		internalState@*/
	/*@modifies internalState @*/;

/** \ingroup signature
 *  Read a pass phrase from the user.
 */
/*@null@*/ char * rpmGetPassPhrase(const char *prompt, const int sigTag)
	/*@globals rpmGlobalMacroContext,
		fileSystem@*/
	/*@modifies rpmGlobalMacroContext,
		fileSystem @*/;

/** \ingroup signature
 *  Return path to pgp executable of given type, or NULL when not found.
 */
/*@-exportlocal -redecl@*/
/*@null@*/ const char * rpmDetectPGPVersion(
			/*@null@*/ /*@out@*/ pgpVersion * pgpVer)
	/*@globals rpmGlobalMacroContext @*/
	/*@modifies *pgpVer @*/;
/*@=exportlocal =redecl@*/

#ifdef __cplusplus
}
#endif

#endif	/* H_SIGNATURE */
