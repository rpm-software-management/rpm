#ifndef H_RPMSIGNVERITY
#define H_RPMSIGNVERITY

#include <rpm/rpmtypes.h>
#include <rpm/rpmutil.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sign file digests in header into signature header
 * @param fd		file descriptor of RPM
 * @param sigh		package signature header
 * @param h		package header
 * @param key		signing key
 * @param keypass	signing key password
 * @param cert		signing cert
 * @return		RPMRC_OK on success
 */
RPM_GNUC_INTERNAL
rpmRC rpmSignVerity(FD_t fd, Header sigh, Header h, char *key,
		    char *keypass, char *cert);

#ifdef _cplusplus
}
#endif

#endif /* H_RPMSIGNVERITY */
