#ifndef H_SCRIPTLET
#define H_SCRIPTLET

/** \file lib/scriptlet.h
 */

#include <rpmlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Retrieve and run scriptlet from header.
 * @param ts		transaction set
 * @param h		header
 * @param scriptTag	scriptlet tag
 * @param progTag	scriptlet interpreter tag
 * @param arg		no. instances of package installed after scriptlet exec
 * @param norunScripts	should scriptlet be executed?
 * @return		0 on success
 */
int runInstScript(const rpmTransactionSet ts, Header h,
		int scriptTag, int progTag, int arg, int norunScripts);

/**
 * Run trigger scripts in the database that are fired by this header.
 * @param psm		package state machine data
 * @param sense		one of RPMSENSE_TRIGGER{IN,UN,POSTUN}
 * @param countCorrection 0 if installing, -1 if removing, package
 * @return		0 on success, 1 on error
 */
int runTriggers(PSM_t psm, int sense, int countCorrection);

/**
 * Run triggers from this header that are fired by headers in the database.
 * @param psm		package state machine data
 * @param sense		one of RPMSENSE_TRIGGER{IN,UN,POSTUN}
 * @param countCorrection 0 if installing, -1 if removing, package
 * @return		0 on success, 1 on error
 */
int runImmedTriggers(PSM_t psm, int sense, int countCorrection);

#ifdef __cplusplus
}
#endif

#endif	/* H_SCRIPTLET */
