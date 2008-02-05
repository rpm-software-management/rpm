/** \ingroup rpmdep
 * \file lib/rpmds.c
 */
#include "system.h"

#include <rpm/rpmtag.h>
#include <rpm/rpmlib.h>		/* rpmvercmp */
#include <rpm/rpmstring.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmds.h>

#include "debug.h"

/**
 * Enable noisy range comparison debugging message?
 */
static int _noisy_range_comparison_debug_message = 0;

int _rpmds_debug = 0;

int _rpmds_nopromote = 1;

int _rpmds_unspecified_epoch_noise = 0;

/**
 * A package dependency set.
 */
struct rpmds_s {
    const char * Type;		/*!< Tag name. */
    char * DNEVR;		/*!< Formatted dependency string. */
    Header h;			/*!< Header for dependency set (or NULL) */
    const char ** N;		/*!< Name. */
    const char ** EVR;		/*!< Epoch-Version-Release. */
    rpmsenseFlags * Flags;	/*!< Bit(s) identifying context/comparison. */
    rpm_color_t * Color;	/*!< Bit(s) calculated from file color(s). */
    int32_t * Refs;		/*!< No. of file refs. */
    time_t BT;			/*!< Package build time tie breaker. */
    rpm_tag_t tagN;		/*!< Header tag. */
    rpm_tagtype_t Nt, EVRt, Ft;	/*!< Tag data types. */
    int32_t Count;		/*!< No. of elements */
    int i;			/*!< Element index. */
    unsigned l;			/*!< Low element (bsearch). */
    unsigned u;			/*!< High element (bsearch). */
    int nopromote;		/*!< Don't promote Epoch: in rpmdsCompare()? */
    int nrefs;			/*!< Reference count. */
};

static int dsType(rpm_tag_t tag, 
		  const char ** Type, rpm_tag_t * tagEVR, rpm_tag_t * tagF)
{
    int rc = 0;
    const char *t = NULL;
    rpm_tag_t evr = RPMTAG_NOT_FOUND;
    rpm_tag_t f = RPMTAG_NOT_FOUND;

    if (tag == RPMTAG_PROVIDENAME) {
	t = "Provides";
	evr = RPMTAG_PROVIDEVERSION;
	f = RPMTAG_PROVIDEFLAGS;
    } else if (tag == RPMTAG_REQUIRENAME) {
	t = "Requires";
	evr = RPMTAG_REQUIREVERSION;
	f = RPMTAG_REQUIREFLAGS;
    } else if (tag == RPMTAG_CONFLICTNAME) {
	t = "Conflicts";
	evr = RPMTAG_CONFLICTVERSION;
	f = RPMTAG_CONFLICTFLAGS;
    } else if (tag == RPMTAG_OBSOLETENAME) {
	t = "Obsoletes";
	evr = RPMTAG_OBSOLETEVERSION;
	f = RPMTAG_OBSOLETEFLAGS;
    } else if (tag == RPMTAG_TRIGGERNAME) {
	t = "Trigger";
	evr = RPMTAG_TRIGGERVERSION;
	f = RPMTAG_TRIGGERFLAGS;
    } else {
	rc = 1;
    } 
    if (Type) *Type = t;
    if (tagEVR) *tagEVR = evr;
    if (tagF) *tagF = f;
    return rc;
}    

rpmds rpmdsUnlink(rpmds ds, const char * msg)
{
    if (ds == NULL) return NULL;
if (_rpmds_debug && msg != NULL)
fprintf(stderr, "--> ds %p -- %d %s\n", ds, ds->nrefs, msg);
    ds->nrefs--;
    return NULL;
}

rpmds rpmdsLink(rpmds ds, const char * msg)
{
    if (ds == NULL) return NULL;
    ds->nrefs++;

if (_rpmds_debug && msg != NULL)
fprintf(stderr, "--> ds %p ++ %d %s\n", ds, ds->nrefs, msg);

    return ds;
}

