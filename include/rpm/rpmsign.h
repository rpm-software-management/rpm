#ifndef _RPMSIGN_H
#define _RPMSIGN_H

/** \file rpmsign.h
 *
 * Signature API
 */

#include <rpm/argv.h>
#include <rpm/rpmpgp.h>

#ifdef __cplusplus
extern "C" {
#endif

enum rpmSignFlags_e {
    RPMSIGN_FLAG_NONE		= 0,
    RPMSIGN_FLAG_IMA		= (1 << 0),
    RPMSIGN_FLAG_RPMV3		= (1 << 1),
    RPMSIGN_FLAG_FSVERITY	= (1 << 2),
    RPMSIGN_FLAG_RESIGN		= (1 << 3),
    RPMSIGN_FLAG_RPMV4		= (1 << 4),
    RPMSIGN_FLAG_RPMV6		= (1 << 5),
};
typedef rpmFlags rpmSignFlags;

struct rpmSignArgs {
    char *keyid;		/*!< signer keyid */
    pgpHashAlgo hashalgo;	/*!< hash algorithm to use */
    rpmSignFlags signflags;	/*!< flags to control operation */
    /* ... what else? */
};

/** \ingroup rpmsign
 * Sign a package
 * @param path		path to package
 * @param args		signing parameters (or NULL for defaults)
 * @return		0 on success
 */
int rpmPkgSign(const char *path, const struct rpmSignArgs * args);

/** \ingroup rpmsign
 * Delete signature(s) from a package
 * @param path		path to package
 * @param args		signing parameters (or NULL for defaults)
 * @return		0 on success
 */
int rpmPkgDelSign(const char *path, const struct rpmSignArgs * args);


/** \ingroup rpmsign
 * Delete file signature(s) from a package
 * @param path		path to package
 * @param args		signing parameters (or NULL for defaults)
 * @return		0 on success
 */
int rpmPkgDelFileSign(const char *path, const struct rpmSignArgs * args);

#ifdef __cplusplus
}
#endif

#endif /* _RPMSIGN_H */
