/** \ingroup rpmdep
 * \file lib/rpmte.c
 * Routine(s) to handle an "rpmte"  transaction element.
 */
#include "system.h"
#include <rpmlib.h>

#include "psm.h"

#include "rpmds.h"
#include "rpmfi.h"

#define	_RPMTE_INTERNAL
#include "rpmte.h"
#include "rpmts.h"

#include "debug.h"

/*@unchecked@*/
int _rpmte_debug = 0;

/*@access alKey @*/
/*@access rpmtsi @*/
/*@access rpmts @*/

void rpmteCleanDS(rpmte te)
{
    te->this = rpmdsFree(te->this);
    te->provides = rpmdsFree(te->provides);
    te->requires = rpmdsFree(te->requires);
    te->conflicts = rpmdsFree(te->conflicts);
    te->obsoletes = rpmdsFree(te->obsoletes);
}

/**
 * Destroy transaction element data.
 * @param p		transaction element
 */
static void delTE(rpmte p)
	/*@globals fileSystem @*/
	/*@modifies p, fileSystem @*/
{
    rpmRelocation * r;

    if (p->relocs) {
	for (r = p->relocs; (r->oldPath || r->newPath); r++) {
	    r->oldPath = _free(r->oldPath);
	    r->newPath = _free(r->newPath);
	}
	p->relocs = _free(p->relocs);
    }

    rpmteCleanDS(p);

    p->fi = rpmfiFree(p->fi);

    /*@-noeffectuncon@*/
    if (p->fd != NULL)
        p->fd = fdFree(p->fd, "delTE");
    /*@=noeffectuncon@*/

    p->os = _free(p->os);
    p->arch = _free(p->arch);
    p->epoch = _free(p->epoch);
    p->name = _free(p->name);
    p->NEVR = _free(p->NEVR);
    p->NEVRA = _free(p->NEVRA);

    p->h = headerFree(p->h);

/*@-boundswrite@*/
    memset(p, 0, sizeof(*p));	/* XXX trash and burn */
/*@=boundswrite@*/
    /*@-nullstate@*/ /* FIX: p->{NEVR,name} annotations */
    return;
    /*@=nullstate@*/
}

/**
 * Initialize transaction element data from header.
 * @param ts		transaction set
 * @param p		transaction element
 * @param h		header
 * @param key		(TR_ADDED) package retrieval key (e.g. file name)
 * @param relocs	(TR_ADDED) package file relocations
 */
/*@-bounds@*/
static void addTE(rpmts ts, rpmte p, Header h,
		/*@dependent@*/ /*@null@*/ fnpyKey key,
		/*@null@*/ rpmRelocation * relocs)
	/*@globals fileSystem @*/
	/*@modifies ts, p, h, fileSystem @*/
{
    int scareMem = 0;
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    rpmte savep;
    int_32 * ep;
    const char * arch, * os;
    char * t;
    size_t nb;
    int xx;

    p->NEVR = hGetNEVR(h, NULL);
    p->name = xstrdup(p->NEVR);
    if ((p->release = strrchr(p->name, '-')) != NULL)
	*p->release++ = '\0';
    if ((p->version = strrchr(p->name, '-')) != NULL)
	*p->version++ = '\0';

    arch = NULL;
    xx = hge(h, RPMTAG_ARCH, NULL, (void **)&arch, NULL);
    p->arch = (arch != NULL ? xstrdup(arch) : NULL);
    os = NULL;
    xx = hge(h, RPMTAG_OS, NULL, (void **)&os, NULL);
    p->os = (os != NULL ? xstrdup(os) : NULL);

    nb = strlen(p->NEVR) + 1;
    if (p->arch)
	nb += strlen(p->arch) + 1;
    t = xmalloc(nb);
    p->NEVRA = t;
    *t = '\0';
    t = stpcpy(t, p->NEVR);
    if (p->arch)
	t = stpcpy( stpcpy( t, "."), p->arch);

    ep = NULL;
    xx = hge(h, RPMTAG_EPOCH, NULL, (void **)&ep, NULL);
/*@-branchstate@*/
    if (ep) {
	p->epoch = xmalloc(20);
	sprintf(p->epoch, "%d", *ep);
    } else
	p->epoch = NULL;
/*@=branchstate@*/

    if (relocs != NULL) {
	rpmRelocation * r;
	int i;

	for (i = 0, r = relocs; r->oldPath || r->newPath; i++, r++)
	    {};
	p->relocs = xmalloc((i + 1) * sizeof(*p->relocs));

	for (i = 0, r = relocs; r->oldPath || r->newPath; i++, r++) {
	    p->relocs[i].oldPath = r->oldPath ? xstrdup(r->oldPath) : NULL;
	    p->relocs[i].newPath = r->newPath ? xstrdup(r->newPath) : NULL;
	}
	p->relocs[i].oldPath = NULL;
	p->relocs[i].newPath = NULL;
    } else {
	p->relocs = NULL;
    }

    p->key = key;
    p->fd = NULL;

    p->this = rpmdsThis(h, RPMTAG_PROVIDENAME, RPMSENSE_EQUAL);
    p->provides = rpmdsNew(h, RPMTAG_PROVIDENAME, scareMem);
    p->requires = rpmdsNew(h, RPMTAG_REQUIRENAME, scareMem);
    p->conflicts = rpmdsNew(h, RPMTAG_CONFLICTNAME, scareMem);
    p->obsoletes = rpmdsNew(h, RPMTAG_OBSOLETENAME, scareMem);

    savep = rpmtsSetRelocateElement(ts, p);
    p->fi = rpmfiNew(ts, h, RPMTAG_BASENAMES, scareMem);
    (void) rpmtsSetRelocateElement(ts, savep);

    rpmteColorDS(p, RPMTAG_PROVIDENAME);
    rpmteColorDS(p, RPMTAG_REQUIRENAME);
/*@-compdef@*/
    return;
/*@=compdef@*/
}
/*@=bounds@*/

