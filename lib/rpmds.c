/** \ingroup rpmdep
 * \file lib/rpmds.c
 */
#include "system.h"

#include <rpm/rpmtypes.h>
#include <rpm/rpmlib.h>		/* rpmvercmp */
#include <rpm/rpmstring.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstrpool.h>

#include "lib/rpmds_internal.h"

#include "debug.h"

int _rpmds_debug = 0;

int _rpmds_nopromote = 1;

/**
 * A package dependency set.
 */
struct rpmds_s {
    rpmstrPool pool;		/*!< String pool. */
    const char * Type;		/*!< Tag name. */
    char * DNEVR;		/*!< Formatted dependency string. */
    rpmsid * N;			/*!< Dependency name id's (pool) */
    rpmsid * EVR;		/*!< Dependency EVR id's (pool) */
    rpmsenseFlags * Flags;	/*!< Bit(s) identifying context/comparison. */
    rpm_color_t * Color;	/*!< Bit(s) calculated from file color(s). */
    rpmTagVal tagN;		/*!< Header tag. */
    int32_t Count;		/*!< No. of elements */
    unsigned int instance;	/*!< From rpmdb instance? */
    int i;			/*!< Element index. */
    unsigned l;			/*!< Low element (bsearch). */
    unsigned u;			/*!< High element (bsearch). */
    int nopromote;		/*!< Don't promote Epoch: in rpmdsCompare()? */
    int nrefs;			/*!< Reference count. */
};

