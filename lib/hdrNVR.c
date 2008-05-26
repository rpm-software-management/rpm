/** \ingroup rpmdb
 * \file lib/hdrNVR.c
 */

#include "system.h"

#include <rpm/rpmtypes.h>
#include <rpm/header.h>
#include <rpm/rpmstring.h>

#include "debug.h"

int headerNVR(Header h, const char **np, const char **vp, const char **rp)
{
    return headerNEVRA(h, np, NULL, vp, rp, NULL);
}

int headerNEVRA(Header h, const char **np,
		uint32_t **ep, const char **vp, const char **rp,
		const char **ap)
{
    struct rpmtd_s td;

    if (np) {
	headerGet(h, RPMTAG_NAME, &td, HEADERGET_DEFAULT);
	*np = rpmtdGetString(&td);
    }
    if (vp) {
	headerGet(h, RPMTAG_VERSION, &td, HEADERGET_DEFAULT);
	*vp = rpmtdGetString(&td);
    }
    if (rp) {
	headerGet(h, RPMTAG_RELEASE, &td, HEADERGET_DEFAULT);
	*rp = rpmtdGetString(&td);
    }
    if (ap) {
	headerGet(h, RPMTAG_ARCH, &td, HEADERGET_DEFAULT);
	*ap = rpmtdGetString(&td);
    }
    if (ep) {
	headerGet(h, RPMTAG_EPOCH, &td, HEADERGET_DEFAULT);
	*ep = rpmtdGetUint32(&td);
    }
    return 0;
}

char * headerGetNEVR(Header h, const char ** np)
{
    const char * n;
    char * evr, *nevr = NULL;

    evr = headerGetEVR(h, &n);
    rasprintf(&nevr, "%s-%s", n, evr);
    free(evr);
    if (np)
	*np = n;
    return nevr;
}

char * headerGetNEVRA(Header h, const char ** np)
{
    const char *n = NULL, *a = NULL;
    char *nevr, *nevra = NULL;
    struct rpmtd_s arch;

    nevr = headerGetNEVR(h, &n);
    if (headerGet(h, RPMTAG_ARCH, &arch, HEADERGET_DEFAULT)) {
	a = rpmtdGetString(&arch);
    }

    /* XXX gpg-pubkey packages have no arch, urgh... */
    if (a) {
	const char *arch = headerIsSource(h) ? "src" : a;
    	rasprintf(&nevra, "%s.%s", nevr, arch);
	free(nevr);
    } else {
	nevra = nevr;
    }

    if (np)
	*np = n;
    return nevra;
}

char * headerGetEVR(Header h, const char ** np)
{
    const char *n, *v, *r;
    char *evr = NULL;
    uint32_t *e;

    (void) headerNEVRA(h, &n, &e, &v, &r, NULL);
    if (e) {
	rasprintf(&evr, "%d:%s-%s", *e, v, r);
    } else {
	rasprintf(&evr, "%s-%s", v, r);
    }
    if (np) 
	*np = n;
    return evr;
}

rpm_color_t headerGetColor(Header h)
{
    rpm_color_t hcolor = 0, *fcolor;
    struct rpmtd_s fcolors;

    headerGet(h, RPMTAG_FILECOLORS, &fcolors, HEADERGET_MINMEM);
    while ((fcolor = rpmtdNextUint32(&fcolors)) != NULL) {
	hcolor |= *fcolor;
    }
    hcolor &= 0x0f;

    return hcolor;
}

int headerIsSource(Header h)
{
    return (!headerIsEntry(h, RPMTAG_SOURCERPM));
}

