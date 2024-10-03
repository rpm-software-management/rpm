/**
 * \file lib/rpmps.c
 */

#include "system.h"

#include <vector>

#include <inttypes.h>
#include <stdlib.h>

#include <rpm/rpmstring.h>
#include <rpm/rpmps.h>

#include "debug.h"

using std::vector;

struct rpmps_s {
    vector<rpmProblem> probs;	/*!< Array of pointers to specific problems. */
    int nrefs;			/*!< Reference count. */
};

struct rpmpsi_s {
    int ix;
    rpmps ps;
};


static rpmps rpmpsUnlink(rpmps ps)
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
    if (ps)
	numProblems = ps->probs.size();
    return numProblems;
}

rpmpsi rpmpsInitIterator(rpmps ps)
{
    rpmpsi psi = NULL;
    if (ps != NULL && ps->probs.empty() == false) {
	psi = new rpmpsi_s {};
	psi->ps = rpmpsLink(ps);
	psi->ix = -1;
    }
    return psi;
}

rpmpsi rpmpsFreeIterator(rpmpsi psi)
{
    if (psi != NULL) {
	rpmpsUnlink(psi->ps);
	delete psi;
    }
    return NULL;
}

rpmProblem rpmpsiNext(rpmpsi psi)
{
    rpmProblem p = NULL;
    if (psi != NULL && psi->ps != NULL && ++psi->ix >= 0) {
	rpmps ps = psi->ps;
	if (psi->ix < ps->probs.size()) {
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
    rpmps ps = new rpmps_s {};
    return rpmpsLink(ps);
}

rpmps rpmpsFree(rpmps ps)
{
    if (ps == NULL) return NULL;
    if (ps->nrefs > 1) {
	return rpmpsUnlink(ps);
    }
	
    for (auto & prob : ps->probs)
	rpmProblemFree(prob);
    delete ps;
    return NULL;
}

void rpmpsAppendProblem(rpmps ps, rpmProblem prob)
{
    if (ps == NULL || prob == NULL) return;

    ps->probs.push_back(rpmProblemLink(prob));
}

/*
 * TODO: filter out duplicates while merging. Also horribly inefficient... */
int rpmpsMerge(rpmps dest, rpmps src)
{
    int rc = 0;
    if (dest != NULL) {
	rpmProblem p;
	rpmpsi spi = rpmpsInitIterator(src);
	while ((p = rpmpsiNext(spi)) != NULL) {
	    rpmpsAppendProblem(dest, p);
	    rc++;
	}
	rpmpsFreeIterator(spi);
    }
    return rc;
}

void rpmpsPrint(FILE *fp, rpmps ps)
{
    rpmProblem p;
    rpmpsi psi = rpmpsInitIterator(ps);
    FILE *f = (fp != NULL) ? fp : stderr;

    while ((p = rpmpsiNext(psi))) {
	char *msg = rpmProblemString(p);
	fprintf(f, "\t%s\n", msg);
	free(msg);
    }
    rpmpsFreeIterator(psi);
}

