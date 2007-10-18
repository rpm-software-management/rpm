/**
 * \file lib/rpmps.c
 */

#include "system.h"

#include <rpmlib.h>

#include "rpmps.h"

#include "misc.h"
#include "debug.h"


int _rpmps_debug = 0;

rpmps XrpmpsUnlink(rpmps ps, const char * msg,
		const char * fn, unsigned ln)
{
if (_rpmps_debug > 0 && msg != NULL)
fprintf(stderr, "--> ps %p -- %d %s at %s:%u\n", ps, ps->nrefs, msg, fn, ln);
    ps->nrefs--;
    return ps;
}

rpmps XrpmpsLink(rpmps ps, const char * msg,
		const char * fn, unsigned ln)
{
    ps->nrefs++;
if (_rpmps_debug > 0 && msg != NULL)
fprintf(stderr, "--> ps %p ++ %d %s at %s:%u\n", ps, ps->nrefs, msg, fn, ln);
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
    if (ps != NULL) {
	psi = xcalloc(1, sizeof(*psi));
	psi->ps = rpmpsLink(ps, "iter ref");
	psi->ix = -1;
    }
    return psi;
}

rpmpsi rpmpsFreeIterator(rpmpsi psi)
{
    if (psi != NULL) {
	rpmpsUnlink(psi->ps, "iter unref");
	free(psi);
	psi = NULL;
    }
    return psi;
}

int rpmpsNextIterator(rpmpsi psi)
{
    int i = -1;

    if (psi != NULL && ++psi->ix >= 0) {
	if (psi->ix < rpmpsNumProblems(psi->ps)) {
	    i = psi->ix;
	} else {
	    psi->ix = -1;
	}	     
    }
    return i;
}

rpmProblem rpmpsProblem(rpmpsi psi)
{
    rpmProblem p = NULL;
    if (psi != NULL && psi->ix >= 0 && psi->ix < rpmpsNumProblems(psi->ps)) {
	p = psi->ps->probs + psi->ix;
    } 
    return p;
}

rpmps rpmpsCreate(void)
{
    rpmps ps = xcalloc(1, sizeof(*ps));
    return rpmpsLink(ps, "create");
}

rpmps rpmpsFree(rpmps ps)
{
    if (ps == NULL) return NULL;
    ps = rpmpsUnlink(ps, "dereference");
    if (ps->nrefs > 0)
	return NULL;
	
    if (ps->probs) {
	int i;
	for (i = 0; i < ps->numProblems; i++) {
	    rpmProblem p = ps->probs + i;
	    p->pkgNEVR = _free(p->pkgNEVR);
	    p->altNEVR = _free(p->altNEVR);
	    p->str1 = _free(p->str1);
	}
	ps->probs = _free(ps->probs);
    }
    ps = _free(ps);
    return NULL;
}

void rpmpsAppend(rpmps ps, rpmProblemType type,
		const char * pkgNEVR, fnpyKey key,
		const char * dn, const char * bn,
		const char * altNEVR, unsigned long ulong1)
{
    rpmProblem p;
    char *t;

    if (ps == NULL) return;

    if (ps->numProblems == ps->numProblemsAlloced) {
	if (ps->numProblemsAlloced)
	    ps->numProblemsAlloced *= 2;
	else
	    ps->numProblemsAlloced = 2;
	ps->probs = xrealloc(ps->probs,
			ps->numProblemsAlloced * sizeof(*ps->probs));
    }

    p = ps->probs + ps->numProblems;
    ps->numProblems++;
    memset(p, 0, sizeof(*p));

    p->type = type;
    p->key = key;
    p->ulong1 = ulong1;
    p->ignoreProblem = 0;

    p->pkgNEVR = (pkgNEVR ? xstrdup(pkgNEVR) : NULL);
    p->altNEVR = (altNEVR ? xstrdup(altNEVR) : NULL);

    p->str1 = NULL;
    if (dn != NULL || bn != NULL) {
	t = xcalloc(1,	(dn != NULL ? strlen(dn) : 0) +
			(bn != NULL ? strlen(bn) : 0) + 1);
	p->str1 = t;
	if (dn != NULL) t = stpcpy(t, dn);
	if (bn != NULL) t = stpcpy(t, bn);
    }
}

#define XSTRCMP(a, b) ((!(a) && !(b)) || ((a) && (b) && !strcmp((a), (b))))

int rpmpsTrim(rpmps ps, rpmps filter)
{
    rpmProblem t;
    rpmProblem f;
    int gotProblems = 0;

    if (ps == NULL || ps->numProblems == 0)
	return 0;

    if (filter == NULL)
	return (ps->numProblems == 0 ? 0 : 1);

    t = ps->probs;
    f = filter->probs;

    while ((f - filter->probs) < filter->numProblems) {
	if (!f->ignoreProblem) {
	    f++;
	    continue;
	}
	while ((t - ps->probs) < ps->numProblems) {
	   	/* LCL: looks good to me <shrug> */
	    if (f->type == t->type && t->key == f->key &&
		     XSTRCMP(f->str1, t->str1))
		break;
	    t++;
	    gotProblems = 1;
	}

	/* XXX This can't happen, but let's be sane in case it does. */
	if ((t - ps->probs) == ps->numProblems)
	    break;

	t->ignoreProblem = f->ignoreProblem;
	t++, f++;
    }

    if ((t - ps->probs) < ps->numProblems)
	gotProblems = 1;

    return gotProblems;
}

