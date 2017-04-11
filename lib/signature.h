#ifndef H_SIGNATURE
#define	H_SIGNATURE

/** \ingroup signature
 * \file lib/signature.h
 * Generate and verify signatures.
 */

#include <rpm/header.h>

enum {
    RPMSIG_UNKNOWN_TYPE		= 0,
    RPMSIG_DIGEST_TYPE		= 1,
    RPMSIG_SIGNATURE_TYPE	= 2,
    RPMSIG_OTHER_TYPE		= 3,
};

/* siginfo range bits */
enum {
    RPMSIG_HEADER	= (1 << 0),
    RPMSIG_PAYLOAD	= (1 << 1),
};

struct rpmsinfo_s {
    rpmTagVal tag;
    int id;
    int hashalgo;
    int range;
    int type;
    unsigned int keyid;
    union {
	pgpDigParams sig;
	char *dig;
    };
};

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup signature
 * Read (and verify header+payload size) signature header.
 * If an old-style signature is found, we emulate a new style one.
 * @param fd		file handle
 * @retval sighp	address of (signature) header (or NULL)
 * @retval msg		failure msg
 * @return		rpmRC return code
 */
rpmRC rpmReadSignature(FD_t fd, Header *sighp, char ** msg);

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
 * Generate signature and write to file
 * @param SHA256	SHA256 digest
 * @param SHA1		SHA1 digest
 * @param MD5		MD5 digest
 * @param size		size of header
 * @param payloadSize	size of archive
 * @param fd		output file
 */
rpmRC rpmGenerateSignature(char *SHA256, char *SHA1, uint8_t *MD5,
			rpm_loff_t size, rpm_loff_t payloadSize, FD_t fd);

RPM_GNUC_INTERNAL
rpmRC rpmsinfoInit(rpmtd td, const char *origin,
                     struct rpmsinfo_s *sigt, char **msg);


RPM_GNUC_INTERNAL
void rpmsinfoFini(struct rpmsinfo_s *sinfo);
#ifdef __cplusplus
}
#endif

#endif	/* H_SIGNATURE */
