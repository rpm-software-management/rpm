/** \ingroup rpmdb
 * \file rpmdb/hdrNVR.c
 */

#include "system.h"
#include "lib/rpmlib.h"
#include "debug.h"

int headerNVR(Header h, const char **np, const char **vp, const char **rp)
{
    int type;
    int count;

    if (np) {
	if (!(headerGetEntry(h, RPMTAG_NAME, &type, (void **) np, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*np = NULL;
    }
    if (vp) {
	if (!(headerGetEntry(h, RPMTAG_VERSION, &type, (void **) vp, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*vp = NULL;
    }
    if (rp) {
	if (!(headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) rp, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*rp = NULL;
    }
    return 0;
}
