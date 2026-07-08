#ifndef _RPMKEYRING_H
#define _RPMKEYRING_H

/** \ingroup rpmkeyring
 * \file rpmkeyring.h
 *
 * RPM keyring API
 */

#include <rpm/rpmtypes.h>
#include <rpm/argv.h>
#include <rpm/rpmpgp.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmkeyring
  * Operation mode definitions for rpmKeyringModify
  *   ADD: add a new key, merge with pre-existing key
  *   DELETE: delete an existing key
  */
typedef enum rpmKeyringModifyMode_e {
    RPMKEYRING_ADD	= 1,
    RPMKEYRING_DELETE	= 2,
} rpmKeyringModifyMode;


/** \ingroup rpmkeyring
 * Create a new, empty keyring
 * @return	new keyring handle
 */
RPM_PUBLIC_API
rpmKeyring rpmKeyringNew(void);

/** \ingroup rpmkeyring
 * Free keyring and the keys within it
 * @return	NULL always
 */
RPM_PUBLIC_API
rpmKeyring rpmKeyringFree(rpmKeyring keyring);

/** \ingroup rpmkeyring
 * Add a public key to keyring.
 * @param keyring	keyring handle
 * @param key		pubkey handle
 * @return		0 on success, -1 on error, 1 if key already present
 */
RPM_PUBLIC_API
int rpmKeyringAddKey(rpmKeyring keyring, rpmPubkey key);

/** \ingroup rpmkeyring
 * Get iterator for all the primary keys in the keyring
 * @param keyring       keyring handle
 * @param unused	reserved for future use, must be 0
 * @return		iterator or NULL
 */
RPM_PUBLIC_API
rpmKeyringIterator rpmKeyringInitIterator(rpmKeyring keyring, int unused);

/** \ingroup rpmkeyring
 * Get next key in keyring
 * @param iterator	iterator handle
 * @return		weak reference to next public key
 *			or NULL if end is reached
 */
RPM_PUBLIC_API
rpmPubkey rpmKeyringIteratorNext(rpmKeyringIterator iterator);

/** \ingroup rpmkeyring
 * Free iterator
 * @param iterator	iterator handle
 * @return		NULL
 */
RPM_PUBLIC_API
rpmKeyringIterator rpmKeyringIteratorFree(rpmKeyringIterator iterator);

/** \ingroup rpmkeyring
 * Perform combined keyring lookup and signature verification
 * @param keyring	keyring handle
 * @param sig		OpenPGP signature parameters
 * @param ctx		signature hash context
 * @return		RPMRC_OK / RPMRC_FAIL / RPMRC_NOKEY
 */
RPM_PUBLIC_API
rpmRC rpmKeyringVerifySig(rpmKeyring keyring, pgpDigParams sig, DIGEST_CTX ctx);

/** \ingroup rpmkeyring
 * Perform combined keyring lookup and signature verification (DEPRECATED)
 * @param keyring	keyring handle
 * @param sig		OpenPGP signature parameters
 * @param ctx		signature hash context
 * @param keyptr	matching key (refcounted)
 * @return		RPMRC_OK / RPMRC_FAIL / RPMRC_NOKEY
 */
RPM_PUBLIC_API
rpmRC rpmKeyringVerifySig2(rpmKeyring keyring, pgpDigParams sig, DIGEST_CTX ctx,  rpmPubkey * keyptr);

/** \ingroup rpmkeyring
 * Perform combined keyring lookup and signature verification
 * @param keyring	keyring handle
 * @param sig		OpenPGP signature parameters
 * @param ctx		signature hash context
 * @param keyptr	matching key (refcounted)
 * @param lints		lints (if any)
 * @return		RPMRC_OK / RPMRC_FAIL / RPMRC_NOKEY
 */
RPM_PUBLIC_API
rpmRC rpmKeyringVerifySig3(rpmKeyring keyring, pgpDigParams sig, DIGEST_CTX ctx,
			   rpmPubkey * keyptr, ARGV_t *lints);

/** \ingroup rpmkeyring
 * Reference a keyring.
 * @param keyring	keyring handle
 * @return		new keyring reference
 */
RPM_PUBLIC_API
rpmKeyring rpmKeyringLink(rpmKeyring keyring);

/** \ingroup rpmkeyring
 * Create a new rpmPubkey from OpenPGP packet
 * @param pkt		OpenPGP packet data
 * @param pktlen	Data length
 * @return		new pubkey handle
 */
