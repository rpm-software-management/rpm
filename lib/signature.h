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
    RPMSIGTYPE_BAD	= 2,	/*!< Unknown signature type. */
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
Header rpmNewSignature(void);

/** \ingroup signature
 * Read (and verify header+archive size) signature header.
 * If an old-style signature is found, we emulate a new style one.
 * @param fd		file handle
 * @retval header	address of (signature) header
 * @param sig_type	type of signature header to read (from lead).
 * @return		0 on success, 1 on error
 */
int rpmReadSignature(FD_t fd, /*@out@*/ Header *header, sigType sig_type);

/** \ingroup signature
 * Write signature header.
 * @param fd		file handle
 * @param header	(signature) header
 * @return		0 on success, 1 on error
 */
int rpmWriteSignature(FD_t fd, Header header);

/** \ingroup signature
 *  Generate a signature of data in file, insert in header.
 */
int rpmAddSignature(Header h, const char *file,
		    int_32 sigTag, const char *passPhrase);

/******************************************************************/

/* Possible actions for rpmLookupSignatureType() */
#define RPMLOOKUPSIG_QUERY	0	/* Lookup type in effect          */
#define RPMLOOKUPSIG_DISABLE	1	/* Disable (--sign was not given) */
#define RPMLOOKUPSIG_ENABLE	2	/* Re-enable %_signature          */

/** \ingroup signature
 * Return type of signature in effect for building.
 */
int rpmLookupSignatureType(int action);

/** \ingroup signature
 *  Read a pass phrase from the user.
 */
char * rpmGetPassPhrase(const char *prompt, const int sigTag);

/** \ingroup signature
 *  Return path to pgp executable of given type, or NULL when not found.
 */
/*@null@*/ const char * rpmDetectPGPVersion( /*@out@*/ pgpVersion *pgpVersion);

#ifdef __cplusplus
}
#endif

#endif	/* H_SIGNATURE */
