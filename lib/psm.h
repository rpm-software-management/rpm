#ifndef H_PSM
#define H_PSM

/** \ingroup rpmtrans payload
 * \file lib/psm.h
 * Package state machine to handle a package from a transaction set.
 */

#include <rpm/rpmte.h>

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
 * Package state machine driver.
 * @param ts		transaction set
 * @param te		transaction element
 * @param goal		state machine goal
 * @return		0 on success
 */
RPM_GNUC_INTERNAL
rpmRC rpmpsmRun(rpmts ts, rpmte te, pkgGoal goal);

#ifdef __cplusplus
}
#endif

#endif	/* H_PSM */
