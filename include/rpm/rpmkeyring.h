#ifndef _RPMKEYRING_H
#define _RPMKEYRING_H

/** \ingroup rpmkeyring
 * \file rpmkeyring.h
 *
 * RPM keyring API
 */

#include <rpm/rpmtypes.h>
#include <rpm/rpmpgp.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmkeyring
  * Operation mode definitions for rpmKeyringModify
  *   ADD: add a new key, do nothing if the key is already present
  *   REPLACE: add a key, replace if already present
  *   DELETE: delete an existing key
  */
typedef enum rpmKeyringModifyMode_e {
    RPMKEYRING_ADD	= 1,
    RPMKEYRING_REPLACE	= 2,
    RPMKEYRING_DELETE	= 3
} rpmKeyringModifyMode;


/** \ingroup rpmkeyring
 * Create a new, empty keyring
 * @return	new keyring handle
 */
rpmKeyring rpmKeyringNew(void);

/** \ingroup rpmkeyring
 * Free keyring and the keys within it
 * @return	NULL always
 */
rpmKeyring rpmKeyringFree(rpmKeyring keyring);

/** \ingroup rpmkeyring
 * Add a public key to keyring.
 * @param keyring	keyring handle
 * @param key		pubkey handle
 * @return		0 on success, -1 on error, 1 if key already present
 */
int rpmKeyringAddKey(rpmKeyring keyring, rpmPubkey key);

/** \ingroup rpmkeyring
 * Perform combined keyring lookup and signature verification
 * @param keyring	keyring handle
 * @param sig		OpenPGP signature parameters
 * @param ctx		signature hash context
 * @return		RPMRC_OK / RPMRC_FAIL / RPMRC_NOKEY
 */
rpmRC rpmKeyringVerifySig(rpmKeyring keyring, pgpDigParams sig, DIGEST_CTX ctx);

/** \ingroup rpmkeyring
 * Reference a keyring.
 * @param keyring	keyring handle
 * @return		new keyring reference
 */
rpmKeyring rpmKeyringLink(rpmKeyring keyring);

/** \ingroup rpmkeyring
 * Create a new rpmPubkey from OpenPGP packet
 * @param pkt		OpenPGP packet data
 * @param pktlen	Data length
 * @return		new pubkey handle
 */
rpmPubkey rpmPubkeyNew(const uint8_t *pkt, size_t pktlen);

/** \ingroup rpmkeyring
 * Return array of subkeys belonging to mainkey
 * param mainkey	main rpmPubkey
 * param count		count of returned subkeys
 * @return		an array of subkey's handles
 */
rpmPubkey *rpmGetSubkeys(rpmPubkey mainkey, int *count);

/** \ingroup rpmkeyring
 * Create a new rpmPubkey from ASCII-armored pubkey file
 * @param filename	Path to pubkey file
 * @return		new pubkey handle
 */
rpmPubkey rpmPubkeyRead(const char *filename);

/** \ingroup rpmkeyring
 * Free a pubkey.
 * @param key		Pubkey to free
 * @return		NULL always
 */
rpmPubkey rpmPubkeyFree(rpmPubkey key);

/** \ingroup rpmkeyring
 * Reference a pubkey.
 * @param key		Pubkey
 * @return		new pubkey reference
 */
rpmPubkey rpmPubkeyLink(rpmPubkey key);

/** \ingroup rpmkeyring
 * Return base64 encoding of pubkey
 * @param key           Pubkey
 * @return              base64 encoded pubkey (malloced), NULL on error
 */
char * rpmPubkeyBase64(rpmPubkey key);

/** \ingroup rpmkeyring
 * Return pgp params of key
 * @param key		Pubkey
 * @return		pgp params, NULL on error
 */
pgpDigParams rpmPubkeyPgpDigParams(rpmPubkey key);

/** \ingroup rpmkeyring
 * Lookup a pubkey in the keyring
 * @param keyring      keyring handle
 * @param key          Pubkey to find in keyring
 * @return             pubkey handle, NULL if not found
 */
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
int rpmKeyringModify(rpmKeyring keyring, rpmPubkey key, rpmKeyringModifyMode mode);

/** \ingroup rpmkeyring
 * Merge the data of two pubkeys describing the same key
 * @param oldkey       old Pubkey
 * @param newkey       new Pubkey
 * @param mergedkeyp   merged Pubkey, NULL if identical to old Pubkey
 * @return             RPMRC_OK / RPMRC_FAIL
 */
rpmRC rpmPubkeyMerge(rpmPubkey oldkey, rpmPubkey newkey, rpmPubkey *mergedkeyp);

#ifdef __cplusplus
}
#endif
#endif /* _RPMKEYDB_H */