#if !defined(HAVE_VSNPRINTF)
static inline int vsnprintf(char * buf, int nb,
	const char * fmt, va_list ap)
{
    return vsprintf(buf, fmt, ap);
}
#endif
#if !defined(HAVE_SNPRINTF)
static inline int snprintf(char * buf, int nb, const char * fmt, ...)
{
    va_list ap;
    int rc;
    va_start(ap, fmt);
    rc = vsnprintf(buf, nb, fmt, ap);
    va_end(ap);
    return rc;
}
#endif

const char * rpmProblemString(const rpmProblem prob)
{
    const char * pkgNEVR = (prob->pkgNEVR ? prob->pkgNEVR : "?pkgNEVR?");
    const char * altNEVR = (prob->altNEVR ? prob->altNEVR : "? ?altNEVR?");
    const char * str1 = (prob->str1 ? prob->str1 : N_("different"));
    int nb =	strlen(pkgNEVR) + strlen(str1) + strlen(altNEVR) + 100;
    char * buf = xmalloc(nb+1);
    int rc;

    switch (prob->type) {
    case RPMPROB_BADARCH:
	rc = snprintf(buf, nb,
		_("package %s is intended for a %s architecture"),
		pkgNEVR, str1);
	break;
    case RPMPROB_BADOS:
	rc = snprintf(buf, nb,
		_("package %s is intended for a %s operating system"),
		pkgNEVR, str1);
	break;
    case RPMPROB_PKG_INSTALLED:
	rc = snprintf(buf, nb,
		_("package %s is already installed"),
		pkgNEVR);
	break;
    case RPMPROB_BADRELOCATE:
	rc = snprintf(buf, nb,
		_("path %s in package %s is not relocatable"),
		str1, pkgNEVR);
	break;
    case RPMPROB_NEW_FILE_CONFLICT:
	rc = snprintf(buf, nb,
		_("file %s conflicts between attempted installs of %s and %s"),
		str1, pkgNEVR, altNEVR);
	break;
    case RPMPROB_FILE_CONFLICT:
	rc = snprintf(buf, nb,
	    _("file %s from install of %s conflicts with file from package %s"),
		str1, pkgNEVR, altNEVR);
	break;
    case RPMPROB_OLDPACKAGE:
	rc = snprintf(buf, nb,
		_("package %s (which is newer than %s) is already installed"),
		altNEVR, pkgNEVR);
	break;
    case RPMPROB_DISKSPACE:
	rc = snprintf(buf, nb,
	    _("installing package %s needs %ld%cB on the %s filesystem"),
		pkgNEVR,
		prob->ulong1 > (1024*1024)
		    ? (prob->ulong1 + 1024 * 1024 - 1) / (1024 * 1024)
		    : (prob->ulong1 + 1023) / 1024,
		prob->ulong1 > (1024*1024) ? 'M' : 'K',
		str1);
	break;
    case RPMPROB_DISKNODES:
	rc = snprintf(buf, nb,
	    _("installing package %s needs %ld inodes on the %s filesystem"),
		pkgNEVR, (long)prob->ulong1, str1);
	break;
    case RPMPROB_REQUIRES:
	rc = snprintf(buf, nb, _("%s is needed by %s%s"),
		altNEVR+2,
		(prob->ulong1 ? "" : _("(installed) ")), pkgNEVR);
	break;
    case RPMPROB_CONFLICT:
	rc = snprintf(buf, nb, _("%s conflicts with %s%s"),
		altNEVR+2,
		(prob->ulong1 ? "" : _("(installed) ")), pkgNEVR);
	break;
    default:
	rc = snprintf(buf, nb,
		_("unknown error %d encountered while manipulating package %s"),
		prob->type, pkgNEVR);
	break;
    }

    buf[nb] = '\0';
    return buf;
}

static int sameProblem(const rpmProblem ap, const rpmProblem bp)
{
    if (ap->type != bp->type)
	return 1;
    if (ap->pkgNEVR)
	if (bp->pkgNEVR && strcmp(ap->pkgNEVR, bp->pkgNEVR))
	    return 1;
    if (ap->altNEVR)
	if (bp->altNEVR && strcmp(ap->altNEVR, bp->altNEVR))
	    return 1;
    if (ap->str1)
	if (bp->str1 && strcmp(ap->str1, bp->str1))
	    return 1;

    if (ap->ulong1 != bp->ulong1)
	return 1;

    return 0;
}

void rpmpsPrint(FILE *fp, rpmps ps)
{
    const char * msg;
    rpmpsi psi = NULL;
    int i;

    if (ps == NULL || ps->probs == NULL || ps->numProblems <= 0)
	return;

    if (fp == NULL)
	fp = stderr;

    psi = rpmpsInitIterator(ps);
    while ((i = rpmpsNextIterator(psi)) >= 0) {
	int j;
	rpmProblem p = rpmpsProblem(psi);

	if (p->ignoreProblem)
	    continue;

	rpmpsi psif = rpmpsInitIterator(ps);
	/* Filter already displayed problems. */
    	while ((j = rpmpsNextIterator(psif)) < i) {
	    if (!sameProblem(p, rpmpsProblem(psif)))
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
