#ifndef _RPMVS_H
#define _RPMVS_H

#include <rpm/rpmtypes.h>
#include <rpm/rpmts.h> /* for rpmVSFlags */
#include "lib/header_internal.h"

/* siginfo range bits */
enum {
    RPMSIG_HEADER	= (1 << 0),
    RPMSIG_PAYLOAD	= (1 << 1),
};

struct rpmsinfo_s {
    /* static data */
    int type;
    int disabler;
    int range;
    /* parsed data */
    int hashalgo;
    int sigalgo;
    int id;
    int wrapped;
    int strength;
    unsigned int keyid;
    union {
	pgpDigParams sig;
	char *dig;
    };
    char *descr;
    DIGEST_CTX ctx;
    /* verify results */
    rpmRC rc;
    char *msg;
};

/**
 * Signature/digest verification callback prototype.
 * Useful eg for customizing verification output and results.
 *
 * @param sinfo		signature/digest info
 * @param cbdata	callback data
 * @return		1 to continue, 0 to stop verification
 */
typedef int (*rpmsinfoCb)(struct rpmsinfo_s *sinfo, void *cbdata);

#ifdef __cplusplus
extern "C" {
#endif

RPM_GNUC_INTERNAL
const char *rpmsinfoDescr(struct rpmsinfo_s *sinfo);

RPM_GNUC_INTERNAL
char *rpmsinfoMsg(struct rpmsinfo_s *sinfo);

RPM_GNUC_INTERNAL
struct rpmvs_s *rpmvsCreate(int vfylevel, rpmVSFlags vsflags, rpmKeyring keyring);

RPM_GNUC_INTERNAL
void rpmvsInit(struct rpmvs_s *vs, hdrblob blob, rpmDigestBundle bundle);

RPM_GNUC_INTERNAL
rpmVSFlags rpmvsFlags(struct rpmvs_s *vs);

RPM_GNUC_INTERNAL
struct rpmvs_s *rpmvsFree(struct rpmvs_s *sis);

RPM_GNUC_INTERNAL
void rpmvsAppendTag(struct rpmvs_s *sis, hdrblob blob, rpmTagVal tag);

RPM_GNUC_INTERNAL
void rpmvsInitRange(struct rpmvs_s *sis, int range);

RPM_GNUC_INTERNAL
void rpmvsFiniRange(struct rpmvs_s *sis, int range);

RPM_GNUC_INTERNAL
int rpmvsVerify(struct rpmvs_s *sis, int type,
                       rpmsinfoCb cb, void *cbdata);

RPM_GNUC_INTERNAL
rpmRC rpmpkgRead(struct rpmvs_s *vs, FD_t fd,
		hdrblob *sigblobp, hdrblob *blobp, char **emsg);

#ifdef __cplusplus
}
#endif

#endif /* _RPMVS_H */
