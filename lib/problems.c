/**
 * \file lib/problems.c
 */

#include "system.h"

#include <rpmlib.h>

#include "depends.h"
#include "misc.h"
#include "debug.h"

/*@access rpmProblemSet@*/
/*@access rpmProblem@*/

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

static int sameProblem(struct rpmDependencyConflict * ap,
		       struct rpmDependencyConflict * bp)
{

    if (ap->sense != bp->sense)
	return 1;

    if (ap->byName && bp->byName && strcmp(ap->byName, bp->byName))
	return 1;
    if (ap->byVersion && bp->byVersion && strcmp(ap->byVersion, bp->byVersion))
	return 1;
    if (ap->byRelease && bp->byRelease && strcmp(ap->byRelease, bp->byRelease))
	return 1;

    if (ap->needsName && bp->needsName && strcmp(ap->needsName, bp->needsName))
	return 1;
    if (ap->needsVersion && bp->needsVersion && strcmp(ap->needsVersion, bp->needsVersion))
	return 1;
    if (ap->needsFlags && bp->needsFlags && ap->needsFlags != bp->needsFlags)
	return 1;

    return 0;
}

/* XXX FIXME: merge into problems */
void printDepProblems(FILE * fp, struct rpmDependencyConflict * conflicts,
			     int numConflicts)
{
    int i;

    for (i = 0; i < numConflicts; i++) {
	int j;

	/* Filter already displayed problems. */
	for (j = 0; j < i; j++) {
	    if (!sameProblem(conflicts + i, conflicts + j))
		break;
	}
	if (j < i)
	    continue;

	fprintf(fp, "\t%s", conflicts[i].needsName);
	if (conflicts[i].needsFlags)
	    printDepFlags(fp, conflicts[i].needsVersion, 
			  conflicts[i].needsFlags);

	if (conflicts[i].sense == RPMDEP_SENSE_REQUIRES) 
	    fprintf(fp, _(" is needed by %s-%s-%s\n"), conflicts[i].byName, 
		    conflicts[i].byVersion, conflicts[i].byRelease);
	else
	    fprintf(fp, _(" conflicts with %s-%s-%s\n"), conflicts[i].byName, 
		    conflicts[i].byVersion, conflicts[i].byRelease);
    }
}

const char * rpmProblemString(rpmProblem prob) /*@*/
{
    int nb =	(prob->pkgNEVR ? strlen(prob->pkgNEVR) : 0) +
		(prob->str1 ? strlen(prob->str1) : 0) +
		(prob->altNEVR ? strlen(prob->altNEVR) : 0) +
		100;
    char * buf = xmalloc(nb+1);

    switch (prob->type) {
    case RPMPROB_BADARCH:
	snprintf(buf, nb, _("package %s is for a different architecture"),
		prob->pkgNEVR);
	break;
    case RPMPROB_BADOS:
	snprintf(buf, nb, _("package %s is for a different operating system"),
		prob->pkgNEVR);
	break;
    case RPMPROB_PKG_INSTALLED:
	snprintf(buf, nb, _("package %s is already installed"),
		prob->pkgNEVR);
	break;
    case RPMPROB_BADRELOCATE:
	snprintf(buf, nb, _("path %s in package %s is not relocateable"),
		prob->str1, prob->pkgNEVR);
	break;
    case RPMPROB_NEW_FILE_CONFLICT:
	snprintf(buf, nb,
		_("file %s conflicts between attemped installs of %s and %s"),
		prob->str1, prob->pkgNEVR, prob->altNEVR);
	break;
    case RPMPROB_FILE_CONFLICT:
	snprintf(buf, nb,
	    _("file %s from install of %s conflicts with file from package %s"),
		prob->str1, prob->pkgNEVR, prob->altNEVR);
	break;
    case RPMPROB_OLDPACKAGE:
	snprintf(buf, nb,
		_("package %s (which is newer than %s) is already installed"),
		prob->altNEVR, prob->pkgNEVR);
	break;
    case RPMPROB_DISKSPACE:
	snprintf(buf, nb,
	    _("installing package %s needs %ld%cb on the %s filesystem"),
		prob->pkgNEVR,
		prob->ulong1 > (1024*1024)
		    ? (prob->ulong1 + 1024 * 1024 - 1) / (1024 * 1024)
		    : (prob->ulong1 + 1023) / 1024,
		prob->ulong1 > (1024*1024) ? 'M' : 'K',
		prob->str1);
	break;
    case RPMPROB_DISKNODES:
	snprintf(buf, nb,
	    _("installing package %s needs %ld inodes on the %s filesystem"),
		prob->pkgNEVR, (long)prob->ulong1, prob->str1);
	break;
    case RPMPROB_BADPRETRANS:
	snprintf(buf, nb,
		_("package %s pre-transaction syscall(s): %s failed: %s"),
		prob->pkgNEVR, prob->str1, strerror(prob->ulong1));
	break;
    case RPMPROB_REQUIRES:
    case RPMPROB_CONFLICT:
    default:
	snprintf(buf, nb,
		_("unknown error %d encountered while manipulating package %s"),
		prob->type, prob->pkgNEVR);
	break;
    }

    buf[nb] = '\0';
    return buf;
}

void rpmProblemPrint(FILE *fp, rpmProblem prob)
{
    const char *msg = rpmProblemString(prob);
    fprintf(fp, "%s\n", msg);
    free((void *)msg);
}

void rpmProblemSetPrint(FILE *fp, rpmProblemSet probs)
{
    int i;

    if (probs == NULL)
	return;

    if (fp == NULL)
	fp = stderr;

    for (i = 0; i < probs->numProblems; i++) {
	rpmProblem myprob = probs->probs + i;
	if (!myprob->ignoreProblem)
	    rpmProblemPrint(fp, myprob);
    }
}
