#ifndef _RPMFS_H
#define _RPMFS_H

#include <rpm/rpmfi.h>
#include <rpm/rpmte.h>

/** \ingroup rpmfs
 * Transaction element file states.
 */
typedef struct rpmfs_s * rpmfs;
typedef struct sharedFileInfo_s * sharedFileInfo;
typedef char rpm_fstate_t;

/* XXX psm needs access to these */
struct sharedFileInfo_s {
    int pkgFileNum;
    int otherPkg;
    int otherFileNum;
};

#ifdef __cplusplus
extern "C" {
#endif

RPM_GNUC_INTERNAL
rpmfs rpmfsNew(Header h, rpmElementType type);

RPM_GNUC_INTERNAL
rpmfs rpmfsFree(rpmfs fs);

RPM_GNUC_INTERNAL
rpm_count_t rpmfsFC(rpmfs fs);

RPM_GNUC_INTERNAL
void rpmfsAddReplaced(rpmfs fs, int pkgFileNum, int otherPkg, int otherFileNum);

RPM_GNUC_INTERNAL
sharedFileInfo rpmfsGetReplaced(rpmfs fs);

RPM_GNUC_INTERNAL
sharedFileInfo rpmfsNextReplaced(rpmfs fs , sharedFileInfo replaced);

RPM_GNUC_INTERNAL
void rpmfsSetState(rpmfs fs, unsigned int ix, rpmfileState state);

RPM_GNUC_INTERNAL
rpmfileState rpmfsGetState(rpmfs fs, unsigned int ix);

/* May return NULL */
RPM_GNUC_INTERNAL
rpm_fstate_t * rpmfsGetStates(rpmfs fs);

RPM_GNUC_INTERNAL
rpmFileAction rpmfsGetAction(rpmfs fs, unsigned int ix);

/* XXX this should be internal too but build code needs for now */
void rpmfsSetAction(rpmfs fs, unsigned int ix, rpmFileAction action);

#ifdef __cplusplus
}
#endif

#endif /* _RPMFS_H */