rpmds rpmdsFree(rpmds ds)
{
    HFD_t hfd = headerFreeData;
    rpm_tag_t tagEVR, tagF;

    if (ds == NULL)
	return NULL;

    if (ds->nrefs > 1)
	return rpmdsUnlink(ds, ds->Type);

if (_rpmds_debug < 0)
fprintf(stderr, "*** ds %p\t%s[%d]\n", ds, ds->Type, ds->Count);

    if (dsType(ds->tagN, NULL, &tagEVR, &tagF))
	return NULL;

    if (ds->Count > 0) {
	ds->N = hfd(ds->N, ds->Nt);
	ds->EVR = hfd(ds->EVR, ds->EVRt);
	ds->Flags = (ds->h != NULL ? hfd(ds->Flags, ds->Ft) : _free(ds->Flags));
	ds->h = headerFree(ds->h);
    }

    ds->DNEVR = _free(ds->DNEVR);
    ds->Color = _free(ds->Color);
    ds->Refs = _free(ds->Refs);

    (void) rpmdsUnlink(ds, ds->Type);
    memset(ds, 0, sizeof(*ds));		/* XXX trash and burn */
    ds = _free(ds);
    return NULL;
}

rpmds rpmdsNew(Header h, rpm_tag_t tagN, int flags)
{
    int scareMem = (flags & 0x1);
    HGE_t hge =
	(scareMem ? (HGE_t) headerGetEntryMinMemory : (HGE_t) headerGetEntry);
    rpm_tag_t tagBT = RPMTAG_BUILDTIME;
    rpm_tagtype_t BTt;
    rpm_time_t * BTp;
    rpm_tag_t tagEVR, tagF;
    rpmds ds = NULL;
    const char * Type;
    const char ** N;
    rpm_tagtype_t Nt;
    rpm_count_t Count;

    if (dsType(tagN, &Type, &tagEVR, &tagF))
	goto exit;

    if (hge(h, tagN, &Nt, (rpm_data_t *) &N, &Count)
     && N != NULL && Count > 0)
    {
	int xx;

	ds = xcalloc(1, sizeof(*ds));
	ds->Type = Type;
	ds->h = (scareMem ? headerLink(h) : NULL);
	ds->i = -1;
	ds->DNEVR = NULL;
	ds->tagN = tagN;
	ds->N = N;
	ds->Nt = Nt;
	ds->Count = Count;
	ds->nopromote = _rpmds_nopromote;

	xx = hge(h, tagEVR, &ds->EVRt, (rpm_data_t *) &ds->EVR, NULL);
	xx = hge(h, tagF, &ds->Ft, (rpm_data_t *) &ds->Flags, NULL);
	if (!scareMem && ds->Flags != NULL)
	    ds->Flags = memcpy(xmalloc(ds->Count * sizeof(*ds->Flags)),
                                ds->Flags, ds->Count * sizeof(*ds->Flags));
	xx = hge(h, tagBT, &BTt, (rpm_data_t *) &BTp, NULL);
	ds->BT = (xx && BTp != NULL && BTt == RPM_INT32_TYPE ? *BTp : 0);
	ds->Color = xcalloc(Count, sizeof(*ds->Color));
	ds->Refs = xcalloc(Count, sizeof(*ds->Refs));

if (_rpmds_debug < 0)
fprintf(stderr, "*** ds %p\t%s[%d]\n", ds, ds->Type, ds->Count);

    }

exit:
    /* FIX: ds->Flags may be NULL */
    ds = rpmdsLink(ds, (ds ? ds->Type : RPMDBG()));

    return ds;
}

