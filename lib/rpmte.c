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

transactionElement teFree(transactionElement te)
{
    if (te != NULL) {
	memset(te, 0, sizeof(*te));	/* XXX trash and burn */
	te = _free(te);
    }
    return NULL;
}

transactionElement teNew(void)
{
    transactionElement te = xcalloc(1, sizeof(*te));
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

tsortInfo teGetTSI(transactionElement te)
{
    /*@-compdef -retalias -retexpose -usereleased @*/
    return te->tsi;
    /*@=compdef =retalias =retexpose =usereleased @*/
}

alKey teGetAddedKey(transactionElement te)
{
    return (te != NULL ? te->u.addedKey : 0);
}

alKey teGetDependsOnKey(transactionElement te)
{
    return (te != NULL ? te->u.removed.dependsOnKey : 0);
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

int teiGetOc(teIterator tei)
{
    return tei->ocsave;
}

teIterator teFreeIterator(/*@only@*//*@null@*/ teIterator tei)
{
    if (tei)
	tei->ts = rpmtsUnlink(tei->ts, "tsIterator");
    return _free(tei);
}

teIterator teInitIterator(rpmTransactionSet ts)
{
    teIterator tei = NULL;

    tei = xcalloc(1, sizeof(*tei));
    tei->ts = rpmtsLink(ts, "teIterator");
    tei->reverse = ((ts->transFlags & RPMTRANS_FLAG_REVERSE) ? 1 : 0);
    tei->oc = (tei->reverse ? (ts->orderCount - 1) : 0);
    tei->ocsave = tei->oc;
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
	/*@-type@*/
	if (p->type == type)
	    break;
	/*@=type@*/
    }
    return p;
}
