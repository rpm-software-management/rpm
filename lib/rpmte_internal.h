#ifndef	_RPMTE_INTERNAL_H
#define _RPMTE_INTERNAL_H

#include <rpm/rpmte.h>

/** \ingroup rpmte
 * Dependncy ordering information.
 */
struct tsortInfo_s {
    union {
	int	count;
	rpmte	suc;
    } tsi_u;
#define	tsi_count	tsi_u.count
#define	tsi_suc		tsi_u.suc
    struct tsortInfo_s * tsi_next;
    rpmte tsi_chain;
    int		tsi_reqx;
    int		tsi_qcnt;
};

/**
 */
typedef struct sharedFileInfo_s *		sharedFileInfo;

/** \ingroup rpmte
 * Transaction element file states.
 */
typedef struct rpmfs_s *		rpmfs;

/**
 */
struct sharedFileInfo_s {
    int pkgFileNum;
    int otherPkg;
    int otherFileNum;
};

struct rpmfs_s {
    unsigned int fc;

    rpmfileState * states;
    rpmFileAction * actions;	/*!< File disposition(s). */

    sharedFileInfo replaced;	/*!< (TR_ADDED) to be replaced files in the rpmdb */
    int numReplaced;
    int allocatedReplaced;
};

/**
 * Iterator across transaction elements, forward on install, backward on erase.
 */
struct rpmtsi_s {
    rpmts ts;		/*!< transaction set. */
    int reverse;	/*!< reversed traversal? */
    int ocsave;		/*!< last returned iterator index. */
    int oc;		/*!< iterator index. */
};

RPM_GNUC_INTERNAL
rpmfi rpmteSetFI(rpmte te, rpmfi fi);

RPM_GNUC_INTERNAL
FD_t rpmteSetFd(rpmte te, FD_t fd);

RPM_GNUC_INTERNAL
int rpmteOpen(rpmte te, rpmts ts, int reload_fi);

RPM_GNUC_INTERNAL
int rpmteClose(rpmte te, rpmts ts);

RPM_GNUC_INTERNAL
int rpmteMarkFailed(rpmte te, rpmts ts);

RPM_GNUC_INTERNAL
int rpmteHaveTransScript(rpmte te, rpmTag tag);

//RPM_GNUC_INTERNAL
rpmfs rpmteGetFileStates(rpmte te);

RPM_GNUC_INTERNAL
rpmfs rpmfsNew(unsigned int fc);

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

/*
 * May return NULL
 */
RPM_GNUC_INTERNAL
rpmfileState * rpmfsGetStates(rpmfs fs);

RPM_GNUC_INTERNAL
rpmFileAction rpmfsGetAction(rpmfs fs, unsigned int ix);

//RPM_GNUC_INTERNAL
void rpmfsSetAction(rpmfs fs, unsigned int ix, rpmFileAction action);

RPM_GNUC_INTERNAL
/* XXX here for now... */
void rpmRelocateFileList(rpmte p, rpmRelocation *relocs, int numRelocations, Header h);

#endif	/* _RPMTE_INTERNAL_H */

