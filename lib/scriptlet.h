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
 * @param ts		transaction set
 * @param sense		one of RPMSENSE_TRIGGER{IN,UN,POSTUN}
 * @param h		header
 * @param countCorrection 0 if installing, -1 if removing, package
 * @return		0 on success, 1 on error
 */
int runTriggers(const rpmTransactionSet ts, int sense, Header h,
		int countCorrection);

/**
 * Run triggers from this header that are fired by headers in the database.
 * @param ts		transaction set
 * @param sense		one of RPMSENSE_TRIGGER{IN,UN,POSTUN}
 * @param h		header
 * @param countCorrection 0 if installing, -1 if removing, package
 * @return		0 on success, 1 on error
 */
int runImmedTriggers(const rpmTransactionSet ts, int sense, Header h,
		int countCorrection);

#ifdef __cplusplus
}
#endif

#endif	/* H_SCRIPTLET */
