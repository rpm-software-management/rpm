/**
 * \file lib/problems.c
 */

#include "system.h"

#include <rpmlib.h>

#include "depends.h"
#include "misc.h"

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

const char * rpmProblemString(rpmProblem prob)
{
    const char * name, * version, * release;
    const char * altName = NULL, * altVersion = NULL, * altRelease = NULL;
    char * buf;

    headerNVR(prob->h, &name, &version, &release);

    if (prob->altH)
	headerNVR(prob->altH, &altName, &altVersion, &altRelease);

    buf = xmalloc(strlen(name) + strlen(version) + strlen(release) + 400);

    switch (prob->type) {
      case RPMPROB_BADARCH:
	sprintf(buf, _("package %s-%s-%s is for a different architecture"), 
		name, version, release);
	break;

      case RPMPROB_BADOS:
	sprintf(buf, _("package %s-%s-%s is for a different operating system"), 
		name, version, release);
	break;
	
      case RPMPROB_PKG_INSTALLED:
	sprintf(buf, _("package %s-%s-%s is already installed"),
		name, version, release);
	break;

      case RPMPROB_BADRELOCATE:
	sprintf(buf, _("path %s is not relocateable for package %s-%s-%s"),
		prob->str1, name, version, release);
	break;

      case RPMPROB_NEW_FILE_CONFLICT:
	sprintf(buf, _("file %s conflicts between attemped installs of "
		       "%s-%s-%s and %s-%s-%s"), prob->str1, name, version, 
		release, altName, altVersion, altRelease);
	break;

      case RPMPROB_FILE_CONFLICT:
	sprintf(buf, _("file %s from install of %s-%s-%s conflicts with "
		       "file from package %s-%s-%s"), prob->str1, name, version, 
		release, altName, altVersion, altRelease);
	break;

      case RPMPROB_OLDPACKAGE:
	sprintf(buf, _("package %s-%s-%s (which is newer than %s-%s-%s) is "
		       "already installed"), altName, altVersion, altRelease,
			name, version, release);
	break;

      case RPMPROB_DISKSPACE:
	sprintf(buf, _("installing package %s-%s-%s needs %ld%cb on the %s"
		       " filesystem"), name, version, release, 
			prob->ulong1 > (1024*1024)
			    ? (prob->ulong1 + 1024 * 1024 - 1) / (1024 * 1024)
			    : (prob->ulong1 + 1023) / 1024,
			prob->ulong1 > (1024*1024) ? 'M' : 'K',
			prob->str1);
	break;

      case RPMPROB_DISKNODES:
	sprintf(buf, _("installing package %s-%s-%s needs %ld inodes on the %s"
		       " filesystem"), name, version, release, 
			prob->ulong1,
			prob->str1);
	break;

      case RPMPROB_BADPRETRANS:
	sprintf(buf, _("package %s-%s-%s pre-transaction syscall(s): %s failed: %s"),
			name, version, release, 
			prob->str1, strerror(prob->ulong1));
	break;

      case RPMPROB_REQUIRES:
      case RPMPROB_CONFLICT:
      default:
	sprintf(buf, _("unknown error %d encountered while manipulating "
		"package %s-%s-%s"), prob->type, name, version, release);
	break;
    }

    return buf;
}

void rpmProblemPrint(FILE *fp, rpmProblem prob)
{
    const char *msg = rpmProblemString(prob);
    fprintf(fp, "%s\n", msg);
    xfree(msg);
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