char * rpmdsNewDNEVR(const char * dspfx, const rpmds ds)
{
    char * tbuf, * t;
    size_t nb;

    nb = 0;
    if (dspfx)	nb += strlen(dspfx) + 1;
    if (ds->N[ds->i])	nb += strlen(ds->N[ds->i]);
    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (ds->Flags != NULL && (ds->Flags[ds->i] & RPMSENSE_SENSEMASK)) {
	if (nb)	nb++;
	if (ds->Flags[ds->i] & RPMSENSE_LESS)	nb++;
	if (ds->Flags[ds->i] & RPMSENSE_GREATER) nb++;
	if (ds->Flags[ds->i] & RPMSENSE_EQUAL)	nb++;
    }
    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (ds->EVR != NULL && ds->EVR[ds->i] && *ds->EVR[ds->i]) {
	if (nb)	nb++;
	nb += strlen(ds->EVR[ds->i]);
    }

    t = tbuf = xmalloc(nb + 1);
    if (dspfx) {
	t = stpcpy(t, dspfx);
	*t++ = ' ';
    }
    if (ds->N[ds->i])
	t = stpcpy(t, ds->N[ds->i]);
    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (ds->Flags != NULL && (ds->Flags[ds->i] & RPMSENSE_SENSEMASK)) {
	if (t != tbuf)	*t++ = ' ';
	if (ds->Flags[ds->i] & RPMSENSE_LESS)	*t++ = '<';
	if (ds->Flags[ds->i] & RPMSENSE_GREATER) *t++ = '>';
	if (ds->Flags[ds->i] & RPMSENSE_EQUAL)	*t++ = '=';
    }
    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (ds->EVR != NULL && ds->EVR[ds->i] && *ds->EVR[ds->i]) {
	if (t != tbuf)	*t++ = ' ';
	t = stpcpy(t, ds->EVR[ds->i]);
    }
    *t = '\0';
    return tbuf;
}

rpmds rpmdsThis(Header h, rpm_tag_t tagN, rpmsenseFlags Flags)
{
    HGE_t hge = (HGE_t) headerGetEntryMinMemory;
    rpmds ds = NULL;
    const char * Type;
    const char * n, * v, * r;
    int32_t * ep;
    const char ** N, ** EVR;
    char * t;
    int xx;

    if (dsType(tagN, &Type, NULL, NULL))
	goto exit;

    xx = headerNVR(h, &n, &v, &r);
    ep = NULL;
    xx = hge(h, RPMTAG_EPOCH, NULL, (rpm_data_t *)&ep, NULL);

    t = xmalloc(sizeof(*N) + strlen(n) + 1);
    N = (const char **) t;
    t += sizeof(*N);
    *t = '\0';
    N[0] = t;
    t = stpcpy(t, n);

    t = xmalloc(sizeof(*EVR) +
		(ep ? 20 : 0) + strlen(v) + strlen(r) + sizeof("-"));
    EVR = (const char **) t;
    t += sizeof(*EVR);
    *t = '\0';
    EVR[0] = t;
    if (ep) {
	sprintf(t, "%d:", *ep);
	t += strlen(t);
    }
    t = stpcpy( stpcpy( stpcpy( t, v), "-"), r);

    ds = xcalloc(1, sizeof(*ds));
    ds->h = NULL;
    ds->Type = Type;
    ds->tagN = tagN;
    ds->Count = 1;
    ds->N = N;
    ds->Nt = RPM_FORCEFREE_TYPE;	/* XXX to insure that hfd will free */
    ds->EVR = EVR;
    ds->EVRt = RPM_FORCEFREE_TYPE;	/* XXX to insure that hfd will free */
    ds->Flags = xmalloc(sizeof(*ds->Flags));	ds->Flags[0] = Flags;
    ds->i = 0;
    {	char pre[2];
	pre[0] = ds->Type[0];
	pre[1] = '\0';
	/* LCL: ds->Type may be NULL ??? */
	ds->DNEVR = rpmdsNewDNEVR(pre, ds);
    }

exit:
    return rpmdsLink(ds, (ds ? ds->Type : RPMDBG()));
}

rpmds rpmdsSingle(rpm_tag_t tagN, const char * N, const char * EVR, rpmsenseFlags Flags)
{
    rpmds ds = NULL;
    const char * Type;

    if (dsType(tagN, &Type, NULL, NULL))
	goto exit;

    ds = xcalloc(1, sizeof(*ds));
    ds->h = NULL;
    ds->Type = Type;
    ds->tagN = tagN;
    {	time_t now = time(NULL);
	ds->BT = now;
    }
    ds->Count = 1;
    ds->N = xmalloc(sizeof(*ds->N));		ds->N[0] = N;
    ds->Nt = RPM_FORCEFREE_TYPE;	/* XXX to insure that hfd will free */
    ds->EVR = xmalloc(sizeof(*ds->EVR));	ds->EVR[0] = EVR;
    ds->EVRt = RPM_FORCEFREE_TYPE;	/* XXX to insure that hfd will free */
    ds->Flags = xmalloc(sizeof(*ds->Flags));	ds->Flags[0] = Flags;
    ds->i = 0;
    {	char t[2];
	t[0] = ds->Type[0];
	t[1] = '\0';
	ds->DNEVR = rpmdsNewDNEVR(t, ds);
    }

exit:
    return rpmdsLink(ds, (ds ? ds->Type : NULL));
}

