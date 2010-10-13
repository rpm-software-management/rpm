#ifndef _RPMSIGN_H
#define _RPMSIGN_H

#include <rpm/argv.h>
#include <rpm/rpmpgp.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmsign
 * Create/delete package signatures.
 * @param argv		array of package path arguments (NULL terminated)
 * @param deleting	adding or deleting signature(s)
 * @param passPhrase	passphrase (ignored when deleting)
 * @return		0 on success
 */
int rpmcliSign(ARGV_const_t argv, int deleting, const char *passPhrase);

struct rpmSignArgs {
    char *keyid;
    pgpHashAlgo hashalgo;
    /* ... what else? */
};

/** \ingroup rpmsign
 * Sign a package
 * @param path		path to package
 * @param args		signing parameters (or NULL for defaults)
 * @param passphrase	passphrase for the signing key
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