static int dsType(rpmTagVal tag, 
		  const char ** Type, rpmTagVal * tagEVR, rpmTagVal * tagF)
{
    int rc = 0;
    const char *t = NULL;
    rpmTagVal evr = RPMTAG_NOT_FOUND;
    rpmTagVal f = RPMTAG_NOT_FOUND;

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
    } else if (tag == RPMTAG_ORDERNAME) {
	t = "Order";
	evr = RPMTAG_ORDERVERSION;
	f = RPMTAG_ORDERFLAGS;
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

rpmsid rpmdsNIdIndex(rpmds ds, int i)
{
    rpmsid id = 0;
    if (ds != NULL && i >= 0 && i < ds->Count && ds->N != NULL)
	id = ds->N[i];
    return id;
}

rpmsid rpmdsEVRIdIndex(rpmds ds, int i)
{
    rpmsid id = 0;
    if (ds != NULL && i >= 0 && i < ds->Count && ds->EVR != NULL)
	id = ds->EVR[i];
    return id;
}
const char * rpmdsNIndex(rpmds ds, int i)
{
    const char * N = NULL;
    if (ds != NULL && i >= 0 && i < ds->Count && ds->N != NULL)
	N = rpmstrPoolStr(ds->pool, ds->N[i]);
    return N;
}

const char * rpmdsEVRIndex(rpmds ds, int i)
{
    const char * EVR = NULL;
    if (ds != NULL && i >= 0 && i < ds->Count && ds->EVR != NULL)
	EVR = rpmstrPoolStr(ds->pool, ds->EVR[i]);
    return EVR;
}

rpmsenseFlags rpmdsFlagsIndex(rpmds ds, int i)
{
    rpmsenseFlags Flags = 0;
    if (ds != NULL && i >= 0 && i < ds->Count && ds->Flags != NULL)
	Flags = ds->Flags[i];
    return Flags;
}

rpm_color_t rpmdsColorIndex(rpmds ds, int i)
{
    rpm_color_t Color = 0;
    if (ds != NULL && i >= 0 && i < ds->Count && ds->Color != NULL)
	Color = ds->Color[i];
    return Color;
}
static rpmds rpmdsUnlink(rpmds ds)
{
    if (ds)
	ds->nrefs--;
    return NULL;
}

rpmds rpmdsLink(rpmds ds)
{
    if (ds)
	ds->nrefs++;
    return ds;
}

rpmds rpmdsFree(rpmds ds)
{
    rpmTagVal tagEVR, tagF;

    if (ds == NULL)
	return NULL;

    if (ds->nrefs > 1)
	return rpmdsUnlink(ds);

    if (dsType(ds->tagN, NULL, &tagEVR, &tagF))
	return NULL;

    if (ds->Count > 0) {
	ds->N = _free(ds->N);
	ds->EVR = _free(ds->EVR);
	ds->Flags = _free(ds->Flags);
    }

    ds->pool = rpmstrPoolFree(ds->pool);
    ds->DNEVR = _free(ds->DNEVR);
    ds->Color = _free(ds->Color);

    (void) rpmdsUnlink(ds);
    memset(ds, 0, sizeof(*ds));		/* XXX trash and burn */
    ds = _free(ds);
    return NULL;
}

static rpmds rpmdsCreate(rpmstrPool pool,
		  rpmTagVal tagN, const char * Type, int Count,
		  unsigned int instance)
{
    rpmds ds = xcalloc(1, sizeof(*ds));

    ds->pool = (pool != NULL) ? rpmstrPoolLink(pool) : rpmstrPoolCreate();
    ds->tagN = tagN;
    ds->Type = Type;
    ds->Count = Count;
    ds->instance = instance;
    ds->nopromote = _rpmds_nopromote;
    ds->i = -1;

    return rpmdsLink(ds);
}

rpmds rpmdsNewPool(rpmstrPool pool, Header h, rpmTagVal tagN, int flags)
{
    rpmTagVal tagEVR, tagF;
    rpmds ds = NULL;
    const char * Type;
    struct rpmtd_s names;
    if (dsType(tagN, &Type, &tagEVR, &tagF))
	goto exit;

    if (headerGet(h, tagN, &names, HEADERGET_MINMEM)) {
	struct rpmtd_s evr, flags; 

	ds = rpmdsCreate(pool, tagN, Type,
			 rpmtdCount(&names), headerGetInstance(h));

	ds->N = rpmtdToPool(&names, ds->pool);
	headerGet(h, tagEVR, &evr, HEADERGET_MINMEM);
	ds->EVR = rpmtdToPool(&evr, ds->pool);
	headerGet(h, tagF, &flags, HEADERGET_ALLOC);
	ds->Flags = flags.data;
	/* ensure rpmlib() requires always have RPMSENSE_RPMLIB flag set */
	if (tagN == RPMTAG_REQUIRENAME && ds->Flags) {
	    for (int i = 0; i < ds->Count; i++) {
		if (!(rpmdsFlagsIndex(ds, i) & RPMSENSE_RPMLIB)) {
		    const char *N = rpmdsNIndex(ds, i);
		    if (rstreqn(N, "rpmlib(", sizeof("rpmlib(")-1))
			ds->Flags[i] |= RPMSENSE_RPMLIB;
		}
	    }
	}
	rpmtdFreeData(&names);
	rpmtdFreeData(&evr);

	/* freeze the pool to save memory, but only if private pool */
	if (ds->pool != pool)
	    rpmstrPoolFreeze(ds->pool, 0);
    }

exit:
    return ds;
}

rpmds rpmdsNew(Header h, rpmTagVal tagN, int flags)
{
    return rpmdsNewPool(NULL, h, tagN, flags);
}

char * rpmdsNewDNEVR(const char * dspfx, const rpmds ds)
{
    const char * N = rpmdsN(ds);
    const char * EVR = rpmdsEVR(ds);
    rpmsenseFlags Flags = rpmdsFlags(ds);
    char * tbuf, * t;
    size_t nb;

    nb = 0;
    if (dspfx)	nb += strlen(dspfx) + 1;
    if (N)	nb += strlen(N);
    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (Flags & RPMSENSE_SENSEMASK) {
	if (nb)	nb++;
	if (Flags & RPMSENSE_LESS)	nb++;
	if (Flags & RPMSENSE_GREATER) nb++;
	if (Flags & RPMSENSE_EQUAL)	nb++;
    }
    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (EVR && *EVR) {
	if (nb)	nb++;
	nb += strlen(EVR);
    }

    t = tbuf = xmalloc(nb + 1);
    if (dspfx) {
	t = stpcpy(t, dspfx);
	*t++ = ' ';
    }
    if (N)
	t = stpcpy(t, N);
    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (Flags & RPMSENSE_SENSEMASK) {
	if (t != tbuf)	*t++ = ' ';
	if (Flags & RPMSENSE_LESS)	*t++ = '<';
	if (Flags & RPMSENSE_GREATER) *t++ = '>';
	if (Flags & RPMSENSE_EQUAL)	*t++ = '=';
    }
    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (EVR && *EVR) {
	if (t != tbuf)	*t++ = ' ';
	t = stpcpy(t, EVR);
    }
    *t = '\0';
    return tbuf;
}

static rpmds singleDSPool(rpmstrPool pool, rpmTagVal tagN,
			  rpmsid N, rpmsid EVR, rpmsenseFlags Flags,
			  unsigned int instance, rpm_color_t Color)
{
    rpmds ds = NULL;
    const char * Type;

    if (dsType(tagN, &Type, NULL, NULL))
	goto exit;

    ds = rpmdsCreate(pool, tagN, Type, 1, instance);

    ds->N = xmalloc(1 * sizeof(*ds->N));
    ds->N[0] = N;
    ds->EVR = xmalloc(1 * sizeof(*ds->EVR));
    ds->EVR[0] = EVR;
    ds->Flags = xmalloc(sizeof(*ds->Flags));
    ds->Flags[0] = Flags;
    ds->i = 0;
    if (Color)
	rpmdsSetColor(ds, Color);

exit:
    return ds;
}

static rpmds singleDS(rpmstrPool pool, rpmTagVal tagN,
		      const char * N, const char * EVR,
		      rpmsenseFlags Flags, unsigned int instance,
		      rpm_color_t Color)
{
    rpmds ds = singleDSPool(pool, tagN, 0, 0, Flags, instance, Color);
    if (ds) {
	/* now that we have a pool, we can insert our N & EVR strings */
	ds->N[0] = rpmstrPoolId(ds->pool, N ? N : "", 1);
	ds->EVR[0] = rpmstrPoolId(ds->pool, EVR ? EVR : "", 1);
	/* freeze the pool to save memory, but only if private pool */
	if (ds->pool != pool)
	    rpmstrPoolFreeze(ds->pool, 0);
    }
    return ds;
}

rpmds rpmdsThisPool(rpmstrPool pool,
		    Header h, rpmTagVal tagN, rpmsenseFlags Flags)
{
    char *evr = headerGetAsString(h, RPMTAG_EVR);
    rpmds ds = singleDS(pool, tagN, headerGetString(h, RPMTAG_NAME),
			evr, Flags, headerGetInstance(h), 0);
    free(evr);
    return ds;
}

rpmds rpmdsThis(Header h, rpmTagVal tagN, rpmsenseFlags Flags)
{
    return rpmdsThisPool(NULL, h, tagN, Flags);
}

rpmds rpmdsSinglePool(rpmstrPool pool,rpmTagVal tagN,
		      const char * N, const char * EVR, rpmsenseFlags Flags)
{
    return singleDS(pool, tagN, N, EVR, Flags, 0, 0);
}

rpmds rpmdsSingle(rpmTagVal tagN, const char * N, const char * EVR, rpmsenseFlags Flags)
{
    return rpmdsSinglePool(NULL, tagN, N, EVR, Flags);
}

rpmds rpmdsCurrent(rpmds ds)
{
    rpmds cds = NULL;
    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	/* Using parent's pool so we can just use the same id's */
	cds = singleDSPool(ds->pool, ds->tagN, ds->N[ds->i], ds->EVR[ds->i],
			   rpmdsFlags(ds), ds->instance, rpmdsColor(ds));
    }
    return cds;
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
	ds->DNEVR = _free(ds->DNEVR);
    }
    return i;
}

