/**
 * \file lib/problems.c
 */

#include "system.h"

#include <rpmlib.h>

#include "depends.h"
#include "misc.h"
#include "debug.h"

/*@access fnpyKey@*/
/*@access rpmProblem@*/
/*@access rpmProblemSet@*/

rpmProblemSet rpmProblemSetCreate(void)
{
    rpmProblemSet probs;

    probs = xcalloc(1, sizeof(*probs));	/* XXX memory leak */
    probs->numProblems = probs->numProblemsAlloced = 0;
    probs->probs = NULL;

    return probs;
}

void rpmProblemSetFree(rpmProblemSet tsprobs)
{
    int i;

    for (i = 0; i < tsprobs->numProblems; i++) {
	rpmProblem p = tsprobs->probs + i;
	p->pkgNEVR = _free(p->pkgNEVR);
	p->altNEVR = _free(p->altNEVR);
	p->str1 = _free(p->str1);
    }
    tsprobs = _free(tsprobs);
}

void rpmProblemSetAppend(rpmProblemSet tsprobs, rpmProblemType type,
		const char * pkgNEVR, fnpyKey key,
		const char * dn, const char * bn,
		const char * altNEVR, unsigned long ulong1)
{
    rpmProblem p;
    char *t;

    if (tsprobs->numProblems == tsprobs->numProblemsAlloced) {
	if (tsprobs->numProblemsAlloced)
	    tsprobs->numProblemsAlloced *= 2;
	else
	    tsprobs->numProblemsAlloced = 2;
	tsprobs->probs = xrealloc(tsprobs->probs,
			tsprobs->numProblemsAlloced * sizeof(*tsprobs->probs));
    }

    p = tsprobs->probs + tsprobs->numProblems;
    tsprobs->numProblems++;
    memset(p, 0, sizeof(*p));

    p->type = type;
    p->key = key;
    p->ulong1 = ulong1;
    p->ignoreProblem = 0;

    p->pkgNEVR = pkgNEVR;	/* XXX FIXME: xstrdup */
    p->altNEVR = altNEVR;	/* XXX FIXME: xstrdup */

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

int rpmProblemSetTrim(rpmProblemSet tsprobs, rpmProblemSet filter)
{
    rpmProblem t;
    rpmProblem f;
    int gotProblems = 0;

    if (tsprobs == NULL || tsprobs->numProblems == 0)
	return 0;

    if (filter == NULL)
	return (tsprobs->numProblems == 0 ? 0 : 1);

    t = tsprobs->probs;
    f = filter->probs;

    /*@-branchstate@*/
    while ((f - filter->probs) < filter->numProblems) {
	if (!f->ignoreProblem) {
	    f++;
	    continue;
	}
	while ((t - tsprobs->probs) < tsprobs->numProblems) {
	    /*@-nullpass@*/	/* LCL: looks good to me */
	    if (f->type == t->type && t->key == f->key &&
		     XSTRCMP(f->str1, t->str1))
		/*@innerbreak@*/ break;
	    /*@=nullpass@*/
	    t++;
	    gotProblems = 1;
	}

	/* XXX This can't happen, but let's be sane in case it does. */
	if ((t - tsprobs->probs) == tsprobs->numProblems)
	    break;

	t->ignoreProblem = f->ignoreProblem;
	t++, f++;
    }
    /*@=branchstate@*/

    if ((t - tsprobs->probs) < tsprobs->numProblems)
	gotProblems = 1;

    return gotProblems;
}

/* XXX FIXME: merge into problems */
/* XXX used in verify.c rpmlibprov.c */
void printDepFlags(FILE * fp, const char * version, int flags)
{
    if (flags)
	fprintf(fp, " ");

    if (flags & RPMSENSE_LESS) 
	fprintf(fp, "<");
    if (flags & RPMSENSE_GREATER)
	fprintf(fp, ">");
    if (flags & RPMSENSE_EQUAL)
	fprintf(fp, "=");

    if (flags)
	fprintf(fp, " %s", version);
}

static int sameProblem(const rpmProblem ap, const rpmProblem bp)
	/*@*/
{
    if (ap->pkgNEVR)
	if (bp->pkgNEVR && strcmp(ap->pkgNEVR, bp->pkgNEVR))
	    return 1;
    if (ap->altNEVR)
	if (bp->altNEVR && strcmp(ap->altNEVR, bp->altNEVR))
	    return 1;

    return 0;
}

/* XXX FIXME: merge into problems */
rpmProblem rpmdepFreeConflicts(rpmProblem conflicts,
		int numConflicts)
{
    int i;

    if (conflicts != NULL)
    for (i = 0; i < numConflicts; i++) {
	rpmProblem p = conflicts + i;
	p->pkgNEVR = _free(p->pkgNEVR);
	p->altNEVR = _free(p->altNEVR);
	p->str1 = _free(p->str1);
    }
    conflicts = _free(conflicts);
    return NULL;
}

/* XXX FIXME: merge into problems */
void printDepProblems(FILE * fp, rpmProblem conflicts, int numConflicts)
{
    const char * pkgNEVR, * altNEVR;
    rpmProblem c;
    int i;

    for (i = 0; i < numConflicts; i++) {
	int j;

	c = conflicts + i;

	/* Filter already displayed problems. */
	for (j = 0; j < i; j++) {
	    if (!sameProblem(c, conflicts + j))
		/*@innerbreak@*/ break;
	}
	if (j < i)
	    continue;

	pkgNEVR = (c->pkgNEVR ? c->pkgNEVR : "?pkgNEVR?");
	altNEVR = (c->altNEVR ? c->altNEVR : "? ?altNEVR?");

	fprintf(fp, "\t%s %s %s\n", altNEVR+2,
		((altNEVR[0] == 'C' && altNEVR[1] == ' ')
			?  _("conflicts with") : _("is needed by")),
		pkgNEVR);
    }
}

#if !defined(HAVE_VSNPRINTF)
/*@-shadow -bufferoverflowhigh @*/
static inline int vsnprintf(/*@out@*/ char * buf, /*@unused@*/ int nb,
	const char * fmt, va_list ap)
{
    return vsprintf(buf, fmt, ap);
}
/*@=shadow =bufferoverflowhigh @*/
#endif
#if !defined(HAVE_SNPRINTF)
static inline int snprintf(/*@out@*/ char * buf, int nb, const char * fmt, ...)
{
    va_list ap;
    int rc;
    va_start(ap, fmt);
    /*@=modunconnomods@*/
    rc = vsnprintf(buf, nb, fmt, ap);
    /*@=modunconnomods@*/
    va_end(ap);
    return rc;
}
#endif

const char * rpmProblemString(const rpmProblem prob)
{
/*@observer@*/
    const char * pkgNEVR = (prob->pkgNEVR ? prob->pkgNEVR : "?pkgNEVR?");
/*@observer@*/
    const char * altNEVR = (prob->altNEVR ? prob->altNEVR : "?altNEVR?");
/*@observer@*/
    const char * str1 = (prob->str1 ? prob->str1 : "");
    int nb =	strlen(pkgNEVR) + strlen(str1) + strlen(altNEVR) + 100;
    char * buf = xmalloc(nb+1);
    int rc;

    switch (prob->type) {
    case RPMPROB_BADARCH:
	rc = snprintf(buf, nb,
		_("package %s is for a different architecture"),
		pkgNEVR);
	break;
    case RPMPROB_BADOS:
	rc = snprintf(buf, nb,
		_("package %s is for a different operating system"),
		pkgNEVR);
	break;
    case RPMPROB_PKG_INSTALLED:
	rc = snprintf(buf, nb,
		_("package %s is already installed"),
		pkgNEVR);
	break;
    case RPMPROB_BADRELOCATE:
	rc = snprintf(buf, nb,
		_("path %s in package %s is not relocateable"),
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
	    _("installing package %s needs %ld%cb on the %s filesystem"),
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
    case RPMPROB_BADPRETRANS:
	rc = snprintf(buf, nb,
		_("package %s pre-transaction syscall(s): %s failed: %s"),
		pkgNEVR, str1, strerror(prob->ulong1));
	break;
    case RPMPROB_REQUIRES:
	rc = snprintf(buf, nb, _("package %s has unsatisfied Requires: %s\n"),
		pkgNEVR, altNEVR+2);
	break;
    case RPMPROB_CONFLICT:
	rc = snprintf(buf, nb, _("package %s has unsatisfied Conflicts: %s\n"),
		pkgNEVR, altNEVR+2);
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

void rpmProblemPrint(FILE *fp, rpmProblem prob)
{
    const char * msg = rpmProblemString(prob);
    fprintf(fp, "%s\n", msg);
    msg = _free(msg);
}

void rpmProblemSetPrint(FILE *fp, rpmProblemSet tsprobs)
{
    int i;

    if (tsprobs == NULL)
	return;

    if (fp == NULL)
	fp = stderr;

    for (i = 0; i < tsprobs->numProblems; i++) {
	rpmProblem myprob = tsprobs->probs + i;
	if (!myprob->ignoreProblem)
	    rpmProblemPrint(fp, myprob);
    }
}