rpmte rpmteFree(rpmte te)
{
    if (te != NULL) {
	delTE(te);
	memset(te, 0, sizeof(*te));	/* XXX trash and burn */
	te = _free(te);
    }
    return NULL;
}

rpmte rpmteNew(const rpmts ts, Header h,
		rpmElementType type,
		fnpyKey key,
		rpmRelocation * relocs,
		int dboffset,
		alKey pkgKey)
{
    rpmte te = xcalloc(1, sizeof(*te));

    addTE(ts, te, h, key, relocs);
    switch (type) {
    case TR_ADDED:
	te->type = type;
	te->u.addedKey = pkgKey;
	break;
    case TR_REMOVED:
	te->type = type;
	te->u.removed.dependsOnKey = pkgKey;
	te->u.removed.dboffset = dboffset;
	break;
    }
    return te;
}

rpmElementType rpmteType(rpmte te)
{
    return (te != NULL ? te->type : -1);
}

const char * rpmteN(rpmte te)
{
    return (te != NULL ? te->name : NULL);
}

const char * rpmteE(rpmte te)
{
    return (te != NULL ? te->epoch : NULL);
}

const char * rpmteV(rpmte te)
{
    return (te != NULL ? te->version : NULL);
}

const char * rpmteR(rpmte te)
{
    return (te != NULL ? te->release : NULL);
}

const char * rpmteA(rpmte te)
{
    return (te != NULL ? te->arch : NULL);
}

const char * rpmteO(rpmte te)
{
    return (te != NULL ? te->os : NULL);
}

uint_32 rpmteColor(rpmte te)
{
    return (te != NULL ? te->color : 0);
}

uint_32 rpmteSetColor(rpmte te, uint_32 color)
{
    int ocolor = 0;
    if (te != NULL) {
	ocolor = te->color;
	te->color = color;
    }
    return ocolor;
}

int rpmteDepth(rpmte te)
{
    return (te != NULL ? te->depth : 0);
}

int rpmteSetDepth(rpmte te, int ndepth)
{
    int odepth = 0;
    if (te != NULL) {
	odepth = te->depth;
	te->depth = ndepth;
    }
    return odepth;
}

int rpmteNpreds(rpmte te)
{
    return (te != NULL ? te->npreds : 0);
}

int rpmteSetNpreds(rpmte te, int npreds)
{
    int opreds = 0;
    if (te != NULL) {
	opreds = te->npreds;
	te->npreds = npreds;
    }
    return opreds;
}

int rpmteTree(rpmte te)
{
    return (te != NULL ? te->tree : 0);
}

int rpmteSetTree(rpmte te, int ntree)
{
    int otree = 0;
    if (te != NULL) {
	otree = te->tree;
	te->tree = ntree;
    }
    return otree;
}

