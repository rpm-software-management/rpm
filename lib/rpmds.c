/** \ingroup rpmdep
 * \file lib/rpmds.c
 */
#include "system.h"
#include <rpmlib.h>

#include "rpmds.h"

#include "debug.h"

/*@access rpmDependencyConflict @*/
/*@access problemsSet @*/

/*@access rpmDepSet @*/

/*@unchecked@*/
static int _ds_debug = 0;

rpmDepSet dsFree(rpmDepSet ds)
{
    HFD_t hfd = headerFreeData;
    rpmTag tagEVR, tagF;

/*@-modfilesystem@*/
if (_ds_debug)
fprintf(stderr, "*** ds %p --\n", ds);
/*@=modfilesystem@*/

    if (ds == NULL)
	return ds;

    if (ds->tagN == RPMTAG_PROVIDENAME) {
	tagEVR = RPMTAG_PROVIDEVERSION;
	tagF = RPMTAG_PROVIDEFLAGS;
    } else
    if (ds->tagN == RPMTAG_REQUIRENAME) {
	tagEVR = RPMTAG_REQUIREVERSION;
	tagF = RPMTAG_REQUIREFLAGS;
    } else
    if (ds->tagN == RPMTAG_CONFLICTNAME) {
	tagEVR = RPMTAG_CONFLICTVERSION;
	tagF = RPMTAG_CONFLICTFLAGS;
    } else
    if (ds->tagN == RPMTAG_OBSOLETENAME) {
	tagEVR = RPMTAG_OBSOLETEVERSION;
	tagF = RPMTAG_OBSOLETEFLAGS;
    } else {
	return NULL;
    }

    /*@-branchstate@*/
    if (ds->Count > 0) {
	ds->N = hfd(ds->N, ds->Nt);
	ds->EVR = hfd(ds->EVR, ds->EVRt);
	/*@-evalorder@*/
	ds->Flags = (ds->h != NULL ? hfd(ds->Flags, ds->Ft) : _free(ds->Flags));
	/*@=evalorder@*/
	ds->h = headerFree(ds->h, "dsFree");
    }
    /*@=branchstate@*/

    ds->DNEVR = _free(ds->DNEVR);

    memset(ds, 0, sizeof(*ds));		/* XXX trash and burn */
    ds = _free(ds);
    return NULL;
}

rpmDepSet dsNew(Header h, rpmTag tagN, int scareMem)
{
    HGE_t hge =
	(scareMem ? (HGE_t) headerGetEntryMinMemory : (HGE_t) headerGetEntry);
    rpmTag tagEVR, tagF;
    rpmDepSet ds = NULL;
    const char * Type;

    if (tagN == RPMTAG_PROVIDENAME) {
	Type = "Provides";
	tagEVR = RPMTAG_PROVIDEVERSION;
	tagF = RPMTAG_PROVIDEFLAGS;
    } else
    if (tagN == RPMTAG_REQUIRENAME) {
	Type = "Requires";
	tagEVR = RPMTAG_REQUIREVERSION;
	tagF = RPMTAG_REQUIREFLAGS;
    } else
    if (tagN == RPMTAG_CONFLICTNAME) {
	Type = "Conflicts";
	tagEVR = RPMTAG_CONFLICTVERSION;
	tagF = RPMTAG_CONFLICTFLAGS;
    } else
    if (tagN == RPMTAG_OBSOLETENAME) {
	Type = "Obsoletes";
	tagEVR = RPMTAG_OBSOLETEVERSION;
	tagF = RPMTAG_OBSOLETEFLAGS;
    } else {
	goto exit;
    }

    ds = xcalloc(1, sizeof(*ds));
    ds->i = -1;

    ds->Type = Type;
    ds->DNEVR = NULL;

    ds->tagN = tagN;
    ds->h = (scareMem ? headerLink(h, "dsNew") : NULL);
    if (hge(h, tagN, &ds->Nt, (void **) &ds->N, &ds->Count)) {
	int xx;
	xx = hge(h, tagEVR, &ds->EVRt, (void **) &ds->EVR, NULL);
	xx = hge(h, tagF, &ds->Ft, (void **) &ds->Flags, NULL);
	if (!scareMem && ds->Flags != NULL)
	    ds->Flags = memcpy(xmalloc(ds->Count * sizeof(*ds->Flags)),
                                ds->Flags, ds->Count* sizeof(*ds->Flags));
    } else
	ds->h = headerFree(ds->h, "dsNew");

exit:

/*@-modfilesystem@*/
if (_ds_debug)
fprintf(stderr, "*** ds %p ++ %s[%d]\n", ds, ds->Type, ds->Count);
/*@=modfilesystem@*/

    /*@-nullret@*/ /* FIX: ds->Flags may be NULL. */
    return ds;
    /*@=nullret@*/
}

