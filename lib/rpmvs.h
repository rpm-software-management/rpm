#ifndef _RPMVS_H
#define _RPMVS_H

#include <rpm/rpmtypes.h>
#include <rpm/rpmts.h> /* for rpmVSFlags */
#include "lib/header_internal.h"

enum {
    RPMSIG_UNKNOWN_TYPE		= 0,
    RPMSIG_DIGEST_TYPE		= (1 << 0),
    RPMSIG_SIGNATURE_TYPE	= (1 << 1),
    RPMSIG_OTHER_TYPE		= (1 << 2),
};

#define RPMSIG_VERIFIABLE_TYPE (RPMSIG_DIGEST_TYPE|RPMSIG_SIGNATURE_TYPE)

/* siginfo range bits */
enum {
    RPMSIG_HEADER	= (1 << 0),
    RPMSIG_PAYLOAD	= (1 << 1),
};

struct rpmsinfo_s {
    /* static data */
    rpmTagVal tag;
    int type;
    int disabler;
    int range;
    /* parsed data */
    int hashalgo;
    int id;
    unsigned int keyid;
    union {
	pgpDigParams sig;
	char *dig;
    };
};

typedef rpmRC (*rpmsinfoCb)(struct rpmsinfo_s *sinfo, rpmRC sigres, const char *result, void *cbdata);

#ifdef __cplusplus
extern "C" {
#endif

RPM_GNUC_INTERNAL
struct rpmvs_s *rpmvsCreate(hdrblob blob, rpmVSFlags vsflags);

RPM_GNUC_INTERNAL
struct rpmvs_s *rpmvsFree(struct rpmvs_s *sis);

RPM_GNUC_INTERNAL
void rpmvsAppend(struct rpmvs_s *sis, hdrblob blob, rpmTagVal tag);

RPM_GNUC_INTERNAL
void rpmvsInitDigests(struct rpmvs_s *sis, int range, rpmDigestBundle bundle);

RPM_GNUC_INTERNAL
int rpmvsVerifyItems(struct rpmvs_s *sis, int range, rpmDigestBundle bundle,
                       rpmKeyring keyring, rpmsinfoCb cb, void *cbdata);

RPM_GNUC_INTERNAL
rpmRC rpmsinfoInit(rpmtd td, const char *origin,
                     struct rpmsinfo_s *sigt, char **msg);

RPM_GNUC_INTERNAL
void rpmsinfoFini(struct rpmsinfo_s *sinfo);

RPM_GNUC_INTERNAL
int rpmsinfoDisabled(const struct rpmsinfo_s *sinfo, rpmVSFlags vsflags);

RPM_GNUC_INTERNAL
rpmRC rpmpkgVerifySignatures(rpmKeyring keyring, rpmVSFlags flags, FD_t fd,
			    rpmsinfoCb cb, void *cbdata);

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
RPM_GNUC_INTERNAL
rpmRC rpmVerifySignature(rpmKeyring keyring, struct rpmsinfo_s *sinfo,
			 DIGEST_CTX ctx, char ** result);

#ifdef __cplusplus
}
#endif

#endif /* _RPMVS_H */
