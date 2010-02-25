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
} pkgGoal;

typedef enum pkgStage_e {
    PSM_UNKNOWN		=  0,
    PSM_INIT		=  1,
    PSM_PRE		=  2,
    PSM_PROCESS		=  3,
    PSM_POST		=  4,
    PSM_UNDO		=  5,
    PSM_FINI		=  6,

    PSM_PKGCOMMIT	= 10,

    PSM_CREATE		= 17,
    PSM_NOTIFY		= 22,
    PSM_DESTROY		= 23,
    PSM_COMMIT		= 25,

    PSM_CHROOT_IN	= 51,
    PSM_CHROOT_OUT	= 52,
    PSM_SCRIPT		= 53,
    PSM_TRIGGERS	= 54,
    PSM_IMMED_TRIGGERS	= 55,

    PSM_RPMDB_ADD	= 98,
    PSM_RPMDB_REMOVE	= 99

} pkgStage;

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

RPM_GNUC_INTERNAL
rpmRC rpmpsmRun(rpmpsm psm, pkgGoal goal);

/**
 * Package state machine driver.
 * @param psm		package state machine data
 * @param stage		next stage
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
rpmRC rpmpsmStage(rpmpsm psm, pkgStage stage);
#define	rpmpsmUNSAFE	rpmpsmSTAGE

/**
 * Run rpmpsmStage(PSM_SCRIPT) for scriptTag
 * @param psm		package state machine data
 * @param scriptTag	scriptlet tag to execute
 * @return 		0 on success
 */
RPM_GNUC_INTERNAL
rpmRC rpmpsmScriptStage(rpmpsm psm, rpmTag scriptTag);

#ifdef __cplusplus
}
#endif

#endif	/* H_PSM */
