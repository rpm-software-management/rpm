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
    rpmTagType type;
    rpm_count_t count;

    if (np) {
	if (!(headerGetEntry(h, RPMTAG_NAME, &type, (rpm_data_t *) np, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*np = NULL;
    }
    if (vp) {
	if (!(headerGetEntry(h, RPMTAG_VERSION, &type, (rpm_data_t *) vp, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*vp = NULL;
    }
    if (rp) {
	if (!(headerGetEntry(h, RPMTAG_RELEASE, &type, (rpm_data_t *) rp, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*rp = NULL;
    }
    if (ap) {
	if (!(headerGetEntry(h, RPMTAG_ARCH, &type, (rpm_data_t *) ap, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*ap = NULL;
    }
    if (ep) {
	if (!(headerGetEntry(h, RPMTAG_EPOCH, &type, (rpm_data_t *) ep, &count)
	    && type == RPM_INT32_TYPE && count == 1))
		*ep = NULL;
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

    nevr = headerGetNEVR(h, &n);
    headerGetEntry(h, RPMTAG_ARCH, NULL, (rpm_data_t *) &a, NULL);

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

