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

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup rpmte
 * Create a transaction element.
 * @param ts		transaction set
 * @param h		header
 * @param type		TR_ADDED/TR_REMOVED
 * @param key		(TR_ADDED) package retrieval key (e.g. file name)
 * @param relocs	(TR_ADDED) package file relocations
 * @return		new transaction element
 */
RPM_GNUC_INTERNAL
rpmte rpmteNew(rpmts ts, Header h, rpmElementType type, fnpyKey key,
	       rpmRelocation * relocs);

/** \ingroup rpmte
 * Destroy a transaction element.
 * @param te		transaction element
 * @return		NULL always
 */
RPM_GNUC_INTERNAL
rpmte rpmteFree(rpmte te);

RPM_GNUC_INTERNAL
rpmfi rpmteSetFI(rpmte te, rpmfi fi);

RPM_GNUC_INTERNAL
FD_t rpmteSetFd(rpmte te, FD_t fd);

RPM_GNUC_INTERNAL
FD_t rpmtePayload(rpmte te);

RPM_GNUC_INTERNAL
int rpmteProcess(rpmte te, pkgGoal goal);

RPM_GNUC_INTERNAL
void rpmteAddProblem(rpmte te, rpmProblemType type,
                     const char *altNEVR, const char *str, uint64_t number);

RPM_GNUC_INTERNAL
void rpmteAddDepProblem(rpmte te, const char * pkgNEVR, rpmds ds,
		        fnpyKey * suggestedKeys);

RPM_GNUC_INTERNAL
void rpmteAddRelocProblems(rpmte te);

RPM_GNUC_INTERNAL
const char * rpmteTypeString(rpmte te);

RPM_GNUC_INTERNAL
tsortInfo rpmteTSI(rpmte te);

RPM_GNUC_INTERNAL
void rpmteSetTSI(rpmte te, tsortInfo tsi);

RPM_GNUC_INTERNAL
int rpmteHaveTransScript(rpmte te, rpmTagVal tag);

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

/** \ingroup rpmte
 * Add a collection to the list of last collections for the installation
 * section of a transaction element
 * @param te		transaction element
 * @param collname		collection name
 * @return		0 on success, non-zero on error
 */
RPM_GNUC_INTERNAL
int rpmteAddToLastInCollectionAdd(rpmte te, const char * collname);

/** \ingroup rpmte
 * Add a collection to the list of last collections for the installation
 * or removal section of a transaction element
 * @param te		transaction element
 * @param collname		collection name
 * @return		0 on success, non-zero on error
 */
RPM_GNUC_INTERNAL
int rpmteAddToLastInCollectionAny(rpmte te, const char * collname);

/** \ingroup rpmte
 * Add a collection to the list of first collections for the removal
 * section of a transaction element
 * @param te		transaction element
 * @param collname		collection name
 * @return		0 on success, non-zero on error
 */
RPM_GNUC_INTERNAL
int rpmteAddToFirstInCollectionRemove(rpmte te, const char * collname);

/** \ingroup rpmte
 * Sends the open te plugin hook for each plugins with the transaction element open
 * @param te		transaction element
 * @return		0 on success, non-zero on error
 */
RPM_GNUC_INTERNAL
rpmRC rpmteSetupCollectionPlugins(rpmte te);

#ifdef __cplusplus
}
#endif

#endif	/* _RPMTE_INTERNAL_H */

