#ifndef	_RPMTE_INTERNAL_H
#define _RPMTE_INTERNAL_H

#include <rpm/rpmte.h>
#include <rpm/rpmds.h>
#include "lib/rpmfs.h"

/** \ingroup rpmte
 * Transaction element ordering chain linkage.
 */
typedef struct tsortInfo_s *		tsortInfo;

RPM_GNUC_INTERNAL
rpmfi rpmteSetFI(rpmte te, rpmfi fi);

RPM_GNUC_INTERNAL
FD_t rpmteSetFd(rpmte te, FD_t fd);

RPM_GNUC_INTERNAL
int rpmteOpen(rpmte te, rpmts ts, int reload_fi);

RPM_GNUC_INTERNAL
int rpmteClose(rpmte te, rpmts ts, int reset_fi);

RPM_GNUC_INTERNAL
FD_t rpmtePayload(rpmte te);

RPM_GNUC_INTERNAL
int rpmteMarkFailed(rpmte te, rpmts ts);

RPM_GNUC_INTERNAL
int rpmteHaveTransScript(rpmte te, rpmTag tag);

RPM_GNUC_INTERNAL
void rpmteAddProblem(rpmte te, rpmProblemType type,
                     const char *altNEVR, const char *str, uint64_t number);

RPM_GNUC_INTERNAL
void rpmteAddDepProblem(rpmte te, const char * pkgNEVR, rpmds ds,
		        fnpyKey * suggestedKeys, int adding);

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

#endif	/* _RPMTE_INTERNAL_H */

