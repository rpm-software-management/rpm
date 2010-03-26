/**
 * \file lib/rpmps.c
 */

#include "system.h"

#include <inttypes.h>
#include <stdlib.h>

#include <rpm/rpmstring.h>
#include <rpm/rpmps.h>

#include "debug.h"

struct rpmps_s {
    int numProblems;		/*!< Current probs array size. */
    int numProblemsAlloced;	/*!< Allocated probs array size. */
    rpmProblem *probs;		/*!< Array of pointers to specific problems. */
    int nrefs;			/*!< Reference count. */
};

struct rpmpsi_s {
    int ix;
    rpmps ps;
};


rpmps rpmpsUnlink(rpmps ps)
{
    if (ps) {
	ps->nrefs--;
    }
    return NULL;
}

rpmps rpmpsLink(rpmps ps)
{
    if (ps) {
	ps->nrefs++;
    }
    return ps;
}

int rpmpsNumProblems(rpmps ps)
{
    int numProblems = 0;
    if (ps && ps->probs)
	numProblems = ps->numProblems;
    return numProblems;
}

rpmpsi rpmpsInitIterator(rpmps ps)
{
    rpmpsi psi = NULL;
    if (ps != NULL && ps->numProblems > 0) {
	psi = xcalloc(1, sizeof(*psi));
	psi->ps = rpmpsLink(ps);
	psi->ix = -1;
    }
    return psi;
}

rpmpsi rpmpsFreeIterator(rpmpsi psi)
{
    if (psi != NULL) {
	rpmpsUnlink(psi->ps);
	free(psi);
    }
    return NULL;
}

rpmProblem rpmpsiNext(rpmpsi psi)
{
    rpmProblem p = NULL;
    if (psi != NULL && psi->ps != NULL && ++psi->ix >= 0) {
	rpmps ps = psi->ps;
	if (psi->ix < ps->numProblems) {
	    p = ps->probs[psi->ix];
	} else {
	    psi->ix = -1;
	}
    }
    return p;
}

int rpmpsNextIterator(rpmpsi psi)
{
    return (rpmpsiNext(psi) != NULL) ? psi->ix : -1;
}

rpmProblem rpmpsGetProblem(rpmpsi psi)
{
    rpmProblem p = NULL;
    if (psi != NULL && psi->ix >= 0 && psi->ix < rpmpsNumProblems(psi->ps)) {
	p = psi->ps->probs[psi->ix];
    } 
    return p;
}

rpmps rpmpsCreate(void)
{
    rpmps ps = xcalloc(1, sizeof(*ps));
    return rpmpsLink(ps);
}

rpmps rpmpsFree(rpmps ps)
{
    if (ps == NULL) return NULL;
    if (ps->nrefs > 1) {
	return rpmpsUnlink(ps);
    }
	
    if (ps->probs) {
	rpmpsi psi = rpmpsInitIterator(ps);
	while (rpmpsNextIterator(psi) >= 0) {
	    rpmProblemFree(rpmpsGetProblem(psi));	
	}
	rpmpsFreeIterator(psi);
	ps->probs = _free(ps->probs);
    }
    ps = _free(ps);
    return NULL;
}

void rpmpsAppendProblem(rpmps ps, rpmProblem prob)
{
    if (ps == NULL || prob == NULL) return;

    if (ps->numProblems == ps->numProblemsAlloced) {
	if (ps->numProblemsAlloced)
	    ps->numProblemsAlloced *= 2;
	else
	    ps->numProblemsAlloced = 2;
	ps->probs = xrealloc(ps->probs,
			ps->numProblemsAlloced * sizeof(ps->probs));
    }

    ps->probs[ps->numProblems] = rpmProblemLink(prob);
    ps->numProblems++;
}

void rpmpsAppend(rpmps ps, rpmProblemType type,
		const char * pkgNEVR, fnpyKey key,
		const char * dn, const char * bn,
		const char * altNEVR, uint64_t number)
{
    rpmProblem p = NULL;
    if (ps == NULL) return;

    p = rpmProblemCreate(type, pkgNEVR, key, dn, bn, altNEVR, number);
    rpmpsAppendProblem(ps, p);
    rpmProblemFree(p);
}

/* XXX TODO: implement with iterators */
int rpmpsTrim(rpmps ps, rpmps filter)
{
    rpmProblem *t;
    rpmProblem *f;
    int gotProblems = 0;

    if (ps == NULL || ps->numProblems == 0)
	return 0;

    if (filter == NULL)
	return (ps->numProblems == 0 ? 0 : 1);

    t = ps->probs;
    f = filter->probs;

    while ((f - filter->probs) < filter->numProblems) {
	while ((t - ps->probs) < ps->numProblems) {
	    if (rpmProblemCompare(*f, *t) == 0)
		break;
	    t++;
	    gotProblems = 1;
	}

	/* XXX This can't happen, but let's be sane in case it does. */
	if ((t - ps->probs) == ps->numProblems)
	    break;

	t++, f++;
    }

    if ((t - ps->probs) < ps->numProblems)
	gotProblems = 1;

    return gotProblems;
}

void rpmpsPrint(FILE *fp, rpmps ps)
{
    char * msg = NULL;
    rpmpsi psi = NULL;
    int i;

    if (ps == NULL || ps->probs == NULL || ps->numProblems <= 0)
	return;

    if (fp == NULL)
	fp = stderr;

    psi = rpmpsInitIterator(ps);
    while ((i = rpmpsNextIterator(psi)) >= 0) {
	int j;
	rpmProblem p = rpmpsGetProblem(psi);

	rpmpsi psif = rpmpsInitIterator(ps);
	/* Filter already displayed problems. */
    	while ((j = rpmpsNextIterator(psif)) < i) {
	    if (rpmProblemCompare(p, rpmpsGetProblem(psif)) == 0)
		break;
	}
	rpmpsFreeIterator(psif);
	if (j < i)
	    continue;

	msg = rpmProblemString(p);
	fprintf(fp, "\t%s\n", msg);
	msg = _free(msg);

    }
    psi = rpmpsFreeIterator(psi);
}