int rpmdsCount(const rpmds ds)
{
    return (ds != NULL ? ds->Count : 0);
}

int rpmdsIx(const rpmds ds)
{
    return (ds != NULL ? ds->i : -1);
}

int rpmdsSetIx(rpmds ds, int ix)
{
    int i = -1;

    if (ds != NULL) {
	i = ds->i;
	ds->i = ix;
    }
    return i;
}

const char * rpmdsDNEVR(const rpmds ds)
{
    const char * DNEVR = NULL;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->DNEVR != NULL)
	    DNEVR = ds->DNEVR;
    }
    return DNEVR;
}

const char * rpmdsN(const rpmds ds)
{
    const char * N = NULL;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->N != NULL)
	    N = ds->N[ds->i];
    }
    return N;
}

const char * rpmdsEVR(const rpmds ds)
{
    const char * EVR = NULL;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->EVR != NULL)
	    EVR = ds->EVR[ds->i];
    }
    return EVR;
}

rpmsenseFlags rpmdsFlags(const rpmds ds)
{
    rpmsenseFlags Flags = 0;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->Flags != NULL)
	    Flags = ds->Flags[ds->i];
    }
    return Flags;
}

rpm_tag_t rpmdsTagN(const rpmds ds)
{
    rpm_tag_t tagN = 0;

    if (ds != NULL)
	tagN = ds->tagN;
    return tagN;
}

time_t rpmdsBT(const rpmds ds)
{
    time_t BT = 0;
    if (ds != NULL && ds->BT > 0)
	BT = ds->BT;
    return BT;
}

time_t rpmdsSetBT(const rpmds ds, time_t BT)
{
    time_t oBT = 0;
    if (ds != NULL) {
	oBT = ds->BT;
	ds->BT = BT;
    }
    return oBT;
}

int rpmdsNoPromote(const rpmds ds)
{
    int nopromote = 0;

    if (ds != NULL)
	nopromote = ds->nopromote;
    return nopromote;
}

int rpmdsSetNoPromote(rpmds ds, int nopromote)
{
    int onopromote = 0;

    if (ds != NULL) {
	onopromote = ds->nopromote;
	ds->nopromote = nopromote;
    }
    return onopromote;
}

rpm_color_t rpmdsColor(const rpmds ds)
{
    rpm_color_t Color = 0;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->Color != NULL)
	    Color = ds->Color[ds->i];
    }
    return Color;
}

rpm_color_t rpmdsSetColor(const rpmds ds, rpm_color_t color)
{
    rpm_color_t ocolor = 0;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->Color != NULL) {
	    ocolor = ds->Color[ds->i];
	    ds->Color[ds->i] = color;
	}
    }
    return ocolor;
}

int32_t rpmdsRefs(const rpmds ds)
{
    int32_t Refs = 0;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->Refs != NULL)
	    Refs = ds->Refs[ds->i];
    }
    return Refs;
}

int32_t rpmdsSetRefs(const rpmds ds, int32_t refs)
{
    int32_t orefs = 0;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->Refs != NULL) {
	    orefs = ds->Refs[ds->i];
	    ds->Refs[ds->i] = refs;
	}
    }
    return orefs;
}

void rpmdsNotify(rpmds ds, const char * where, int rc)
{
    if (!(ds != NULL && ds->i >= 0 && ds->i < ds->Count))
	return;
    if (!(ds->Type != NULL && ds->DNEVR != NULL))
	return;

    rpmlog(RPMLOG_DEBUG, "%9s: %-45s %-s %s\n", ds->Type,
		(!strcmp(ds->DNEVR, "cached") ? ds->DNEVR : ds->DNEVR+2),
		(rc ? _("NO ") : _("YES")),
		(where != NULL ? where : ""));
}