const char * rpmdsDNEVR(const rpmds ds)
{
    const char * DNEVR = NULL;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->DNEVR == NULL) {
	    char t[2] = { ds->Type[0], '\0' };
	    ds->DNEVR = rpmdsNewDNEVR(t, ds);
	}
	DNEVR = ds->DNEVR;
    }
    return DNEVR;
}

rpmsid rpmdsNId(rpmds ds)
{
    return (ds != NULL) ? rpmdsNIdIndex(ds, ds->i) : 0;
}

rpmsid rpmdsEVRId(rpmds ds)
{
    return (ds != NULL) ? rpmdsEVRIdIndex(ds, ds->i) : 0;
}

const char * rpmdsN(const rpmds ds)
{
    return (ds != NULL) ? rpmdsNIndex(ds, ds->i) : NULL;
}

const char * rpmdsEVR(const rpmds ds)
{
    return (ds != NULL) ? rpmdsEVRIndex(ds, ds->i) : NULL;
}

rpmsenseFlags rpmdsFlags(const rpmds ds)
{
    return (ds != NULL) ? rpmdsFlagsIndex(ds, ds->i) : 0;
}

rpmTagVal rpmdsTagN(const rpmds ds)
{
    rpmTagVal tagN = RPMTAG_NOT_FOUND;

    if (ds != NULL)
	tagN = ds->tagN;
    return tagN;
}

