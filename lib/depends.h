#ifndef H_DEPENDS
#define H_DEPENDS

/** \ingroup rpmdep rpmtrans
 * \file lib/depends.h
 * Structures used for dependency checking.
 */

#include <header.h>
#include <rpmhash.h>

#include "rpmds.h"
#include "rpmfi.h"
#include "rpmal.h"

/*@unchecked@*/
/*@-exportlocal@*/
extern int _cacheDependsRC;
/*@=exportlocal@*/

#include "rpmts.h"

/* XXX FIXME: rpmTransactionSet not opaque */
#include "rpmte.h"

#ifdef	DYING
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return (malloc'd) header name-version-release string.
 * @param h		header
 * @retval np		name tag value
 * @return		name-version-release string
 */
/*@only@*/ char * hGetNEVR(Header h, /*@out@*/ const char ** np )
	/*@modifies *np @*/;

#ifdef __cplusplus
}
#endif
#endif

#endif	/* H_DEPENDS */
