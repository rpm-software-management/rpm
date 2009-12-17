/** \ingroup rpmbuild
 * \file build/parseReqs.c
 *  Parse dependency tag from spec file or from auto-dependency generator.
 */

#include "system.h"

#include <rpm/rpmtypes.h>
#include <rpm/rpmbuild.h>
#include <rpm/rpmlog.h>
#include "debug.h"

/**
 */
static struct ReqComp {
const char * token;
    rpmsenseFlags sense;
} const ReqComparisons[] = {
    { "<=", RPMSENSE_LESS | RPMSENSE_EQUAL},
    { "=<", RPMSENSE_LESS | RPMSENSE_EQUAL},
    { "<", RPMSENSE_LESS},

    { "==", RPMSENSE_EQUAL},
    { "=", RPMSENSE_EQUAL},
    
    { ">=", RPMSENSE_GREATER | RPMSENSE_EQUAL},
    { "=>", RPMSENSE_GREATER | RPMSENSE_EQUAL},
    { ">", RPMSENSE_GREATER},

    { NULL, 0 },
};

#define	SKIPWHITE(_x)	{while(*(_x) && (risspace(*_x) || *(_x) == ',')) (_x)++;}
#define	SKIPNONWHITE(_x){while(*(_x) &&!(risspace(*_x) || *(_x) == ',')) (_x)++;}

rpmRC parseRCPOT(rpmSpec spec, Package pkg, const char *field, rpmTag tagN,
	       int index, rpmsenseFlags tagflags)
{
    const char *r, *re, *v, *ve;
    char * N = NULL, * EVR = NULL;
    rpmsenseFlags Flags;
    Header h;
    rpmRC rc = RPMRC_FAIL; /* assume failure */

    switch (tagN) {
    case RPMTAG_PROVIDEFLAGS:
	tagflags |= RPMSENSE_PROVIDES;
	h = pkg->header;
	break;
    case RPMTAG_OBSOLETEFLAGS:
	tagflags |= RPMSENSE_OBSOLETES;
	h = pkg->header;
	break;
    case RPMTAG_CONFLICTFLAGS:
	tagflags |= RPMSENSE_CONFLICTS;
	h = pkg->header;
	break;
    case RPMTAG_BUILDCONFLICTS:
	tagflags |= RPMSENSE_CONFLICTS;
	h = spec->buildRestrictions;
	break;
    case RPMTAG_PREREQ:
	/* XXX map legacy PreReq into Requires(pre,preun) */
	tagflags |= (RPMSENSE_SCRIPT_PRE|RPMSENSE_SCRIPT_PREUN);
	h = pkg->header;
	break;
    case RPMTAG_TRIGGERPREIN:
	tagflags |= RPMSENSE_TRIGGERPREIN;
	h = pkg->header;
	break;
    case RPMTAG_TRIGGERIN:
	tagflags |= RPMSENSE_TRIGGERIN;
	h = pkg->header;
	break;
    case RPMTAG_TRIGGERPOSTUN:
	tagflags |= RPMSENSE_TRIGGERPOSTUN;
	h = pkg->header;
	break;
    case RPMTAG_TRIGGERUN:
	tagflags |= RPMSENSE_TRIGGERUN;
	h = pkg->header;
	break;
    case RPMTAG_BUILDPREREQ:
    case RPMTAG_BUILDREQUIRES:
	tagflags |= RPMSENSE_ANY;
	h = spec->buildRestrictions;
	break;
    default:
    case RPMTAG_REQUIREFLAGS:
	tagflags |= RPMSENSE_ANY;
	h = pkg->header;
	break;
    }

    for (r = field; *r != '\0'; r = re) {
	SKIPWHITE(r);
	if (*r == '\0')
	    break;

	Flags = (tagflags & ~RPMSENSE_SENSEMASK);

	/* 
	 * Tokens must begin with alphanumeric, _, or /, but we don't know
	 * the spec's encoding so we only check what we can: plain ascii.
	 */
	if (isascii(r[0]) && !(risalnum(r[0]) || r[0] == '_' || r[0] == '/')) {
	    rpmlog(RPMLOG_ERR,
		     _("line %d: Dependency tokens must begin with alpha-numeric, '_' or '/': %s\n"),
		     spec->lineNum, spec->line);
	    goto exit;
	}

	re = r;
	SKIPNONWHITE(re);
	N = xmalloc((re-r) + 1);
	rstrlcpy(N, r, (re-r) + 1);

	/* Parse EVR */
	v = re;
	SKIPWHITE(v);
	ve = v;
	SKIPNONWHITE(ve);

	re = v;	/* ==> next token (if no EVR found) starts here */

	/* Check for possible logical operator */
	if (ve > v) {
	  const struct ReqComp *rc;
	  for (rc = ReqComparisons; rc->token != NULL; rc++) {
	    if ((ve-v) != strlen(rc->token) || !rstreqn(v, rc->token, (ve-v)))
		continue;

	    if (r[0] == '/') {
		rpmlog(RPMLOG_ERR,
			 _("line %d: Versioned file name not permitted: %s\n"),
			 spec->lineNum, spec->line);
		goto exit;
	    }

	    switch(tagN) {
	    case RPMTAG_BUILDPREREQ:
	    case RPMTAG_PREREQ:
	    case RPMTAG_PROVIDEFLAGS:
	    case RPMTAG_OBSOLETEFLAGS:
		/* Add prereq on rpmlib that has versioned dependencies. */
		if (!rpmExpandNumeric("%{?_noVersionedDependencies}"))
		    (void) rpmlibNeedsFeature(h, "VersionedDependencies", "3.0.3-1");
		break;
	    default:
		break;
	    }
	    Flags |= rc->sense;

	    /* now parse EVR */
	    v = ve;
	    SKIPWHITE(v);
	    ve = v;
	    SKIPNONWHITE(ve);
	    break;
	  }
	}

	if (Flags & RPMSENSE_SENSEMASK) {
	    if (*v == '\0' || ve == v) {
		rpmlog(RPMLOG_ERR, _("line %d: Version required: %s\n"),
			spec->lineNum, spec->line);
		goto exit;
	    }
	    EVR = xmalloc((ve-v) + 1);
	    rstrlcpy(EVR, v, (ve-v) + 1);
	    if (rpmCharCheck(spec, EVR, ve-v, ".-_+:%{}")) goto exit;
	    re = ve;	/* ==> next token after EVR string starts here */
	} else
	    EVR = NULL;

	if (addReqProv(spec, h, tagN, N, EVR, Flags, index)) {
	    rpmlog(RPMLOG_ERR, _("line %d: invalid dependency: %s\n"),
		   spec->lineNum, spec->line);
	    goto exit;
	}

	N = _free(N);
	EVR = _free(EVR);

    }
    rc = RPMRC_OK;

exit:
    free(N);
    free(EVR);

    return rc;
}