unsigned int rpmdsInstance(rpmds ds)
{
    return (ds != NULL) ? ds->instance : 0;
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
    return (ds != NULL) ? rpmdsColorIndex(ds, ds->i) : 0;
}

rpm_color_t rpmdsSetColor(const rpmds ds, rpm_color_t color)
{
    rpm_color_t ocolor = 0;

    if (ds != NULL && ds->i >= 0 && ds->i < ds->Count) {
	if (ds->Color == NULL) {
	    ds->Color = xcalloc(ds->Count, sizeof(*ds->Color));
	}
	ocolor = ds->Color[ds->i];
	ds->Color[ds->i] = color;
    }
    return ocolor;
}

void rpmdsNotify(rpmds ds, const char * where, int rc)
{
    const char *DNEVR;

    if (!rpmIsDebug())
	return;
    if (!(ds != NULL && ds->i >= 0 && ds->i < ds->Count))
	return;
    if (!(ds->Type != NULL && (DNEVR = rpmdsDNEVR(ds)) != NULL))
	return;

    rpmlog(RPMLOG_DEBUG, "%9s: %-45s %-s %s\n", ds->Type,
		(rstreq(DNEVR, "cached") ? DNEVR : DNEVR+2),
		(rc ? _("NO ") : _("YES")),
		(where != NULL ? where : ""));
}

int rpmdsNext(rpmds ds)
{
    int i = -1;

    if (ds != NULL && ++ds->i >= 0) {
	if (ds->i < ds->Count) {
	    i = ds->i;
	    ds->DNEVR = _free(ds->DNEVR);
	} else
	    ds->i = -1;

if (_rpmds_debug  < 0 && i != -1)
fprintf(stderr, "*** ds %p\t%s[%d]: %s\n", ds, (ds->Type ? ds->Type : "?Type?"), i, (ds->DNEVR ? ds->DNEVR : "?DNEVR?"));

    }

    return i;
}

rpmds rpmdsInit(rpmds ds)
{
    if (ds != NULL) {
	ds->i = -1;
	ds->DNEVR = _free(ds->DNEVR);
    }
    return ds;
}

static rpmds rpmdsDup(const rpmds ods)
{
    rpmds ds = rpmdsCreate(ods->pool, ods->tagN, ods->Type,
			   ods->Count, ods->instance);
    size_t nb;
    
    ds->i = ods->i;
    ds->l = ods->l;
    ds->u = ods->u;
    ds->nopromote = ods->nopromote;

    nb = ds->Count * sizeof(*ds->N);
    ds->N = memcpy(xmalloc(nb), ods->N, nb);
    
    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (ods->EVR) {
	nb = ds->Count * sizeof(*ds->EVR);
	ds->EVR = memcpy(xmalloc(nb), ods->EVR, nb);
    }

    if (ods->Flags) {
	nb = ds->Count * sizeof(*ds->Flags);
	ds->Flags = memcpy(xmalloc(nb), ods->Flags, nb);
    }

    return ds;

}