RPM_PUBLIC_API
rpmPubkey rpmPubkeyNew(const uint8_t *pkt, size_t pktlen);

/** \ingroup rpmkeyring
 * Return array of subkeys belonging to primarykey
 * param primarykey	primary rpmPubkey
 * param count		count of returned subkeys
 * @return		an array of subkey's handles
 */
RPM_PUBLIC_API
rpmPubkey *rpmGetSubkeys(rpmPubkey primarykey, int *count);

/** \ingroup rpmkeyring
 * Create a new rpmPubkey from ASCII-armored pubkey file
 * @param filename	Path to pubkey file
 * @return		new pubkey handle
 */
RPM_PUBLIC_API
rpmPubkey rpmPubkeyRead(const char *filename);

/** \ingroup rpmkeyring
 * Free a pubkey.
 * @param key		Pubkey to free
 * @return		NULL always
 */
RPM_PUBLIC_API
rpmPubkey rpmPubkeyFree(rpmPubkey key);

/** \ingroup rpmkeyring
 * Reference a pubkey.
 * @param key		Pubkey
 * @return		new pubkey reference
 */
RPM_PUBLIC_API
rpmPubkey rpmPubkeyLink(rpmPubkey key);

/** \ingroup rpmkeyring
 * Return base64 encoding of pubkey
 * @param key           Pubkey
 * @return              base64 encoded pubkey (malloced), NULL on error
 */
RPM_PUBLIC_API
char * rpmPubkeyBase64(rpmPubkey key);

/** \ingroup rpmkeyring
 * Return base64 encoding of pubkey
 * @param key           Pubkey
 * @return              armored pubkey (malloced), NULL on error
 */
RPM_PUBLIC_API
char * rpmPubkeyArmorWrap(rpmPubkey key);

/** \ingroup rpmkeyring
 * Return fingerprint of primary key
 * @param key		Pubkey
 * @param fp		Fingerprint data
 * @param fplen		Length of Fingerprint
 * @return		0 on success
 */
RPM_PUBLIC_API
int rpmPubkeyFingerprint(rpmPubkey key, uint8_t **fp, size_t *fplen);

/** \ingroup rpmkeyring
 * Return fingerprint of primary key as hex string
 * @param key		Pubkey
 * @return		string or NULL on failure
 */
RPM_PUBLIC_API
const char * rpmPubkeyFingerprintAsHex(rpmPubkey key);

/** \ingroup rpmkeyring
 * Return key ID of key as hex string
 * @param key		Pubkey
 * @return		string or NULL on failure
 */
RPM_PUBLIC_API
const char * rpmPubkeyKeyIDAsHex(rpmPubkey key);

/** \ingroup rpmkeyring
 * Return pgp params of key
 * @param key		Pubkey
 * @return		pgp params, NULL on error
 */
RPM_PUBLIC_API
pgpDigParams rpmPubkeyPgpDigParams(rpmPubkey key);

/** \ingroup rpmkeyring
 * Lookup a pubkey in the keyring
 * @param keyring      keyring handle
 * @param key          Pubkey to find in keyring
 * @return             pubkey handle, NULL if not found
 */
RPM_PUBLIC_API
rpmPubkey rpmKeyringLookupKey(rpmKeyring keyring, rpmPubkey key);

/** \ingroup rpmkeyring
 * Modify the keys in the keyring
 * @param keyring      keyring handle
 * @param key		pubkey handle
 * @param mode          mode of operation
 * @return		0 on success, -1 on error, 1 if the operation did not
 *                      change anything (key already present for RPMKEYRING_ADD,
 *                      key not found for RPMKEYRING_DELETE)
 */
RPM_PUBLIC_API
int rpmKeyringModify(rpmKeyring keyring, rpmPubkey key, rpmKeyringModifyMode mode);

/** \ingroup rpmkeyring
 * Merge the data of two pubkeys describing the same key
 * @param oldkey       old Pubkey
 * @param newkey       new Pubkey
 * @param mergedkeyp   merged Pubkey, NULL if identical to old Pubkey
 * @return             RPMRC_OK / RPMRC_FAIL
 */
RPM_PUBLIC_API
rpmRC rpmPubkeyMerge(rpmPubkey oldkey, rpmPubkey newkey, rpmPubkey *mergedkeyp);

#ifdef __cplusplus
}
#endif
#endif /* _RPMKEYDB_H */
