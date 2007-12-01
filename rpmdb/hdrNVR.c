/** \ingroup rpmdb
 * \file rpmdb/hdrNVR.c
 */

#include "system.h"
#include <rpmlib.h>
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

int headerNEVRA(Header h, const char **np,
		const char **ep, const char **vp, const char **rp,
		const char **ap)
{
    int type;
    int count;

    headerNVR(h, np, vp, rp);
    if (ap) {
	if (!(headerGetEntry(h, RPMTAG_ARCH, &type, (void **) ap, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*ap = NULL;
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
    xx = headerGetEntry(h, RPMTAG_ARCH, NULL, (void **) &a, NULL);
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

uint32_t headerGetColor(Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    uint32_t hcolor = 0;
    uint32_t * fcolors;
    int32_t ncolors;
    int i;

    fcolors = NULL;
    ncolors = 0;
    if (hge(h, RPMTAG_FILECOLORS, NULL, (void **)&fcolors, &ncolors)
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

