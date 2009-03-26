#ifndef H_SIGNATURE
#define	H_SIGNATURE

/** \ingroup signature
 * \file lib/signature.h
 * Generate and verify signatures.
 */

#include <rpm/header.h>

/** \ingroup signature
 * Signature types stored in rpm lead.
 */
typedef	enum sigType_e {
    RPMSIGTYPE_HEADERSIG= 5	/*!< Header style signature */
} sigType;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup signature
 * Return new, empty (signature) header instance.
 * @return		signature header
 */
Header rpmNewSignature(void);

/** \ingroup signature
 * Read (and verify header+payload size) signature header.
 * If an old-style signature is found, we emulate a new style one.
 * @param fd		file handle
 * @retval sighp	address of (signature) header (or NULL)
 * @param sig_type	type of signature header to read (from lead)
 * @retval msg		failure msg
 * @return		rpmRC return code
 */
rpmRC rpmReadSignature(FD_t fd, Header *sighp, sigType sig_type, char ** msg);

/** \ingroup signature
 * Write signature header.
 * @param fd		file handle
 * @param h		(signature) header
 * @return		0 on success, 1 on error
 */
int rpmWriteSignature(FD_t fd, Header h);

/** \ingroup signature
 * Generate signature(s) from a header+payload file, save in signature header.
 * @param sigh		signature header
 * @param file		header+payload file name
 * @param sigTag	type of signature(s) to add
 * @param passPhrase	private key pass phrase
 * @return		0 on success, -1 on failure
 */
int rpmAddSignature(Header sigh, const char * file,
		    rpmSigTag sigTag, const char * passPhrase);

/** \ingroup signature
 * Verify a signature from a package.
 *
 * @param keyring	keyring handle
 * @param sigtd		signature tag data container
 * @param dig		signature/pubkey parameters
 * @retval result	detailed text result of signature verification
 * 			(malloc'd)
 * @return		result of signature verification
 */
rpmRC rpmVerifySignature(rpmKeyring keyring, rpmtd sigtd, pgpDig dig, DIGEST_CTX ctx, char ** result);

/** \ingroup signature
 * Destroy signature header from package.
 * @param h		signature header
 * @return		NULL always
 */
Header rpmFreeSignature(Header h);

/******************************************************************/

/**
 *  Possible actions for rpmLookupSignatureType()
 */
#define RPMLOOKUPSIG_QUERY	0	/* Lookup type in effect          */
#define RPMLOOKUPSIG_DISABLE	1	/* Disable (--sign was not given) */
#define RPMLOOKUPSIG_ENABLE	2	/* Re-enable %_signature          */

/** \ingroup signature
 * Return type of signature needed for signing/building.
 * @param action	enable/disable/query action
 * @return		sigTag to use, 0 if none, -1 on error
 */
int rpmLookupSignatureType(int action);

/** \ingroup signature
 * Read a pass phrase using getpass(3), confirm with gpg/pgp helper binaries.
 * @param prompt	user prompt
 * @param sigTag	signature type/tag
 * @return		pass phrase
 */
char * rpmGetPassPhrase(const char * prompt, const rpmSigTag sigTag);

#ifdef __cplusplus
}
#endif

#endif	/* H_SIGNATURE */
