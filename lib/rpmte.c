/** \ingroup rpmdep
 * \file lib/rpmte.c
 * Routine to handle a transactionElement.
 */
#include "system.h"

#define	_NEED_TEITERATOR	1
#include "psm.h"

#include "debug.h"

/*@unchecked@*/
int _te_debug = 0;

/*@access alKey @*/
/*@access teIterator @*/
/*@access transactionElement @*/
/*@access rpmTransactionSet @*/

/**
 */
static void delTE(transactionElement p)
	/*@modifies p @*/
{
    rpmRelocation * r;

    if (p->relocs) {
	for (r = p->relocs; (r->oldPath || r->newPath); r++) {
	    r->oldPath = _free(r->oldPath);
	    r->newPath = _free(r->newPath);
	}
	p->relocs = _free(p->relocs);
    }

    p->this = dsFree(p->this);
    p->provides = dsFree(p->provides);
    p->requires = dsFree(p->requires);
    p->conflicts = dsFree(p->conflicts);
    p->obsoletes = dsFree(p->obsoletes);
    p->fi = fiFree(p->fi, 1);

    /*@-noeffectuncon@*/
    if (p->fd != NULL)
        p->fd = fdFree(p->fd, "delTE");
    /*@=noeffectuncon@*/

    p->os = _free(p->os);
    p->arch = _free(p->arch);
    p->epoch = _free(p->epoch);
    p->name = _free(p->name);
    p->NEVR = _free(p->NEVR);

    p->h = headerFree(p->h, "delTE");

    /*@-abstract@*/
    memset(p, 0, sizeof(*p));	/* XXX trash and burn */
    /*@=abstract@*/
    /*@-nullstate@*/ /* FIX: p->{NEVR,name} annotations */
    return;
    /*@=nullstate@*/
}

/**
 */
static void addTE(rpmTransactionSet ts, transactionElement p, Header h,
		/*@dependent@*/ /*@null@*/ fnpyKey key,
		/*@null@*/ rpmRelocation * relocs)
	/*@modifies ts, p, h @*/
{
    int scareMem = 0;
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    int_32 * ep;
    const char * arch, * os;
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

    ep = NULL;
    xx = hge(h, RPMTAG_EPOCH, NULL, (void **)&ep, NULL);
    /*@-branchstate@*/
    if (ep) {
	p->epoch = xmalloc(20);
	sprintf(p->epoch, "%d", *ep);
    } else
	p->epoch = NULL;
    /*@=branchstate@*/

    p->this = dsThis(h, RPMTAG_PROVIDENAME, RPMSENSE_EQUAL);
    p->provides = dsNew(h, RPMTAG_PROVIDENAME, scareMem);
    p->fi = fiNew(ts, NULL, h, RPMTAG_BASENAMES, scareMem);
    p->requires = dsNew(h, RPMTAG_REQUIRENAME, scareMem);
    p->conflicts = dsNew(h, RPMTAG_CONFLICTNAME, scareMem);
    p->obsoletes = dsNew(h, RPMTAG_OBSOLETENAME, scareMem);

    p->key = key;

    p->fd = NULL;

    p->multiLib = 0;

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
}

transactionElement teFree(transactionElement te)
{
    if (te != NULL) {
	delTE(te);
	memset(te, 0, sizeof(*te));	/* XXX trash and burn */
	te = _free(te);
    }
    return NULL;
}