char * dsDNEVR(const char * depend, const rpmDepSet key)
{
    char * tbuf, * t;
    size_t nb;

    nb = 0;
    if (depend)	nb += strlen(depend) + 1;
    if (key->N[key->i])	nb += strlen(key->N[key->i]);
    if (key->Flags[key->i] & RPMSENSE_SENSEMASK) {
	if (nb)	nb++;
	if (key->Flags[key->i] & RPMSENSE_LESS)	nb++;
	if (key->Flags[key->i] & RPMSENSE_GREATER) nb++;
	if (key->Flags[key->i] & RPMSENSE_EQUAL)	nb++;
    }
    if (key->EVR[key->i] && *key->EVR[key->i]) {
	if (nb)	nb++;
	nb += strlen(key->EVR[key->i]);
    }

    t = tbuf = xmalloc(nb + 1);
    if (depend) {
	t = stpcpy(t, depend);
	*t++ = ' ';
    }
    if (key->N[key->i])
	t = stpcpy(t, key->N[key->i]);
    if (key->Flags[key->i] & RPMSENSE_SENSEMASK) {
	if (t != tbuf)	*t++ = ' ';
	if (key->Flags[key->i] & RPMSENSE_LESS)	*t++ = '<';
	if (key->Flags[key->i] & RPMSENSE_GREATER) *t++ = '>';
	if (key->Flags[key->i] & RPMSENSE_EQUAL)	*t++ = '=';
    }
    if (key->EVR[key->i] && *key->EVR[key->i]) {
	if (t != tbuf)	*t++ = ' ';
	t = stpcpy(t, key->EVR[key->i]);
    }
    *t = '\0';
    return tbuf;
}

const char * dsiGetDNEVR(rpmDepSet ds)
{
    const char * DNEVR = NULL;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->DNEVR != NULL)
	    DNEVR = ds->DNEVR;
    }
    return DNEVR;
}

const char * dsiGetN(rpmDepSet ds)
{
    const char * N = NULL;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->N != NULL)
	    N = ds->N[ds->i];
    }
    return N;
}

const char * dsiGetEVR(rpmDepSet ds)
{
    const char * EVR = NULL;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->EVR != NULL)
	    EVR = ds->EVR[ds->i];
    }
    return EVR;
}

int_32 dsiGetFlags(rpmDepSet ds)
{
    int_32 Flags = 0;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->Flags != NULL)
	    Flags = ds->Flags[ds->i];
    }
    return Flags;
}

void dsiNotify(rpmDepSet ds, const char * where, int rc)
{
    if (!(ds != NULL && ds->i >= 0 && ds->i < ds->Count))
	return;
    if (!(ds->Type != NULL && ds->DNEVR != NULL))
	return;

    rpmMessage(RPMMESS_DEBUG, "%9s: %-45s %-s %s\n", ds->Type,
		(!strcmp(ds->DNEVR, "cached") ? ds->DNEVR : ds->DNEVR+2),
		(rc ? _("NO ") : _("YES")),
		(where != NULL ? where : ""));
}

int dsiNext(/*@null@*/ rpmDepSet ds)
	/*@modifies ds @*/
{
    int i = -1;

    if (ds != NULL && ++ds->i >= 0) {
	if (ds->i < ds->Count) {
	    char t[2];
	    i = ds->i;
	    ds->DNEVR = _free(ds->DNEVR);
	    t[0] = ((ds->Type != NULL) ? ds->Type[0] : '\0');
	    t[1] = '\0';
	    /*@-nullstate@*/
	    ds->DNEVR = dsDNEVR(t, ds);
	    /*@=nullstate@*/

/*@-modfilesystem@*/
if (_ds_debug)
fprintf(stderr, "*** ds %p[%d] %s: %s\n", ds, i, (ds->Type ? ds->Type : "???"), ds->DNEVR);
/*@=modfilesystem@*/

	}
    }
    return i;
}

rpmDepSet dsiInit(/*@returned@*/ /*@null@*/ rpmDepSet ds)
	/*@modifies ds @*/
{
    if (ds != NULL)
	ds->i = -1;
    return ds;
}

/**
 * Split EVR into epoch, version, and release components.
 * @param evr		[epoch:]version[-release] string
 * @retval *ep		pointer to epoch
 * @retval *vp		pointer to version
 * @retval *rp		pointer to release
 */
