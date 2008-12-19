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

/** \ingroup rpmte
 * A single package instance to be installed/removed atomically.
 */
struct rpmte_s {
    rpmElementType type;	/*!< Package disposition (installed/removed). */

    Header h;			/*!< Package header. */
    char * NEVR;		/*!< Package name-version-release. */
    char * NEVRA;		/*!< Package name-version-release.arch. */
    char * name;		/*!< Name: */
    char * epoch;
    char * version;		/*!< Version: */
    char * release;		/*!< Release: */
    char * arch;		/*!< Architecture hint. */
    char * os;			/*!< Operating system hint. */
    int archScore;		/*!< (TR_ADDED) Arch score. */
    int osScore;		/*!< (TR_ADDED) Os score. */
    int isSource;		/*!< (TR_ADDED) source rpm? */

    rpmte parent;		/*!< Parent transaction element. */
    int degree;			/*!< No. of immediate children. */
    int npreds;			/*!< No. of predecessors. */
    int tree;			/*!< Tree index. */
    int depth;			/*!< Depth in dependency tree. */
    int breadth;		/*!< Breadth in dependency tree. */
    unsigned int db_instance;	/*!< Database instance (of removed pkgs) */
    tsortInfo tsi;		/*!< Dependency ordering chains. */

    rpmds this;			/*!< This package's provided NEVR. */
    rpmds provides;		/*!< Provides: dependencies. */
    rpmds requires;		/*!< Requires: dependencies. */
    rpmds conflicts;		/*!< Conflicts: dependencies. */
    rpmds obsoletes;		/*!< Obsoletes: dependencies. */
    rpmfi fi;			/*!< File information. */

    rpm_color_t color;		/*!< Color bit(s) from package dependencies. */
    rpm_loff_t pkgFileSize;	/*!< No. of bytes in package file (approx). */

    fnpyKey key;		/*!< (TR_ADDED) Retrieval key. */
    rpmRelocation * relocs;	/*!< (TR_ADDED) Payload file relocations. */
    int nrelocs;		/*!< (TR_ADDED) No. of relocations. */
    FD_t fd;			/*!< (TR_ADDED) Payload file descriptor. */

#define RPMTE_HAVE_PRETRANS	(1 << 0)
#define RPMTE_HAVE_POSTTRANS	(1 << 1)
    int transscripts;		/*!< pre/posttrans script existence */
    int failed;			/*!< (parent) install/erase failed */

    rpmalKey pkgKey;

    rpmfs fs;
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
int rpmteOpen(rpmte te, rpmts ts);

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


#endif	/* _RPMTE_INTERNAL_H */

