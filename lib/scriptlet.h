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
 * @param psm		package state machine data
 * @return		0 on success
 */
int runInstScript(PSM_t psm)
	/*@modifies psm @*/;

/**
 * Run trigger scripts in the database that are fired by this header.
 * @param psm		package state machine data
 * @return		0 on success, 1 on error
 */
int runTriggers(PSM_t psm)
	/*@modifies psm @*/;

/**
 * Run triggers from this header that are fired by headers in the database.
 * @param psm		package state machine data
 * @return		0 on success, 1 on error
 */
int runImmedTriggers(PSM_t psm)
	/*@modifies psm @*/;

#ifdef __cplusplus
}
#endif

#endif	/* H_SCRIPTLET */