static
void parseEVR(char * evr,
		/*@exposed@*/ /*@out@*/ const char ** ep,
		/*@exposed@*/ /*@out@*/ const char ** vp,
		/*@exposed@*/ /*@out@*/ const char ** rp)
	/*@modifies *ep, *vp, *rp @*/
{
    const char *epoch;
    const char *version;		/* assume only version is present */
    const char *release;
    char *s, *se;

    s = evr;
    while (*s && xisdigit(*s)) s++;	/* s points to epoch terminator */
    se = strrchr(s, '-');		/* se points to version terminator */

    if (*s == ':') {
	epoch = evr;
	*s++ = '\0';
	version = s;
	/*@-branchstate@*/
	if (*epoch == '\0') epoch = "0";
	/*@=branchstate@*/
    } else {
	epoch = NULL;	/* XXX disable epoch compare if missing */
	version = evr;
    }
    if (se) {
	*se++ = '\0';
	release = se;
    } else {
	release = NULL;
    }

    if (ep) *ep = epoch;
    if (vp) *vp = version;
    if (rp) *rp = release;
}

int dsCompare(const rpmDepSet A, const rpmDepSet B)
{
    const char *aDepend = (A->DNEVR != NULL ? xstrdup(A->DNEVR+2) : "");
    const char *bDepend = (B->DNEVR != NULL ? xstrdup(B->DNEVR+2) : "");
    char *aEVR, *bEVR;
    const char *aE, *aV, *aR, *bE, *bV, *bR;
    int result;
    int sense;

    /* Different names don't overlap. */
    if (strcmp(A->N[A->i], B->N[B->i])) {
	result = 0;
	goto exit;
    }

    /* Same name. If either A or B is an existence test, always overlap. */
    if (!((A->Flags[A->i] & RPMSENSE_SENSEMASK) && (B->Flags[B->i] & RPMSENSE_SENSEMASK))) {
	result = 1;
	goto exit;
    }

    /* If either EVR is non-existent or empty, always overlap. */
    if (!(A->EVR[A->i] && *A->EVR[A->i] && B->EVR[B->i] && *B->EVR[B->i])) {
	result = 1;
	goto exit;
    }

    /* Both AEVR and BEVR exist. */
    aEVR = xstrdup(A->EVR[A->i]);
    parseEVR(aEVR, &aE, &aV, &aR);
    bEVR = xstrdup(B->EVR[B->i]);
    parseEVR(bEVR, &bE, &bV, &bR);

    /* Compare {A,B} [epoch:]version[-release] */
    sense = 0;
    if (aE && *aE && bE && *bE)
	sense = rpmvercmp(aE, bE);
    else if (aE && *aE && atol(aE) > 0) {
	/* XXX legacy epoch-less requires/conflicts compatibility */
	rpmMessage(RPMMESS_DEBUG, _("the \"B\" dependency needs an epoch (assuming same as \"A\")\n\tA %s\tB %s\n"),
		aDepend, bDepend);
	sense = 0;
    } else if (bE && *bE && atol(bE) > 0)
	sense = -1;

    if (sense == 0) {
	sense = rpmvercmp(aV, bV);
	if (sense == 0 && aR && *aR && bR && *bR) {
	    sense = rpmvercmp(aR, bR);
	}
    }
    aEVR = _free(aEVR);
    bEVR = _free(bEVR);

    /* Detect overlap of {A,B} range. */
    result = 0;
    if (sense < 0 && ((A->Flags[A->i] & RPMSENSE_GREATER) || (B->Flags[B->i] & RPMSENSE_LESS))) {
	result = 1;
    } else if (sense > 0 && ((A->Flags[A->i] & RPMSENSE_LESS) || (B->Flags[B->i] & RPMSENSE_GREATER))) {
	result = 1;
    } else if (sense == 0 &&
	(((A->Flags[A->i] & RPMSENSE_EQUAL) && (B->Flags[B->i] & RPMSENSE_EQUAL)) ||
	 ((A->Flags[A->i] & RPMSENSE_LESS) && (B->Flags[B->i] & RPMSENSE_LESS)) ||
	 ((A->Flags[A->i] & RPMSENSE_GREATER) && (B->Flags[B->i] & RPMSENSE_GREATER)))) {
	result = 1;
    }

exit:
    rpmMessage(RPMMESS_DEBUG, _("  %s    A %s\tB %s\n"),
	(result ? _("YES") : _("NO ")), aDepend, bDepend);
    aDepend = _free(aDepend);
    bDepend = _free(bDepend);
    return result;
}