transactionElement teNew(const rpmTransactionSet ts, Header h,
		rpmTransactionType type,
		fnpyKey key,
		rpmRelocation * relocs,
		int dboffset,
		alKey pkgKey)
{
    transactionElement te = xcalloc(1, sizeof(*te));

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

rpmTransactionType teGetType(transactionElement te)
{
    return te->type;
}

const char * teGetN(transactionElement te)
	/*@*/
{
    return (te != NULL ? te->name : NULL);
}

const char * teGetE(transactionElement te)
{
    return (te != NULL ? te->epoch : NULL);
}

const char * teGetV(transactionElement te)
{
    return (te != NULL ? te->version : NULL);
}

const char * teGetR(transactionElement te)
{
    return (te != NULL ? te->release : NULL);
}

const char * teGetA(transactionElement te)
{
    return (te != NULL ? te->arch : NULL);
}

const char * teGetO(transactionElement te)
{
    return (te != NULL ? te->os : NULL);
}

int teGetMultiLib(transactionElement te)
{
    return (te != NULL ? te->multiLib : 0);
}

int teSetMultiLib(transactionElement te, int nmultiLib)
{
    int omultiLib = 0;
    if (te != NULL) {
	omultiLib = te->multiLib;
	te->multiLib = nmultiLib;
    }
    return omultiLib;
}

int teGetDepth(transactionElement te)
{
    return (te != NULL ? te->depth : 0);
}

int teSetDepth(transactionElement te, int ndepth)
{
    int odepth = 0;
    if (te != NULL) {
	odepth = te->depth;
	te->depth = ndepth;
    }
    return odepth;
}

int teGetNpreds(transactionElement te)
{
    return (te != NULL ? te->npreds : 0);
}

int teSetNpreds(transactionElement te, int npreds)
{
    int opreds = 0;
    if (te != NULL) {
	opreds = te->npreds;
	te->npreds = npreds;
    }
    return opreds;
}

tsortInfo teGetTSI(transactionElement te)
{
    /*@-compdef -retalias -retexpose -usereleased @*/
    return te->tsi;
    /*@=compdef =retalias =retexpose =usereleased @*/
}

void teFreeTSI(transactionElement te)
{
    if (te != NULL && teGetTSI(te) != NULL) {
	tsortInfo tsi;

	/* Clean up tsort remnants (if any). */
	while ((tsi = teGetTSI(te)->tsi_next) != NULL) {
	    teGetTSI(te)->tsi_next = tsi->tsi_next;
	    tsi->tsi_next = NULL;
	    tsi = _free(tsi);
	}
	te->tsi = _free(te->tsi);
    }
    /*@-nullstate@*/ /* FIX: te->tsi is NULL */
    return;
    /*@=nullstate@*/
}

void teNewTSI(transactionElement te)
{
    if (te != NULL) {
	teFreeTSI(te);
	te->tsi = xcalloc(1, sizeof(*te->tsi));
    }
}

void teCleanDS(transactionElement te)
{
    te->this = dsFree(te->this);
    te->provides = dsFree(te->provides);
    te->requires = dsFree(te->requires);
    te->conflicts = dsFree(te->conflicts);
    te->obsoletes = dsFree(te->obsoletes);
}

alKey teGetAddedKey(transactionElement te)
{
    return (te != NULL ? te->u.addedKey : RPMAL_NOMATCH);
}

alKey teSetAddedKey(transactionElement te, alKey npkgKey)
{
    alKey opkgKey = RPMAL_NOMATCH;
    if (te != NULL) {
	opkgKey = te->u.addedKey;
	te->u.addedKey = npkgKey;
    }
    return opkgKey;
}


alKey teGetDependsOnKey(transactionElement te)
{
    return (te != NULL ? te->u.removed.dependsOnKey : RPMAL_NOMATCH);
}

int teGetDBOffset(transactionElement te)
{
    return (te != NULL ? te->u.removed.dboffset : 0);
}

const char * teGetNEVR(transactionElement te)
{
    return (te != NULL ? te->NEVR : NULL);
}

FD_t teGetFd(transactionElement te)
{
    /*@-compdef -refcounttrans -retalias -retexpose -usereleased @*/
    return (te != NULL ? te->fd : NULL);
    /*@=compdef =refcounttrans =retalias =retexpose =usereleased @*/
}

fnpyKey teGetKey(transactionElement te)
{
    return (te != NULL ? te->key : NULL);
}

rpmDepSet teGetDS(transactionElement te, rpmTag tag)
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

TFI_t teGetFI(transactionElement te, rpmTag tag)
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

int teiGetOc(teIterator tei)
{
    return tei->ocsave;
}

teIterator XteFreeIterator(/*@only@*//*@null@*/ teIterator tei,
		const char * fn, unsigned int ln)
{
    if (tei)
	tei->ts = rpmtsUnlink(tei->ts, "tsIterator");
if (_te_debug)
fprintf(stderr, "*** tei %p -- %s:%d\n", tei, fn, ln);
    return _free(tei);
}

teIterator XteInitIterator(rpmTransactionSet ts,
		const char * fn, unsigned int ln)
{
    teIterator tei = NULL;

    tei = xcalloc(1, sizeof(*tei));
    tei->ts = rpmtsLink(ts, "teIterator");
    tei->reverse = ((ts->transFlags & RPMTRANS_FLAG_REVERSE) ? 1 : 0);
    tei->oc = (tei->reverse ? (ts->orderCount - 1) : 0);
    tei->ocsave = tei->oc;
if (_te_debug)
fprintf(stderr, "*** tei %p ++ %s:%d\n", tei, fn, ln);
    return tei;
}

transactionElement teNextIterator(teIterator tei)
{
    transactionElement te = NULL;
    int oc = -1;

    if (tei->reverse) {
	if (tei->oc >= 0)			oc = tei->oc--;
    } else {
    	if (tei->oc < tei->ts->orderCount)	oc = tei->oc++;
    }
    tei->ocsave = oc;
    /*@-abstract @*/
    if (oc != -1)
	te = tei->ts->order[oc];
    /*@=abstract @*/
    /*@-compdef -usereleased@*/ /* FIX: ts->order may be released */
    return te;
    /*@=compdef =usereleased@*/
}

transactionElement teNext(teIterator tei, rpmTransactionType type)
{
    transactionElement p;

    while ((p = teNextIterator(tei)) != NULL) {
	if (type == 0 || (p->type & type) != 0)
	    break;
    }
    return p;
}
