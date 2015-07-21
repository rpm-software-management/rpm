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

enum {
    RPMSIG_UNKNOWN_TYPE		= 0,
    RPMSIG_DIGEST_TYPE		= 1,
    RPMSIG_SIGNATURE_TYPE	= 2,
    RPMSIG_OTHER_TYPE		= 3,
};

struct sigtInfo_s {
    int hashalgo;
    int payload;
    int type;
};

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
 * Verify a signature from a package.
 *
 * @param keyring	keyring handle
 * @param sigtd		signature tag data container
 * @param sig		signature/pubkey parameters
 * @param ctx		digest context
 * @retval result	detailed text result of signature verification
 * 			(malloc'd)
 * @return		result of signature verification
 */
rpmRC rpmVerifySignature(rpmKeyring keyring, rpmtd sigtd, pgpDigParams sig,
			 DIGEST_CTX ctx, char ** result);

/** \ingroup signature
 * Destroy signature header from package.
 * @param h		signature header
 * @return		NULL always
 */
Header rpmFreeSignature(Header h);

/** \ingroup signature
 * Generate signature and write to file
 * @param SHA1		SHA1 digest
 * @param MD5		MD5 digest
 * @param size		size of header
 * @param payloadSize	size of archive
 * @param fd		output file
 */
rpmRC rpmGenerateSignature(char *SHA1, uint8_t *MD5, rpm_loff_t size,
				rpm_loff_t payloadSize, FD_t fd);

RPM_GNUC_INTERNAL
rpmRC rpmSigInfoParse(rpmtd td, const char *origin,
                     struct sigtInfo_s *sigt, pgpDigParams *sigp, char **msg);

#ifdef __cplusplus
}
#endif

#endif	/* H_SIGNATURE */