void dsProblem(problemsSet psp, Header h, const rpmDepSet dep,
		const void ** suggestedPackages)
{
    rpmDependencyConflict dcp;
    const char * Name =  dsiGetN(dep);
    const char * DNEVR = dsiGetDNEVR(dep);
    const char * EVR = dsiGetEVR(dep);
    int_32 Flags = dsiGetFlags(dep);
    const char * name, * version, * release;
    int xx;

    xx = headerNVR(h, &name, &version, &release);

    /*@-branchstate@*/
    if (Name == NULL) Name = "???";
    if (EVR == NULL) EVR = "???";
    if (DNEVR == NULL) DNEVR = "?????";
    /*@=branchstate@*/

    rpmMessage(RPMMESS_DEBUG, _("package %s-%s-%s has unsatisfied %s: %s\n"),
	    name, version, release,
	    dep->Type,
	    DNEVR+2);

    if (psp->num == psp->alloced) {
	psp->alloced += 5;
	psp->problems = xrealloc(psp->problems,
				sizeof(*psp->problems) * psp->alloced);
    }

    dcp = psp->problems + psp->num;
    psp->num++;

    dcp->byHeader = headerLink(h, "dsProblem");
    dcp->byName = xstrdup(name);
    dcp->byVersion = xstrdup(version);
    dcp->byRelease = xstrdup(release);
    dcp->needsName = xstrdup(Name);
    dcp->needsVersion = xstrdup(EVR);
    dcp->needsFlags = Flags;

    if (dep->tagN == RPMTAG_REQUIRENAME)
	dcp->sense = RPMDEP_SENSE_REQUIRES;
    else if (dep->tagN == RPMTAG_CONFLICTNAME)
	dcp->sense = RPMDEP_SENSE_CONFLICTS;
    else
	dcp->sense = 0;

    dcp->suggestedPackages = suggestedPackages;
}

int rangeMatchesDepFlags (Header h, const rpmDepSet req)
{
    int scareMem = 1;
    rpmDepSet provides = NULL;
    int result = 0;

    if (!(req->Flags[req->i] & RPMSENSE_SENSEMASK) || !req->EVR[req->i] || *req->EVR[req->i] == '\0')
	return 1;

    /* Get provides information from header */
    provides = dsiInit(dsNew(h, RPMTAG_PROVIDENAME, scareMem));
    if (provides == NULL)
	goto exit;	/* XXX should never happen */

    /*
     * Rpm prior to 3.0.3 did not have versioned provides.
     * If no provides version info is available, match any requires.
     */
    if (provides->EVR == NULL) {
	result = 1;
	goto exit;
    }

    result = 0;
    if (provides != NULL)
    while (dsiNext(provides) >= 0) {

	/* Filter out provides that came along for the ride. */
	if (strcmp(provides->N[provides->i], req->N[req->i]))
	    continue;

	result = dsCompare(provides, req);

	/* If this provide matches the require, we're done. */
	if (result)
	    break;
    }

exit:
    provides = dsFree(provides);

    return result;
}

int headerMatchesDepFlags(Header h, const rpmDepSet req)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    const char *name, *version, *release;
    int_32 * epoch;
    const char * pkgEVR;
    char * p;
    int_32 pkgFlags = RPMSENSE_EQUAL;
    rpmDepSet pkg = memset(alloca(sizeof(*pkg)), 0, sizeof(*pkg));
    int rc;

    if (!((req->Flags[req->i] & RPMSENSE_SENSEMASK) && req->EVR[req->i] && *req->EVR[req->i]))
	return 1;

    /* Get package information from header */
    (void) headerNVR(h, &name, &version, &release);

    pkgEVR = p = alloca(21 + strlen(version) + 1 + strlen(release) + 1);
    *p = '\0';
    if (hge(h, RPMTAG_EPOCH, NULL, (void **) &epoch, NULL)) {
	sprintf(p, "%d:", *epoch);
	while (*p != '\0')
	    p++;
    }
    (void) stpcpy( stpcpy( stpcpy(p, version) , "-") , release);

    /*@-compmempass@*/ /* FIX: move pkg immediate variables from stack */
    pkg->i = -1;
    pkg->Type = "Provides";
    pkg->tagN = RPMTAG_PROVIDENAME;
    pkg->DNEVR = NULL;
    /*@-immediatetrans@*/
    pkg->N = &name;
    pkg->EVR = &pkgEVR;
    pkg->Flags = &pkgFlags;
    /*@=immediatetrans@*/
    pkg->Count = 1;
    (void) dsiNext(dsiInit(pkg));

    rc = dsCompare(pkg, req);

    pkg->DNEVR = _free(pkg->DNEVR);
    /*@=compmempass@*/

    return rc;
}