rpmte rpmteParent(rpmte te)
{
    return (te != NULL ? te->parent : NULL);
}

rpmte rpmteSetParent(rpmte te, rpmte pte)
{
    rpmte opte = NULL;
/*@-branchstate@*/
    if (te != NULL) {
	opte = te->parent;
	/*@-assignexpose -temptrans@*/
	te->parent = pte;
	/*@=assignexpose =temptrans@*/
    }
/*@=branchstate@*/
    return opte;
}

int rpmteDegree(rpmte te)
{
    return (te != NULL ? te->degree : 0);
}

int rpmteSetDegree(rpmte te, int ndegree)
{
    int odegree = 0;
    if (te != NULL) {
	odegree = te->degree;
	te->degree = ndegree;
    }
    return odegree;
}

tsortInfo rpmteTSI(rpmte te)
{
    /*@-compdef -retalias -retexpose -usereleased @*/
    return te->tsi;
    /*@=compdef =retalias =retexpose =usereleased @*/
}

void rpmteFreeTSI(rpmte te)
{
    if (te != NULL && rpmteTSI(te) != NULL) {
	tsortInfo tsi;

	/* Clean up tsort remnants (if any). */
	while ((tsi = rpmteTSI(te)->tsi_next) != NULL) {
	    rpmteTSI(te)->tsi_next = tsi->tsi_next;
	    tsi->tsi_next = NULL;
	    tsi = _free(tsi);
	}
	te->tsi = _free(te->tsi);
    }
    /*@-nullstate@*/ /* FIX: te->tsi is NULL */
    return;
    /*@=nullstate@*/
}

void rpmteNewTSI(rpmte te)
{
    if (te != NULL) {
	rpmteFreeTSI(te);
	te->tsi = xcalloc(1, sizeof(*te->tsi));
    }
}

alKey rpmteAddedKey(rpmte te)
{
    return (te != NULL ? te->u.addedKey : RPMAL_NOMATCH);
}

alKey rpmteSetAddedKey(rpmte te, alKey npkgKey)
{
    alKey opkgKey = RPMAL_NOMATCH;
    if (te != NULL) {
	opkgKey = te->u.addedKey;
	te->u.addedKey = npkgKey;
    }
    return opkgKey;
}


alKey rpmteDependsOnKey(rpmte te)
{
    return (te != NULL ? te->u.removed.dependsOnKey : RPMAL_NOMATCH);
}

int rpmteDBOffset(rpmte te)
{
    return (te != NULL ? te->u.removed.dboffset : 0);
}

const char * rpmteNEVR(rpmte te)
{
    return (te != NULL ? te->NEVR : NULL);
}

const char * rpmteNEVRA(rpmte te)
{
    return (te != NULL ? te->NEVRA : NULL);
}

FD_t rpmteFd(rpmte te)
{
    /*@-compdef -refcounttrans -retalias -retexpose -usereleased @*/
    return (te != NULL ? te->fd : NULL);
    /*@=compdef =refcounttrans =retalias =retexpose =usereleased @*/
}

fnpyKey rpmteKey(rpmte te)
{
    return (te != NULL ? te->key : NULL);
}

rpmds rpmteDS(rpmte te, rpmTag tag)
{
    /*@-compdef -refcounttrans -retalias -retexpose -usereleased @*/
    if (te == NULL)
	return NULL;

    if (tag == RPMTAG_NAME)
	return te->this;
    else
    if (tag == RPMTAG_PROVIDENAME)
	return te->provides;
    else
    if (tag == RPMTAG_REQUIRENAME)
	return te->requires;
    else
    if (tag == RPMTAG_CONFLICTNAME)
	return te->conflicts;
    else
    if (tag == RPMTAG_OBSOLETENAME)
	return te->obsoletes;
    else
	return NULL;
    /*@=compdef =refcounttrans =retalias =retexpose =usereleased @*/
}

rpmfi rpmteFI(rpmte te, rpmTag tag)
{
    /*@-compdef -refcounttrans -retalias -retexpose -usereleased @*/
    if (te == NULL)
	return NULL;

    if (tag == RPMTAG_BASENAMES)
	return te->fi;
    else
	return NULL;
    /*@=compdef =refcounttrans =retalias =retexpose =usereleased @*/
}

