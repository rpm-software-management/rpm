/** \ingroup rpmdb
 * \file rpmdb/hdrNVR.c
 */

#include "system.h"

#include <rpm/rpmtypes.h>
#include <rpm/header.h>

#include "debug.h"

int headerNVR(Header h, const char **np, const char **vp, const char **rp)
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
    return 0;
}

int headerNEVRA(Header h, const char **np,
		int32_t **ep, const char **vp, const char **rp,
		const char **ap)
{
    rpmTagType type;
    rpm_count_t count;

    headerNVR(h, np, vp, rp);
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
    const char * n, * v, * r;
    char * NVR, * t;

    (void) headerNVR(h, &n, &v, &r);
    NVR = t = xcalloc(1, strlen(n) + strlen(v) + strlen(r) + sizeof("--"));
    t = stpcpy(t, n);
    t = stpcpy(t, "-");
    t = stpcpy(t, v);
    t = stpcpy(t, "-");
    t = stpcpy(t, r);
    if (np)
	*np = n;
    return NVR;
}

char * headerGetNEVRA(Header h, const char ** np)
{
    const char * n, * v, * r, * a;
    char * NVRA, * t;
    int xx;

    (void) headerNVR(h, &n, &v, &r);
    xx = headerGetEntry(h, RPMTAG_ARCH, NULL, (rpm_data_t *) &a, NULL);
    NVRA = t = xcalloc(1, strlen(n) + strlen(v) + strlen(r) + strlen(a) + sizeof("--."));
    t = stpcpy(t, n);
    t = stpcpy(t, "-");
    t = stpcpy(t, v);
    t = stpcpy(t, "-");
    t = stpcpy(t, r);
    t = stpcpy(t, ".");
    t = stpcpy(t, a);
    if (np)
	*np = n;
    return NVRA;
}

rpm_color_t headerGetColor(Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    rpm_color_t hcolor = 0;
    rpm_color_t * fcolors;
    rpm_count_t ncolors;
    int i;

    fcolors = NULL;
    ncolors = 0;
    if (hge(h, RPMTAG_FILECOLORS, NULL, (rpm_data_t *)&fcolors, &ncolors)
     && fcolors != NULL && ncolors > 0)
    {
	for (i = 0; i < ncolors; i++)
	    hcolor |= fcolors[i];
    }
    hcolor &= 0x0f;

    return hcolor;
}

int headerIsSource(Header h)
{
    return (!headerIsEntry(h, RPMTAG_SOURCERPM));
}

