/** \ingroup rpmbuild
 * \file build/parseReqs.c
 *  Parse dependency tag from spec file or from auto-dependency generator.
 */

#include "system.h"

#include <ctype.h>
#include <rpm/rpmtypes.h>
#include <rpm/rpmlog.h>
#include "build/rpmbuild_internal.h"
#include "build/rpmbuild_misc.h"
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

rpmRC parseRCPOT(rpmSpec spec, Package pkg, const char *field, rpmTagVal tagN,
	       int index, rpmsenseFlags tagflags)
{
    const char *r, *re, *v, *ve;
    const char *emsg = NULL;
    char * N = NULL, * EVR = NULL;
    rpmTagVal nametag = RPMTAG_NOT_FOUND;
    rpmsenseFlags Flags;
    rpmRC rc = RPMRC_FAIL; /* assume failure */

    switch (tagN) {
    default:
    case RPMTAG_REQUIREFLAGS:
	nametag = RPMTAG_REQUIRENAME;
	tagflags |= RPMSENSE_ANY;
	break;
    case RPMTAG_PROVIDEFLAGS:
	nametag = RPMTAG_PROVIDENAME;
	break;
    case RPMTAG_OBSOLETEFLAGS:
	nametag = RPMTAG_OBSOLETENAME;
	break;
    case RPMTAG_CONFLICTFLAGS:
	nametag = RPMTAG_CONFLICTNAME;
	break;
    case RPMTAG_ORDERFLAGS:
	nametag = RPMTAG_ORDERNAME;
	break;
    case RPMTAG_PREREQ:
	/* XXX map legacy PreReq into Requires(pre,preun) */
	nametag = RPMTAG_REQUIRENAME;
	tagflags |= (RPMSENSE_SCRIPT_PRE|RPMSENSE_SCRIPT_PREUN);
	break;
    case RPMTAG_TRIGGERPREIN:
	nametag = RPMTAG_TRIGGERNAME;
	tagflags |= RPMSENSE_TRIGGERPREIN;
	break;
    case RPMTAG_TRIGGERIN:
	nametag = RPMTAG_TRIGGERNAME;
	tagflags |= RPMSENSE_TRIGGERIN;
	break;
    case RPMTAG_TRIGGERPOSTUN:
	nametag = RPMTAG_TRIGGERNAME;
	tagflags |= RPMSENSE_TRIGGERPOSTUN;
	break;
    case RPMTAG_TRIGGERUN:
	nametag = RPMTAG_TRIGGERNAME;
	tagflags |= RPMSENSE_TRIGGERUN;
	break;
    case RPMTAG_BUILDPREREQ:
    case RPMTAG_BUILDREQUIRES:
	nametag = RPMTAG_REQUIRENAME;
	tagflags |= RPMSENSE_ANY;
	break;
    case RPMTAG_BUILDCONFLICTS:
	nametag = RPMTAG_CONFLICTNAME;
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
	    emsg = _("Dependency tokens must begin with alpha-numeric, '_' or '/'");
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
		emsg = _("Versioned file name not permitted");
		goto exit;
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
		emsg = _("Version required");
		goto exit;
	    }
	    EVR = xmalloc((ve-v) + 1);
	    rstrlcpy(EVR, v, (ve-v) + 1);
	    if (rpmCharCheck(spec, EVR, ve-v, ".-_+:%{}~")) goto exit;
	    re = ve;	/* ==> next token after EVR string starts here */
	} else
	    EVR = NULL;

	if (addReqProv(pkg, nametag, N, EVR, Flags, index)) {
	    emsg = _("invalid dependency");
	    goto exit;
	}

	N = _free(N);
	EVR = _free(EVR);

    }
    rc = RPMRC_OK;

exit:
    if (emsg) {
	/* Automatic dependencies don't relate to spec lines */
	if (tagflags & (RPMSENSE_FIND_REQUIRES|RPMSENSE_FIND_PROVIDES)) {
	    rpmlog(RPMLOG_ERR, "%s: %s\n", emsg, r);
	} else {
	    rpmlog(RPMLOG_ERR, _("line %d: %s: %s\n"),
		   spec->lineNum, emsg, spec->line);
	}
    }
    free(N);
    free(EVR);

    return rc;
}