void rpmteColorDS(rpmte te, rpmTag tag)
{
    rpmfi fi = rpmteFI(te, RPMTAG_BASENAMES);
    rpmds ds = rpmteDS(te, tag);
    char deptype = 'R';
    char mydt;
    const int_32 * ddict;
    int_32 * colors;
    int_32 * refs;
    int_32 val;
    int Count;
    size_t nb;
    unsigned ix;
    int ndx, i;

    if (!(te && (Count = rpmdsCount(ds)) > 0 && rpmfiFC(fi) > 0))
	return;

    switch (tag) {
    default:
	return;
	/*@notreached@*/ break;
    case RPMTAG_PROVIDENAME:
	deptype = 'P';
	break;
    case RPMTAG_REQUIRENAME:
	deptype = 'R';
	break;
    }

    nb = Count * sizeof(*colors);
    colors = memset(alloca(nb), 0, nb);
    nb = Count * sizeof(*refs);
    refs = memset(alloca(nb), -1, nb);

    /* Calculate dependency color and reference count. */
    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while (rpmfiNext(fi) >= 0) {
	val = rpmfiFColor(fi);
	ddict = NULL;
	ndx = rpmfiFDepends(fi, &ddict);
	if (ddict != NULL)
	while (ndx-- > 0) {
	    ix = *ddict++;
	    mydt = ((ix >> 24) & 0xff);
	    if (mydt != deptype)
		/*@innercontinue@*/ continue;
	    ix &= 0x00ffffff;
assert (ix < Count);
	    colors[ix] |= val;
	    refs[ix]++;
	}
    }

    /* Set color/refs values in dependency set. */
    ds = rpmdsInit(ds);
    while ((i = rpmdsNext(ds)) >= 0) {
	val = colors[i];
	te->color |= val;
	(void) rpmdsSetColor(ds, val);
	val = refs[i];
	if (val >= 0)
	    val++;
	(void) rpmdsSetRefs(ds, val);
    }
}

int rpmtsiOc(rpmtsi tsi)
{
    return tsi->ocsave;
}

rpmtsi XrpmtsiFree(/*@only@*//*@null@*/ rpmtsi tsi,
		const char * fn, unsigned int ln)
{
    /* XXX watchout: a funky recursion segfaults here iff nrefs is wrong. */
    if (tsi)
	tsi->ts = rpmtsFree(tsi->ts);

/*@-modfilesys@*/
if (_rpmte_debug)
fprintf(stderr, "*** tsi %p -- %s:%d\n", tsi, fn, ln);
/*@=modfilesys@*/
    return _free(tsi);
}

rpmtsi XrpmtsiInit(rpmts ts, const char * fn, unsigned int ln)
{
    rpmtsi tsi = NULL;

    tsi = xcalloc(1, sizeof(*tsi));
    tsi->ts = rpmtsLink(ts, "rpmtsi");
    tsi->reverse = ((rpmtsFlags(ts) & RPMTRANS_FLAG_REVERSE) ? 1 : 0);
    tsi->oc = (tsi->reverse ? (rpmtsNElements(ts) - 1) : 0);
    tsi->ocsave = tsi->oc;
/*@-modfilesys@*/
if (_rpmte_debug)
fprintf(stderr, "*** tsi %p ++ %s:%d\n", tsi, fn, ln);
/*@=modfilesys@*/
    return tsi;
}

/**
 * Return next transaction element.
 * @param tsi		transaction element iterator
 * @return		transaction element, NULL on termination
 */
static /*@dependent@*/ /*@null@*/
rpmte rpmtsiNextElement(rpmtsi tsi)
	/*@modifies tsi @*/
{
    rpmte te = NULL;
    int oc = -1;

    if (tsi == NULL || tsi->ts == NULL || rpmtsNElements(tsi->ts) <= 0)
	return te;

    if (tsi->reverse) {
	if (tsi->oc >= 0)		oc = tsi->oc--;
    } else {
    	if (tsi->oc < rpmtsNElements(tsi->ts))	oc = tsi->oc++;
    }
    tsi->ocsave = oc;
/*@-branchstate@*/
    if (oc != -1)
	te = rpmtsElement(tsi->ts, oc);
/*@=branchstate@*/
    return te;
}

rpmte rpmtsiNext(rpmtsi tsi, rpmElementType type)
{
    rpmte te;

    while ((te = rpmtsiNextElement(tsi)) != NULL) {
	if (type == 0 || (te->type & type) != 0)
	    break;
    }
    return te;
}
