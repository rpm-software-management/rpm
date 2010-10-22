#ifndef _RPMSIGN_H
#define _RPMSIGN_H

#include <rpm/argv.h>
#include <rpm/rpmpgp.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rpmSignArgs {
    char *keyid;
    pgpHashAlgo hashalgo;
    /* ... what else? */
};

/** \ingroup rpmsign
 * Sign a package
 * @param path		path to package
 * @param args		signing parameters (or NULL for defaults)
 * @param passPhrase	passphrase for the signing key
 * @return		0 on success
 */
int rpmPkgSign(const char *path,
	       const struct rpmSignArgs * args, const char *passPhrase);

/** \ingroup rpmsign
 * Delete signature(s) from a package
 * @param path		path to package
 * @return		0 on success
 */
int rpmPkgDelSign(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* _RPMSIGN_H */
