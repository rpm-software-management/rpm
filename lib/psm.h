#ifndef H_PSM
#define H_PSM

/** \ingroup rpmtrans payload
 * \file lib/psm.h
 * Package state machine to handle a package from a transaction set.
 */

#include <rpm/rpmte.h>

extern int _psm_debug;

typedef struct rpmpsm_s * rpmpsm;

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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unreference a package state machine instance.
 * @param psm		package state machine
 * @param msg
 * @return		NULL always
 */
RPM_GNUC_INTERNAL
rpmpsm rpmpsmUnlink (rpmpsm psm,
		const char * msg);

/**
 * Reference a package state machine instance.
 * @param psm		package state machine
 * @param msg
 * @return		new package state machine reference
 */
RPM_GNUC_INTERNAL
rpmpsm rpmpsmLink (rpmpsm psm, const char * msg);

/**
 * Destroy a package state machine.
 * @param psm		package state machine
 * @return		NULL always
 */
RPM_GNUC_INTERNAL
rpmpsm rpmpsmFree(rpmpsm psm);

/**
 * Create and load a package state machine.
 * @param ts		transaction set
 * @param te		transaction set element
 * @return		new package state machine
 */
RPM_GNUC_INTERNAL
rpmpsm rpmpsmNew(rpmts ts, rpmte te);

/**
 * Package state machine driver.
 * @param psm		package state machine data
 * @param goal		state machine goal
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
rpmRC rpmpsmRun(rpmpsm psm, pkgGoal goal);

#ifdef __cplusplus
}
#endif

#endif	/* H_PSM */