int rpmdsNext(rpmds ds)
{
    int i = -1;

    if (ds != NULL && ++ds->i >= 0) {
	if (ds->i < ds->Count) {
	    char t[2];
	    i = ds->i;
	    ds->DNEVR = _free(ds->DNEVR);
	    t[0] = ((ds->Type != NULL) ? ds->Type[0] : '\0');
	    t[1] = '\0';
	    ds->DNEVR = rpmdsNewDNEVR(t, ds);

	} else
	    ds->i = -1;

if (_rpmds_debug  < 0 && i != -1)
fprintf(stderr, "*** ds %p\t%s[%d]: %s\n", ds, (ds->Type ? ds->Type : "?Type?"), i, (ds->DNEVR ? ds->DNEVR : "?DNEVR?"));

    }

    return i;
}

rpmds rpmdsInit(rpmds ds)
{
    if (ds != NULL)
	ds->i = -1;
    return ds;
}

static
const char ** rpmdsDupArgv(const char ** argv, int argc)
{
    const char ** av;
    size_t nb = 0;
    int ac = 0;
    char * t;

    if (argv == NULL)
	return NULL;
    for (ac = 0; ac < argc; ac++) {
assert(argv[ac] != NULL);
	nb += strlen(argv[ac]) + 1;
    }
    nb += (ac + 1) * sizeof(*av);

    av = xmalloc(nb);
    t = (char *) (av + ac + 1);
    for (ac = 0; ac < argc; ac++) {
	av[ac] = t;
	t = stpcpy(t, argv[ac]) + 1;
    }
    av[ac] = NULL;
    return av;
}

static rpmds rpmdsDup(const rpmds ods)
{
    rpmds ds = xcalloc(1, sizeof(*ds));
    size_t nb;

    ds->h = (ods->h != NULL ? headerLink(ods->h) : NULL);
    ds->Type = ods->Type;
    ds->tagN = ods->tagN;
    ds->Count = ods->Count;
    ds->i = ods->i;
    ds->l = ods->l;
    ds->u = ods->u;

    nb = (ds->Count+1) * sizeof(*ds->N);
    ds->N = (ds->h != NULL
	? memcpy(xmalloc(nb), ods->N, nb)
	: rpmdsDupArgv(ods->N, ods->Count) );
    ds->Nt = ods->Nt;

    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
assert(ods->EVR != NULL);
assert(ods->Flags != NULL);

    nb = (ds->Count+1) * sizeof(*ds->EVR);
    ds->EVR = (ds->h != NULL
	? memcpy(xmalloc(nb), ods->EVR, nb)
	: rpmdsDupArgv(ods->EVR, ods->Count) );
    ds->EVRt = ods->EVRt;

    nb = (ds->Count * sizeof(*ds->Flags));
    ds->Flags = (ds->h != NULL
	? ods->Flags
	: memcpy(xmalloc(nb), ods->Flags, nb) );
    ds->Ft = ods->Ft;

/* FIX: ds->Flags is kept, not only */
    return rpmdsLink(ds, (ds ? ds->Type : NULL));

}

