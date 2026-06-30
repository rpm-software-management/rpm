#ifndef _RPMFS_H
#define _RPMFS_H

#include <rpm/rpmfi.h>

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
    char rstate;
};

rpmfs rpmfsNew(rpm_count_t fc, int initState);

rpmfs rpmfsFree(rpmfs fs);

rpm_count_t rpmfsFC(rpmfs fs);

void rpmfsAddReplaced(rpmfs fs, int pkgFileNum, char rstate,
			int otherPkg, int otherFileNum);

sharedFileInfo rpmfsGetReplaced(rpmfs fs);

sharedFileInfo rpmfsNextReplaced(rpmfs fs , sharedFileInfo replaced);

void rpmfsSetState(rpmfs fs, unsigned int ix, rpmfileState state);

/* May return NULL */
rpm_fstate_t * rpmfsGetStates(rpmfs fs);

rpmFileAction rpmfsGetAction(rpmfs fs, unsigned int ix);

void rpmfsSetAction(rpmfs fs, unsigned int ix, rpmFileAction action);

void rpmfsResetActions(rpmfs fs);

#endif /* _RPMFS_H */
