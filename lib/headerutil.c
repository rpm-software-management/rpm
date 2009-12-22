/** \ingroup rpmdb
 * \file lib/hdrNVR.c
 */

#include "system.h"

#include <rpm/rpmtypes.h>
#include <rpm/header.h>
#include <rpm/rpmstring.h>

#include "debug.h"

static int NEVRA(Header h, const char **np,
		 uint32_t **ep, const char **vp, const char **rp,
		 const char **ap)
{
    if (np) *np = headerGetString(h, RPMTAG_NAME);
    if (vp) *vp = headerGetString(h, RPMTAG_VERSION);
    if (rp) *rp = headerGetString(h, RPMTAG_RELEASE);
    if (ap) *ap = headerGetString(h, RPMTAG_ARCH);
    if (ep) {
	struct rpmtd_s td;
	headerGet(h, RPMTAG_EPOCH, &td, HEADERGET_DEFAULT);
	*ep = rpmtdGetUint32(&td);
    }
    return 0;
}

int headerNVR(Header h, const char **np, const char **vp, const char **rp)
{
    return NEVRA(h, np, NULL, vp, rp, NULL);
}

int headerNEVRA(Header h, const char **np,
		uint32_t **ep, const char **vp, const char **rp,
		const char **ap)
{
    return NEVRA(h, np, ep, vp, rp, ap);
}

static char *getNEVRA(Header h, rpmTag tag, const char **np)
{
    if (np) *np = headerGetString(h, RPMTAG_NAME);
    return headerGetAsString(h, tag);
}

char * headerGetNEVR(Header h, const char ** np)
{
    return getNEVRA(h, RPMTAG_NEVR, np);
}

char * headerGetNEVRA(Header h, const char ** np)
{
    return getNEVRA(h, RPMTAG_NEVRA, np);
}

char * headerGetEVR(Header h, const char ** np)
{
    return getNEVRA(h, RPMTAG_EVR, np);
}

rpm_color_t headerGetColor(Header h)
{
    return headerGetNumber(h, RPMTAG_HEADERCOLOR);
}

int headerIsSource(Header h)
{
    return (!headerIsEntry(h, RPMTAG_SOURCERPM));
}

