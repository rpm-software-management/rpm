#include "system.h"

#include "rpmbuild.h"

static struct ReqComp {
    char *token;
    int sense;
} ReqComparisons[] = {
    { "<=", RPMSENSE_LESS | RPMSENSE_EQUAL},
    { "<=S", RPMSENSE_LESS | RPMSENSE_EQUAL | RPMSENSE_SERIAL},
    { "=<", RPMSENSE_LESS | RPMSENSE_EQUAL},
    { "=<S", RPMSENSE_LESS | RPMSENSE_EQUAL | RPMSENSE_SERIAL},
    { "<", RPMSENSE_LESS},
    { "<S", RPMSENSE_LESS | RPMSENSE_SERIAL},

    { "=", RPMSENSE_EQUAL},
    { "=S", RPMSENSE_EQUAL | RPMSENSE_SERIAL},
    
    { ">=", RPMSENSE_GREATER | RPMSENSE_EQUAL},
    { ">=S", RPMSENSE_GREATER | RPMSENSE_EQUAL | RPMSENSE_SERIAL},
    { "=>", RPMSENSE_GREATER | RPMSENSE_EQUAL},
    { "=>S", RPMSENSE_GREATER | RPMSENSE_EQUAL | RPMSENSE_SERIAL},
    { ">", RPMSENSE_GREATER},
    { ">S", RPMSENSE_GREATER | RPMSENSE_SERIAL},
    { NULL, 0 },
};

#define	SKIPWHITE(_x)	{while(*(_x) && (isspace(*_x) || *(_x) == ',')) (_x)++;}
#define	SKIPNONWHITE(_x){while(*(_x) &&!(isspace(*_x) || *(_x) == ',')) (_x)++;}

int parseRCPOT(Spec spec, Package pkg, const char *field, int tag, int index)
{
    const char *r, *re, *v, *ve;
    char *req, *version;
    Header h;
    int flags;

    switch (tag) {
    case RPMTAG_PROVIDES:
	flags = RPMSENSE_PROVIDES;
	h = pkg->header;
	break;
    case RPMTAG_OBSOLETES:
	flags = RPMSENSE_OBSOLETES;
	h = pkg->header;
	break;
    case RPMTAG_CONFLICTFLAGS:
	flags = RPMSENSE_CONFLICTS;
	h = pkg->header;
	break;
    case RPMTAG_BUILDCONFLICTS:
	flags = RPMSENSE_CONFLICTS;
	h = spec->buildRestrictions;
	break;
    case RPMTAG_PREREQ:
	flags = RPMSENSE_PREREQ;
	h = pkg->header;
	break;
    case RPMTAG_BUILDPREREQ:
	flags = RPMSENSE_PREREQ;
	h = spec->buildRestrictions;
	break;
    case RPMTAG_TRIGGERIN:
	flags = RPMSENSE_TRIGGERIN;
	h = pkg->header;
	break;
    case RPMTAG_TRIGGERPOSTUN:
	flags = RPMSENSE_TRIGGERPOSTUN;
	h = pkg->header;
	break;
    case RPMTAG_TRIGGERUN:
	flags = RPMSENSE_TRIGGERUN;
	h = pkg->header;
	break;
    case RPMTAG_BUILDREQUIRES:
	flags = RPMSENSE_ANY;
	h = spec->buildRestrictions;
	break;
    default:
    case RPMTAG_REQUIREFLAGS:
	flags = RPMSENSE_ANY;
	h = pkg->header;
	break;
    }

    for (r = field; *r; r = re) {
	SKIPWHITE(r);
	if (*r == '\0')
	    break;

	/* Tokens must begin with alphanumeric, _, or / */
	if (!(isalnum(r[0]) || r[0] == '_' || r[0] == '/')) {
	    rpmError(RPMERR_BADSPEC,
		     _("line %d: Dependency tokens must begin with alpha-numeric, '_' or '/': %s"),
		     spec->lineNum, spec->line);
	    return RPMERR_BADSPEC;
	}

	/* Don't permit file names as args for certain tags */
	switch (tag) {
	case RPMTAG_OBSOLETES:
	case RPMTAG_CONFLICTFLAGS:
	case RPMTAG_BUILDCONFLICTS:
	    if (r[0] == '/') {
		rpmError(RPMERR_BADSPEC,_("line %d: File name not permitted: %s"),
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	    break;
	default:
	    break;
	}

	re = r;
	SKIPNONWHITE(re);
	req = malloc((re-r) + 1);
	strncpy(req, r, (re-r));
	req[re-r] = '\0';

	/* Parse version */
	v = re;
	SKIPWHITE(v);
	ve = v;
	SKIPNONWHITE(ve);

	re = v;	/* ==> next token (if no version found) starts here */

	/* Check for possible logical operator */
	if (ve > v) {
	  struct ReqComp *rc;
	  for (rc = ReqComparisons; rc->token != NULL; rc++) {
	    if ((ve-v) != strlen(rc->token) || strncmp(v, rc->token, (ve-v)))
		continue;

	    if (r[0] == '/') {
		rpmError(RPMERR_BADSPEC,
			 _("line %d: Versioned file name not permitted: %s"),
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }

	    switch(tag) {
	    case RPMTAG_PROVIDES:
	    case RPMTAG_OBSOLETES:
	    case RPMTAG_BUILDPREREQ:
	    case RPMTAG_PREREQ:
		rpmError(RPMERR_BADSPEC,
			 _("line %d: Version not permitted: %s"),
			 spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
		break;
	    default:
		break;
	    }
	    flags |= rc->sense;

	    /* now parse version */
	    v = ve;
	    SKIPWHITE(v);
	    ve = v;
	    SKIPNONWHITE(ve);
	    break;
	  }
	}

	if (flags & RPMSENSE_SENSEMASK) {
	    if (*v == '\0' || ve == v) {
		rpmError(RPMERR_BADSPEC, _("line %d: Version required: %s"),
			spec->lineNum, spec->line);
		return RPMERR_BADSPEC;
	    }
	    version = malloc((ve-v) + 1);
	    strncpy(version, v, (ve-v));
	    version[ve-v] = '\0';
	    re = ve;	/* ==> next token after version string starts here */
	} else
	    version = NULL;

	addReqProv(spec, h, flags, req, version, index);

	if (req) free(req);
	if (version) free(version);

    }

    return 0;
}
