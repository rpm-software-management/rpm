#ifndef	_RPMTE_INTERNAL_H
#define _RPMTE_INTERNAL_H

#include <rpm/rpmte.h>
#include <rpm/rpmds.h>

/** \ingroup rpmte
 * Dependncy ordering information.
 */

struct relation_s {
    rpmte   rel_suc;  // pkg requiring this package
    rpmsenseFlags rel_flags; // accumulated flags of the requirements
    struct relation_s * rel_next;
};

typedef struct relation_s * relation;

struct tsortInfo_s {
    int	     tsi_count;     // #pkgs this pkg requires
    int	     tsi_qcnt;      // #pkgs requiring this package
    int	     tsi_reqx;       // requires Idx/mark as (queued/loop)
    struct relation_s * tsi_relations;
    struct relation_s * tsi_forward_relations;
    rpmte    tsi_suc;        // used for queuing (addQ)
    int      tsi_SccIdx;     // # of the SCC the node belongs to
                             // (1 for trivial SCCs)
    int      tsi_SccLowlink; // used for SCC detection
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

typedef char rpm_fstate_t;

struct rpmfs_s {
    unsigned int fc;

    rpm_fstate_t * states;
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
int rpmteClose(rpmte te, rpmts ts, int reset_fi);

RPM_GNUC_INTERNAL
int rpmteMarkFailed(rpmte te, rpmts ts);

RPM_GNUC_INTERNAL
int rpmteHaveTransScript(rpmte te, rpmTag tag);

RPM_GNUC_INTERNAL
rpmps rpmteProblems(rpmte te);

//RPM_GNUC_INTERNAL
rpmfs rpmteGetFileStates(rpmte te);

RPM_GNUC_INTERNAL
rpmfs rpmfsNew(unsigned int fc, rpmElementType type);

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
rpm_fstate_t * rpmfsGetStates(rpmfs fs);

RPM_GNUC_INTERNAL
rpmFileAction rpmfsGetAction(rpmfs fs, unsigned int ix);

//RPM_GNUC_INTERNAL
void rpmfsSetAction(rpmfs fs, unsigned int ix, rpmFileAction action);

/* XXX here for now... */
/**
 * Relocate files in header.
 * @todo multilib file dispositions need to be checked.
 * @param relocations		relocations
 * @param numRelocations	number of relocations
 * @param fs			file state set
 * @param h			package header to relocate
 */
RPM_GNUC_INTERNAL
void rpmRelocateFileList(rpmRelocation *relocs, int numRelocations, rpmfs fs, Header h);

#endif	/* _RPMTE_INTERNAL_H */