int rpmdsFind(rpmds ds, const rpmds ods)
{
    int comparison;

    if (ds == NULL || ods == NULL)
	return -1;

    ds->l = 0;
    ds->u = ds->Count;
    while (ds->l < ds->u) {
	ds->i = (ds->l + ds->u) / 2;

	comparison = strcmp(ods->N[ods->i], ds->N[ds->i]);

	/* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
	if (comparison == 0 && ods->EVR && ds->EVR)
	    comparison = strcmp(ods->EVR[ods->i], ds->EVR[ds->i]);
	if (comparison == 0 && ods->Flags && ds->Flags)
	    comparison = (ods->Flags[ods->i] - ds->Flags[ds->i]);

	if (comparison < 0)
	    ds->u = ds->i;
	else if (comparison > 0)
	    ds->l = ds->i + 1;
	else
	    return ds->i;
    }
    return -1;
}

int rpmdsMerge(rpmds * dsp, rpmds ods)
{
    rpmds ds;
    const char ** N;
    const char ** EVR;
    rpmsenseFlags * Flags;
    int j;
int save;

    if (dsp == NULL || ods == NULL)
	return -1;

    /* If not initialized yet, dup the 1st entry. */
    if (*dsp == NULL) {
	save = ods->Count;
	ods->Count = 1;
	*dsp = rpmdsDup(ods);
	ods->Count = save;
    }
    ds = *dsp;
    if (ds == NULL)
	return -1;

    /*
     * Add new entries.
     */
save = ods->i;
    ods = rpmdsInit(ods);
    if (ods != NULL)
    while (rpmdsNext(ods) >= 0) {
	/*
	 * If this entry is already present, don't bother.
	 */
	if (rpmdsFind(ds, ods) >= 0)
	    continue;

	/*
	 * Insert new entry.
	 */
	for (j = ds->Count; j > ds->u; j--)
	    ds->N[j] = ds->N[j-1];
	ds->N[ds->u] = ods->N[ods->i];
	N = rpmdsDupArgv(ds->N, ds->Count+1);
	ds->N = _free(ds->N);
	ds->N = N;
	
	/* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
assert(ods->EVR != NULL);
assert(ods->Flags != NULL);

	for (j = ds->Count; j > ds->u; j--)
	    ds->EVR[j] = ds->EVR[j-1];
	ds->EVR[ds->u] = ods->EVR[ods->i];
	EVR = rpmdsDupArgv(ds->EVR, ds->Count+1);
	ds->EVR = _free(ds->EVR);
	ds->EVR = EVR;

	Flags = xmalloc((ds->Count+1) * sizeof(*Flags));
	if (ds->u > 0)
	    memcpy(Flags, ds->Flags, ds->u * sizeof(*Flags));
	if (ds->u < ds->Count)
	    memcpy(Flags + ds->u + 1, ds->Flags + ds->u, (ds->Count - ds->u) * sizeof(*Flags));
	Flags[ds->u] = ods->Flags[ods->i];
	ds->Flags = _free(ds->Flags);
	ds->Flags = Flags;

	ds->i = ds->Count;
	ds->Count++;

    }
ods->i = save;
    return 0;
}


int rpmdsSearch(rpmds ds, rpmds ods)
{
    int comparison;
    int i, l, u;

    if (ds == NULL || ods == NULL)
	return -1;

    /* Binary search to find the [l,u) subset that contains N */
    i = -1;
    l = 0;
    u = ds->Count;
    while (l < u) {
	i = (l + u) / 2;

	comparison = strcmp(ods->N[ods->i], ds->N[i]);

	if (comparison < 0)
	    u = i;
	else if (comparison > 0)
	    l = i + 1;
	else {
	    /* Set l to 1st member of set that contains N. */
	    if (strcmp(ods->N[ods->i], ds->N[l]))
		l = i;
	    while (l > 0 && !strcmp(ods->N[ods->i], ds->N[l-1]))
		l--;
	    /* Set u to 1st member of set that does not contain N. */
	    if (u >= ds->Count || strcmp(ods->N[ods->i], ds->N[u]))
		u = i;
	    while (++u < ds->Count) {
		if (strcmp(ods->N[ods->i], ds->N[u]))
		    /*@innerbreak@*/ break;
	    }
	    break;
	}
    }

    /* Check each member of [l,u) subset for ranges overlap. */
    i = -1;
    if (l < u) {
	int save = rpmdsSetIx(ds, l-1);
	while ((l = rpmdsNext(ds)) >= 0 && (l < u)) {
	    if ((i = rpmdsCompare(ods, ds)) != 0)
		break;
	}
	/* Return element index that overlaps, or -1. */
	if (i)
	    i = rpmdsIx(ds);
	else {
	    (void) rpmdsSetIx(ds, save);
	    i = -1;
	}
    }
    return i;
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
		const char ** ep,
		const char ** vp,
		const char ** rp)
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
	if (*epoch == '\0') epoch = "0";
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

int rpmdsCompare(const rpmds A, const rpmds B)
{
    char *aDepend = (A->DNEVR != NULL ? xstrdup(A->DNEVR+2) : "");
    char *bDepend = (B->DNEVR != NULL ? xstrdup(B->DNEVR+2) : "");
    char *aEVR, *bEVR;
    const char *aE, *aV, *aR, *bE, *bV, *bR;
    int result;
    int sense;

    /* Different names don't overlap. */
    if (strcmp(A->N[A->i], B->N[B->i])) {
	result = 0;
	goto exit;
    }

    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (!(A->EVR && A->Flags && B->EVR && B->Flags)) {
	result = 1;
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
	if (!B->nopromote) {
	    int lvl = (_rpmds_unspecified_epoch_noise  ? RPMLOG_WARNING : RPMLOG_DEBUG);
	    rpmlog(lvl, _("The \"B\" dependency needs an epoch (assuming same epoch as \"A\")\n\tA = \"%s\"\tB = \"%s\"\n"),
		aDepend, bDepend);
	    sense = 0;
	} else
	    sense = 1;
    } else if (bE && *bE && atol(bE) > 0)
	sense = -1;

    if (sense == 0) {
	sense = rpmvercmp(aV, bV);
	if (sense == 0 && aR && *aR && bR && *bR)
	    sense = rpmvercmp(aR, bR);
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
    if (_noisy_range_comparison_debug_message)
    rpmlog(RPMLOG_DEBUG, _("  %s    A %s\tB %s\n"),
	(result ? _("YES") : _("NO ")), aDepend, bDepend);
    aDepend = _free(aDepend);
    bDepend = _free(bDepend);
    return result;
}

void rpmdsProblem(rpmps ps, const char * pkgNEVR, const rpmds ds,
	const fnpyKey * suggestedKeys, int adding)
{
    const char * Name =  rpmdsN(ds);
    const char * DNEVR = rpmdsDNEVR(ds);
    const char * EVR = rpmdsEVR(ds);
    rpmProblemType type;
    fnpyKey key;

    if (ps == NULL) return;

    if (Name == NULL) Name = "?N?";
    if (EVR == NULL) EVR = "?EVR?";
    if (DNEVR == NULL) DNEVR = "? ?N? ?OP? ?EVR?";

    rpmlog(RPMLOG_DEBUG, _("package %s has unsatisfied %s: %s\n"),
	    pkgNEVR, ds->Type, DNEVR+2);

    switch ((unsigned)DNEVR[0]) {
    case 'C':	type = RPMPROB_CONFLICT;	break;
    default:
    case 'R':	type = RPMPROB_REQUIRES;	break;
    }

    key = (suggestedKeys ? suggestedKeys[0] : NULL);
    rpmpsAppend(ps, type, pkgNEVR, key, NULL, NULL, DNEVR, adding);
}

int rpmdsAnyMatchesDep (const Header h, const rpmds req, int nopromote)
{
    int scareMem = 0;
    rpmds provides = NULL;
    int result = 0;

    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (req->EVR == NULL || req->Flags == NULL)
	return 1;

    if (!(req->Flags[req->i] & RPMSENSE_SENSEMASK) || !req->EVR[req->i] || *req->EVR[req->i] == '\0')
	return 1;

    /* Get provides information from header */
    provides = rpmdsInit(rpmdsNew(h, RPMTAG_PROVIDENAME, scareMem));
    if (provides == NULL)
	goto exit;	/* XXX should never happen */
    if (nopromote)
	(void) rpmdsSetNoPromote(provides, nopromote);

    /*
     * Rpm prior to 3.0.3 did not have versioned provides.
     * If no provides version info is available, match any/all requires
     * with same name.
     */
    if (provides->EVR == NULL) {
	result = 1;
	goto exit;
    }

    result = 0;
    if (provides != NULL)
    while (rpmdsNext(provides) >= 0) {

	/* Filter out provides that came along for the ride. */
	if (strcmp(provides->N[provides->i], req->N[req->i]))
	    continue;

	result = rpmdsCompare(provides, req);

	/* If this provide matches the require, we're done. */
	if (result)
	    break;
    }

exit:
    provides = rpmdsFree(provides);

    return result;
}

int rpmdsNVRMatchesDep(const Header h, const rpmds req, int nopromote)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    const char * pkgN, * v, * r;
    int32_t * epoch;
    const char * pkgEVR;
    char * t;
    rpmsenseFlags pkgFlags = RPMSENSE_EQUAL;
    rpmds pkg;
    int rc = 1;	/* XXX assume match, names already match here */

    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (req->EVR == NULL || req->Flags == NULL)
	return rc;

    if (!((req->Flags[req->i] & RPMSENSE_SENSEMASK) && req->EVR[req->i] && *req->EVR[req->i]))
	return rc;

    /* Get package information from header */
    (void) headerNVR(h, &pkgN, &v, &r);

    t = alloca(21 + strlen(v) + 1 + strlen(r) + 1);
    pkgEVR = t;
    *t = '\0';
    if (hge(h, RPMTAG_EPOCH, NULL, (rpm_data_t *) &epoch, NULL)) {
	sprintf(t, "%d:", *epoch);
	while (*t != '\0')
	    t++;
    }
    (void) stpcpy( stpcpy( stpcpy(t, v) , "-") , r);

    if ((pkg = rpmdsSingle(RPMTAG_PROVIDENAME, pkgN, pkgEVR, pkgFlags)) != NULL) {
	if (nopromote)
	    (void) rpmdsSetNoPromote(pkg, nopromote);
	rc = rpmdsCompare(pkg, req);
	pkg = rpmdsFree(pkg);
    }

    return rc;
}

/**
 */
struct rpmlibProvides_s {
    const char * featureName;
    const char * featureEVR;
    rpmsenseFlags featureFlags;
    const char * featureDescription;
};

static struct rpmlibProvides_s rpmlibProvides[] = {
    { "rpmlib(VersionedDependencies)",	"3.0.3-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("PreReq:, Provides:, and Obsoletes: dependencies support versions.") },
    { "rpmlib(CompressedFileNames)",	"3.0.4-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("file name(s) stored as (dirName,baseName,dirIndex) tuple, not as path.")},
#if HAVE_BZLIB_H
    { "rpmlib(PayloadIsBzip2)",		"3.0.5-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("package payload can be compressed using bzip2.") },
#endif
    { "rpmlib(PayloadFilesHavePrefix)",	"4.0-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("package payload file(s) have \"./\" prefix.") },
    { "rpmlib(ExplicitPackageProvide)",	"4.0-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("package name-version-release is not implicitly provided.") },
    { "rpmlib(HeaderLoadSortsTags)",    "4.0.1-1",
	(                RPMSENSE_EQUAL),
    N_("header tags are always sorted after being loaded.") },
    { "rpmlib(ScriptletInterpreterArgs)",    "4.0.3-1",
	(                RPMSENSE_EQUAL),
    N_("the scriptlet interpreter can use arguments from header.") },
    { "rpmlib(PartialHardlinkSets)",    "4.0.4-1",
	(                RPMSENSE_EQUAL),
    N_("a hardlink file set may be installed without being complete.") },
    { "rpmlib(ConcurrentAccess)",    "4.1-1",
	(                RPMSENSE_EQUAL),
    N_("package scriptlets may access the rpm database while installing.") },
#ifdef WITH_LUA
    { "rpmlib(BuiltinLuaScripts)",    "4.2.2-1",
	(                RPMSENSE_EQUAL),
    N_("internal support for lua scripts.") },
#endif
    { NULL,				NULL, 0,	NULL }
};


int rpmdsRpmlib(rpmds * dsp, void * tblp)
{
    const struct rpmlibProvides_s * rltblp = tblp;
    const struct rpmlibProvides_s * rlp;
    int xx;

    if (rltblp == NULL)
	rltblp = rpmlibProvides;

    for (rlp = rltblp; rlp->featureName != NULL; rlp++) {
	rpmds ds = rpmdsSingle(RPMTAG_PROVIDENAME, rlp->featureName,
			rlp->featureEVR, rlp->featureFlags);
	xx = rpmdsMerge(dsp, ds);
	ds = rpmdsFree(ds);
    }
    return 0;
}

