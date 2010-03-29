#ifndef	_RPMTE_INTERNAL_H
#define _RPMTE_INTERNAL_H

#include <rpm/rpmte.h>
#include <rpm/rpmds.h>
#include "lib/rpmfs.h"

typedef enum pkgGoal_e {
    PKG_NONE		= 0,
    /* permit using rpmteType() for install + erase goals */
    PKG_INSTALL		= TR_ADDED,
    PKG_ERASE		= TR_REMOVED,
    /* permit using scriptname for these for now... */
    PKG_VERIFY		= RPMTAG_VERIFYSCRIPT,
    PKG_PRETRANS	= RPMTAG_PRETRANS,
    PKG_POSTTRANS	= RPMTAG_POSTTRANS,
} pkgGoal;

/** \ingroup rpmte
 * Transaction element ordering chain linkage.
 */
typedef struct tsortInfo_s *		tsortInfo;

RPM_GNUC_INTERNAL
rpmfi rpmteSetFI(rpmte te, rpmfi fi);

RPM_GNUC_INTERNAL
FD_t rpmteSetFd(rpmte te, FD_t fd);

RPM_GNUC_INTERNAL
FD_t rpmtePayload(rpmte te);

RPM_GNUC_INTERNAL
int rpmteProcess(rpmte te, rpmts ts, pkgGoal goal);

RPM_GNUC_INTERNAL
void rpmteAddProblem(rpmte te, rpmProblemType type,
                     const char *altNEVR, const char *str, uint64_t number);

RPM_GNUC_INTERNAL
void rpmteAddDepProblem(rpmte te, const char * pkgNEVR, rpmds ds,
		        fnpyKey * suggestedKeys);

RPM_GNUC_INTERNAL
const char * rpmteTypeString(rpmte te);

RPM_GNUC_INTERNAL
tsortInfo rpmteTSI(rpmte te);

RPM_GNUC_INTERNAL
void rpmteSetTSI(rpmte te, tsortInfo tsi);

/* XXX should be internal too but build code needs for now... */
rpmfs rpmteGetFileStates(rpmte te);

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

/** \ingroup rpmte
 * Retrieve size in bytes of package header.
 * @param te		transaction element
 * @return		size in bytes of package file.
 */
RPM_GNUC_INTERNAL
unsigned int rpmteHeaderSize(rpmte te);

/**
 * Package state machine driver.
 * @param ts		transaction set
 * @param te		transaction element
 * @param goal		state machine goal
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
rpmRC rpmpsmRun(rpmts ts, rpmte te, pkgGoal goal);

#endif	/* _RPMTE_INTERNAL_H */