int rpmdsFind(rpmds ds, const rpmds ods)
{
    int comparison;
    const char *N, *ON = rpmdsN(ods);
    const char *EVR, *OEVR = rpmdsEVR(ods);
    rpmsenseFlags Flags, OFlags = rpmdsFlags(ods);
    int rc = -1; /* assume not found */

    if (ds == NULL || ods == NULL)
	return -1;

    ds->l = 0;
    ds->u = ds->Count;
    while (ds->l < ds->u) {
	ds->i = (ds->l + ds->u) / 2;

	N = rpmdsN(ds);
	EVR = rpmdsEVR(ds);
	Flags = rpmdsFlags(ds);

	comparison = strcmp(ON, N);

	/* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
	if (comparison == 0 && OEVR && EVR)
	    comparison = strcmp(OEVR, EVR);
	if (comparison == 0)
	    comparison = OFlags - Flags;

	if (comparison < 0)
	    ds->u = ds->i;
	else if (comparison > 0)
	    ds->l = ds->i + 1;
	else {
	    rc = ds->i;
	    break;
	}
    }
    return rc;
}

int rpmdsMerge(rpmds * dsp, rpmds ods)
{
    rpmds ds;
    int save;
    int ocount;

    if (dsp == NULL || ods == NULL)
	return -1;

    ocount = rpmdsCount(*dsp);

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

    /* Ensure EVR and Flags exist */
    if (ds->EVR == NULL)
	ds->EVR = xcalloc(ds->Count, sizeof(*ds->EVR));
    if (ds->Flags == NULL)
	ds->Flags = xcalloc(ds->Count, sizeof(*ds->Flags));

    /*
     * Add new entries.
     */
    save = ods->i;
    ods = rpmdsInit(ods);
    while (rpmdsNext(ods) >= 0) {
	const char *OEVR;
	/*
	 * If this entry is already present, don't bother.
	 */
	if (rpmdsFind(ds, ods) >= 0)
	    continue;

	/*
	 * Insert new entry. Ensure pool is unfrozen to allow additions.
	 */
	rpmstrPoolUnfreeze(ds->pool);
	ds->N = xrealloc(ds->N, (ds->Count+1) * sizeof(*ds->N));
	if (ds->u < ds->Count) {
	    memmove(ds->N + ds->u + 1, ds->N + ds->u,
		    (ds->Count - ds->u) * sizeof(*ds->N));
	}
	ds->N[ds->u] = rpmstrPoolId(ds->pool, rpmdsN(ods), 1);

	ds->EVR = xrealloc(ds->EVR, (ds->Count+1) * sizeof(*ds->EVR));
	if (ds->u < ds->Count) {
	    memmove(ds->EVR + ds->u + 1, ds->EVR + ds->u,
		    (ds->Count - ds->u) * sizeof(*ds->EVR));
	}
	OEVR = rpmdsEVR(ods);
	ds->EVR[ds->u] = rpmstrPoolId(ds->pool, OEVR ? OEVR : "", 1);

	ds->Flags = xrealloc(ds->Flags, (ds->Count+1) * sizeof(*ds->Flags));
	if (ds->u < ds->Count) {
	    memmove(ds->Flags + ds->u + 1, ds->Flags + ds->u,
		    (ds->Count - ds->u) * sizeof(*ds->Flags));
	}
	ds->Flags[ds->u] = rpmdsFlags(ods);

	ds->i = ds->Count;
	ds->Count++;

    }
    ods->i = save;
    return (ds->Count - ocount);
}


int rpmdsSearch(rpmds ds, rpmds ods)
{
    int comparison;
    int i, l, u;
    const char *ON = rpmdsN(ods);

    if (ds == NULL || ods == NULL)
	return -1;

    /* Binary search to find the [l,u) subset that contains N */
    i = -1;
    l = 0;
    u = ds->Count;
    while (l < u) {
	i = (l + u) / 2;

	comparison = strcmp(ON, rpmdsNIndex(ds, i));

	if (comparison < 0)
	    u = i;
	else if (comparison > 0)
	    l = i + 1;
	else {
	    /* Set l to 1st member of set that contains N. */
	    if (!rstreq(ON, rpmdsNIndex(ds, l)))
		l = i;
	    while (l > 0 && rstreq(ON, rpmdsNIndex(ds, l-1)))
		l--;
	    /* Set u to 1st member of set that does not contain N. */
	    if (u >= ds->Count || !rstreq(ON, rpmdsNIndex(ds, u)))
		u = i;
	    while (++u < ds->Count) {
		if (!rstreq(ON, rpmdsNIndex(ds, u)))
		    break;
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
    while (*s && risdigit(*s)) s++;	/* s points to epoch terminator */
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

static inline int rpmdsCompareEVR(const char *AEVR, uint32_t AFlags,
				  const char *BEVR, uint32_t BFlags,
				  int nopromote)
{
    const char *aE, *aV, *aR, *bE, *bV, *bR;
    char *aEVR = xstrdup(AEVR);
    char *bEVR = xstrdup(BEVR);
    int sense = 0;
    int result = 0;

    parseEVR(aEVR, &aE, &aV, &aR);
    parseEVR(bEVR, &bE, &bV, &bR);

    /* Compare {A,B} [epoch:]version[-release] */
    if (aE && *aE && bE && *bE)
	sense = rpmvercmp(aE, bE);
    else if (aE && *aE && atol(aE) > 0) {
	if (!nopromote) {
	    sense = 0;
	} else
	    sense = 1;
    } else if (bE && *bE && atol(bE) > 0)
	sense = -1;

    if (sense == 0) {
	sense = rpmvercmp(aV, bV);
	if (sense == 0) {
	    if (aR && *aR && bR && *bR) {
		sense = rpmvercmp(aR, bR);
	    } else {
		/* always matches if the side with no release has SENSE_EQUAL */
		if ((aR && *aR && (BFlags & RPMSENSE_EQUAL)) ||
		    (bR && *bR && (AFlags & RPMSENSE_EQUAL))) {
		    aEVR = _free(aEVR);
		    bEVR = _free(bEVR);
		    result = 1;
		    goto exit;
		}
	    }
	}
    }

    /* Detect overlap of {A,B} range. */
    if (sense < 0 && ((AFlags & RPMSENSE_GREATER) || (BFlags & RPMSENSE_LESS))) {
	result = 1;
    } else if (sense > 0 && ((AFlags & RPMSENSE_LESS) || (BFlags & RPMSENSE_GREATER))) {
	result = 1;
    } else if (sense == 0 &&
	(((AFlags & RPMSENSE_EQUAL) && (BFlags & RPMSENSE_EQUAL)) ||
	 ((AFlags & RPMSENSE_LESS) && (BFlags & RPMSENSE_LESS)) ||
	 ((AFlags & RPMSENSE_GREATER) && (BFlags & RPMSENSE_GREATER)))) {
	result = 1;
    }

exit:
    free(aEVR);
    free(bEVR);
    return result;
}

int rpmdsCompareIndex(rpmds A, int aix, rpmds B, int bix)
{
    const char *AEVR, *BEVR;
    rpmsenseFlags AFlags, BFlags;
    int result;

    /* Different names don't overlap. */
    if (!rpmstrPoolStreq(A->pool, rpmdsNIdIndex(A, aix),
			 B->pool, rpmdsNIdIndex(B, bix))) {
	result = 0;
	goto exit;
    }

    /* XXX rpm prior to 3.0.2 did not always supply EVR and Flags. */
    if (!(A->EVR && A->Flags && B->EVR && B->Flags)) {
	result = 1;
	goto exit;
    }

    /* Same name. If either A or B is an existence test, always overlap. */
    AFlags = rpmdsFlagsIndex(A, aix);
    BFlags = rpmdsFlagsIndex(B, bix);
    if (!((AFlags & RPMSENSE_SENSEMASK) && (BFlags & RPMSENSE_SENSEMASK))) {
	result = 1;
	goto exit;
    }

    AEVR = rpmdsEVRIndex(A, aix);
    BEVR = rpmdsEVRIndex(B, bix);
    if (!(AEVR && *AEVR && BEVR && *BEVR)) {
	/* If either EVR is non-existent or empty, always overlap. */
	result = 1;
    } else {
	/* Both AEVR and BEVR exist, compare [epoch:]version[-release]. */
	result = rpmdsCompareEVR(AEVR, AFlags, BEVR, BFlags, B->nopromote);
    }

exit:
    return result;
}

int rpmdsCompare(const rpmds A, const rpmds B)
{
    return rpmdsCompareIndex(A, A->i, B, B->i);
}

int rpmdsMatches(rpmstrPool pool, Header h, int prix,
		 rpmds req, int selfevr, int nopromote)
{
    rpmds provides;
    rpmTagVal tag = RPMTAG_PROVIDENAME;
    int result = 0;

    /* Get provides information from header */
    if (selfevr)
	provides = rpmdsThisPool(pool, h, tag, RPMSENSE_EQUAL);
    else
	provides = rpmdsNewPool(pool, h, tag, 0);

    rpmdsSetNoPromote(provides, nopromote);

    /*
     * For a self-provide and indexed provide, we only need one comparison.
     * Otherwise loop through the provides until match or end.
     */
    if (prix >= 0 || selfevr) {
	if (prix >= 0)
	    rpmdsSetIx(provides, prix);
	result = rpmdsCompare(provides, req);
    } else {
	provides = rpmdsInit(provides);
	while (rpmdsNext(provides) >= 0) {
	    result = rpmdsCompare(provides, req);
	    /* If this provide matches the require, we're done. */
	    if (result)
		break;
	}
    }

    rpmdsFree(provides);
    return result;
}

int rpmdsMatchesDep (const Header h, int ix, const rpmds req, int nopromote)
{
    return rpmdsMatches(NULL, h, ix, req, 0, nopromote);
}

int rpmdsAnyMatchesDep (const Header h, const rpmds req, int nopromote)
{
    return rpmdsMatches(NULL, h, -1, req, 0, nopromote);
}

int rpmdsNVRMatchesDep(const Header h, const rpmds req, int nopromote)
{
    return rpmdsMatches(NULL, h, -1, req, 1, nopromote);
}

/**
 */
struct rpmlibProvides_s {
    const char * featureName;
    const char * featureEVR;
    rpmsenseFlags featureFlags;
    const char * featureDescription;
};

static const struct rpmlibProvides_s rpmlibProvides[] = {
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
#if HAVE_LZMA_H
    { "rpmlib(PayloadIsXz)",		"5.2-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("package payload can be compressed using xz.") },
    { "rpmlib(PayloadIsLzma)",		"4.4.2-1",
	(RPMSENSE_RPMLIB|RPMSENSE_EQUAL),
    N_("package payload can be compressed using lzma.") },
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
    { "rpmlib(FileDigests)", 		"4.6.0-1",
	(		 RPMSENSE_EQUAL),
    N_("file digest algorithm is per package configurable") },
#ifdef WITH_CAP
    { "rpmlib(FileCaps)", 		"4.6.1-1",
	(		 RPMSENSE_EQUAL),
    N_("support for POSIX.1e file capabilities") },
#endif
    { "rpmlib(ScriptletExpansion)",    "4.9.0-1",
	(		RPMSENSE_EQUAL),
    N_("package scriptlets can be expanded at install time.") },
    { "rpmlib(TildeInVersions)",    "4.10.0-1",
	(		RPMSENSE_EQUAL),
    N_("dependency comparison supports versions with tilde.") },
    { NULL,				NULL, 0,	NULL }
};


int rpmdsRpmlibPool(rpmstrPool pool, rpmds * dsp, const void * tblp)
{
    const struct rpmlibProvides_s * rltblp = tblp;
    const struct rpmlibProvides_s * rlp;
    int rc = 0;

    if (rltblp == NULL)
	rltblp = rpmlibProvides;

    for (rlp = rltblp; rlp->featureName != NULL && rc >= 0; rlp++) {
	rpmds ds = rpmdsSinglePool(pool, RPMTAG_PROVIDENAME, rlp->featureName,
			rlp->featureEVR, rlp->featureFlags);
	rc = rpmdsMerge(dsp, ds);
	rpmdsFree(ds);
    }
    /* freeze the pool to save memory, but only if private pool */
    if (*dsp && (*dsp)->pool != pool)
	rpmstrPoolFreeze((*dsp)->pool, 0);
    return (rc < 0) ? -1 : 0;
}

int rpmdsRpmlib(rpmds * dsp, const void * tblp)
{
    return rpmdsRpmlibPool(NULL, dsp, tblp);
}

rpmstrPool rpmdsPool(rpmds ds)
{
    return (ds != NULL) ? ds->pool : NULL;
}
