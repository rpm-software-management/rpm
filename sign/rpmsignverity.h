#ifndef H_RPMSIGNVERITY
#define H_RPMSIGNVERITY

#include <rpm/rpmtypes.h>
#include <rpm/rpmutil.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Block size used to generate the Merkle tree for fsverity. For now
 * we only support 4K blocks, if we ever decide to support different
 * block sizes, we will need a tag to indicate this.
 */
#define RPM_FSVERITY_BLKSZ	4096

/**
 * Sign file digests in header into signature header
 * @param fd		file descriptor of RPM
 * @param sigh		package signature header
 * @param h		package header
 * @param key		signing key
 * @param keypass	signing key password
 * @param pkcs11_engine		PKCS#11 engine to use PKCS#11 token support for signing key
 * @param pkcs11_module		PKCS#11 module to use PKCS#11 token support for signing key
 * @param pkcs11_keyid		PKCS#11 key identifier
 * @param cert		signing cert
 * @return		RPMRC_OK on success
 */
rpmRC rpmSignVerity(FD_t fd, Header sigh, Header h, char *key, char *keypass,
		    char *pkcs11_engine, char *pkcs11_module, char *pkcs11_keyid,
		    char *cert, uint16_t algo);

#ifdef _cplusplus
}
#endif

#endif /* H_RPMSIGNVERITY */
