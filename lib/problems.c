#include "system.h"

#include "rpmlib.h"

#include "depends.h"
#include "misc.h"

char * rpmProblemString(rpmProblem prob) {
    char * name, * version, * release;
    char * buf;

    headerGetEntry(prob.h, RPMTAG_NAME, NULL, (void **) &name, NULL);
    headerGetEntry(prob.h, RPMTAG_VERSION, NULL, (void **) &version, NULL);
    headerGetEntry(prob.h, RPMTAG_RELEASE, NULL, (void **) &release, NULL);

    buf = malloc(strlen(name) + strlen(version) + strlen(release) + 400);

    switch (prob.type) {
      case RPMPROB_BADOS:
	sprintf(buf, _("package %s-%s-%s is for a different operating system"), 
		name, version, release);
	break;
	
      case RPMPROB_BADARCH:
	sprintf(buf, _("package %s-%s-%s is for a different architecture"), 
		name, version, release);
	break;

      case RPMPROB_PKG_INSTALLED:
	sprintf(buf, _("package %s-%s-%s is already installed"),
			  name, version, release);
	break;

      case RPMPROB_BADRELOCATE:
	sprintf(buf, _("path %s is not relocateable for package %s-%s-%s"),
		     prob.str1, name, version, release);
	break;

      default:
	sprintf(buf, _("unknown error %d encountered while manipulating "
		"package %s-%s-%s"), name, version, release);
	break;
    }

    return buf;
}

void rpmProblemSetFilter(rpmProblemSet ps, int flags) {
    int i;
    int flag;

    for (i = 0; i < ps->numProblems; i++) {
	switch (ps->probs[i].type) {
	  case RPMPROB_BADOS:
	    flag = RPMPROB_FILTER_IGNOREOS; break;
	  case RPMPROB_BADARCH:
	    flag = RPMPROB_FILTER_IGNOREARCH; break;
	  case RPMPROB_PKG_INSTALLED:
	    flag = RPMPROB_FILTER_REPLACEPKG; break;
	  case RPMPROB_BADRELOCATE:
	    flag = RPMPROB_FILTER_FORCERELOCATE; break;
	  default:
	    flag = 0;
	}

	ps->probs[i].ignoreProblem = !!(flags & flag);
    }
}
