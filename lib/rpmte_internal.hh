#ifndef	_RPMTE_INTERNAL_H
#define _RPMTE_INTERNAL_H

#include <rpm/rpmte.h>
#include <rpm/rpmds.h>
#include <rpm/rpmtag.h>
#include "rpmfs.hh"

typedef enum pkgGoal_e {
    PKG_NONE		= 0,
    /* permit using rpmteType() for install + erase goals */
    PKG_INSTALL		= TR_ADDED,
    PKG_ERASE		= TR_REMOVED,
    PKG_RESTORE		= TR_RESTORED,
    /* permit using scriptname for these for now... */
    PKG_VERIFY		= RPMTAG_VERIFYSCRIPT,
    PKG_PRETRANS	= RPMTAG_PRETRANS,
    PKG_POSTTRANS	= RPMTAG_POSTTRANS,
    PKG_PREUNTRANS	= RPMTAG_PREUNTRANS,
    PKG_POSTUNTRANS	= RPMTAG_POSTUNTRANS,
    PKG_TRANSFILETRIGGERIN	= RPMTAG_TRANSFILETRIGGERIN,
    PKG_TRANSFILETRIGGERUN	= RPMTAG_TRANSFILETRIGGERUN,
} pkgGoal;

#define isScriptStage(_g) ((_g) != PKG_INSTALL && (_g) != PKG_ERASE && (_g) != PKG_RESTORE)

/** \ingroup rpmte
 * Transaction element ordering chain linkage.
 */
typedef struct tsortInfo_s *		tsortInfo;

enum addOp_e {
  RPMTE_INSTALL       = 0,
  RPMTE_UPGRADE       = 1,
  RPMTE_REINSTALL     = 2,
  RPMTE_RESTORE       = 3,
};

/** \ingroup rpmte
 * Create a transaction element.
 * @param ts		transaction set
 * @param h		header
 * @param type		TR_ADDED/TR_REMOVED/TR_RPMDB
 * @param key		(TR_ADDED) package retrieval key (e.g. file name)
 * @param relocs	(TR_ADDED) package file relocations
 * @param addop         (TR_ADDED) RPMTE_INSTALL/UPGRADE/REINSTALL
 * @return		new transaction element
 */
rpmte rpmteNew(rpmts ts, Header h, rpmElementType type, fnpyKey key,
	       rpmRelocation * relocs, int addop);

/** \ingroup rpmte
 * Destroy a transaction element.
 * @param te		transaction element
 * @return		NULL always
 */
rpmte rpmteFree(rpmte te);

void rpmteCleanFiles(rpmte te);

FD_t rpmteSetFd(rpmte te, FD_t fd);

FD_t rpmtePayload(rpmte te);

int rpmteProcess(rpmte te, pkgGoal goal, int num);

void rpmteAddProblem(rpmte te, rpmProblemType type,
                     const char *altNEVR, const char *str, uint64_t number);

void rpmteAddDepProblem(rpmte te, const char * pkgNEVR, rpmds ds,
		        fnpyKey * suggestedKeys);

void rpmteAddRelocProblems(rpmte te);

const char * rpmteTypeString(rpmte te);

tsortInfo rpmteTSI(rpmte te);

void rpmteSetTSI(rpmte te, tsortInfo tsi);

int rpmteHaveTransScript(rpmte te, rpmTagVal tag);

/* XXX should be internal too but build code needs for now... */
rpmfs rpmteGetFileStates(rpmte te);

void rpmteSetVerified(rpmte te, int verified);

Header rpmteHeaderAux(rpmte te, int init);

/** \ingroup rpmte
 * Retrieve size in bytes of package header.
 * @param te		transaction element
 * @return		size in bytes of package file.
 */
unsigned int rpmteHeaderSize(rpmte te);

/**
 * Package state machine driver.
 * @param ts		transaction set
 * @param te		transaction element
 * @param goal		state machine goal
 * @return		0 on success
 */
rpmRC rpmpsmRun(rpmts ts, rpmte te, pkgGoal goal);

int rpmteAddOp(rpmte te);

#endif	/* _RPMTE_INTERNAL_H */

