/*@-modfilesys@*/
/** \ingroup rpmio
 * \file rpmio/rpmio.c
 */
#include "system.h"

#define	_RPMGI_INTERNAL
#include <rpmgi.h>

#include <rpmdb.h>

#include "debug.h"

/*@unchecked@*/
int _rpmgi_debug = 0;

rpmgi XrpmgiUnlink(rpmgi gi, const char * msg, const char * fn, unsigned ln)
{
    if (gi == NULL) return NULL;

if (_rpmgi_debug && msg != NULL)
fprintf(stderr, "--> gi %p -- %d %s at %s:%u\n", gi, gi->nrefs, msg, fn, ln);

    gi->nrefs--;
    return NULL;
}

rpmgi XrpmgiLink(rpmgi gi, const char * msg, const char * fn, unsigned ln)
{
    if (gi == NULL) return NULL;
    gi->nrefs++;

if (_rpmgi_debug && msg != NULL)
fprintf(stderr, "--> gi %p ++ %d %s at %s:%u\n", gi, gi->nrefs, msg, fn, ln);

    /*@-refcounttrans@*/ return gi; /*@=refcounttrans@*/
}

rpmgi rpmgiFree(rpmgi gi)
{
    if (gi == NULL)
	return NULL;

    if (gi->nrefs > 1)
	return rpmgiUnlink(gi, NULL);

if (_rpmgi_debug < 0)
fprintf(stderr, "*** gi %p\t%p[%d]\n", gi, gi->argv, gi->argc);

    (void) rpmgiUnlink(gi, NULL);

    switch (gi->tag) {
    default:
    case RPMGI_RPMDB:
	break;
    case RPMGI_HDLIST:
	break;
    case RPMGI_ARGLIST:
	break;
    case RPMGI_FTSWALK:
	break;
    }

    if (gi->ftsp != NULL) {
	int xx;
	xx = Fts_close(gi->ftsp);
	gi->ftsp = NULL;
	gi->fts = NULL;
    }
    gi->mi = rpmdbFreeIterator(gi->mi);
    gi->ts = rpmtsFree(gi->ts);

    memset(gi, 0, sizeof(*gi));		/* XXX trash and burn */
    gi = _free(gi);
    return NULL;
}

rpmgi rpmgiNew(rpmts ts, int tag, const void * keyp, size_t keylen)
{
    rpmgi gi = xcalloc(1, sizeof(*gi));

    if (gi == NULL)
	return NULL;

    gi->ts = rpmtsLink(ts, NULL);
    gi->tag = tag;
    gi->i = -1;

    switch (gi->tag) {
    default:
    case RPMGI_RPMDB:
	gi->mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, NULL, 0);

if (_rpmgi_debug < 0)
fprintf(stderr, "*** gi %p\t%p\n", gi, gi->mi);

	break;
    case RPMGI_HDLIST:
	break;
    case RPMGI_ARGLIST:
    case RPMGI_FTSWALK:
    {   char *const * argv = keyp;
	unsigned flags = keylen;

	gi->argv = argv;
	gi->argc = 0;
	if (argv != NULL)
        while (*argv++ != NULL)
	   gi->argc++;
	gi->ftsOpts = flags;

if (_rpmgi_debug < 0)
fprintf(stderr, "*** gi %p\t%p[%d]\n", gi, gi->argv, gi->argc);

    }	break;
    }

    gi = rpmgiLink(gi, NULL);

    return gi;
}

static int indent = 2;

static const char * ftsInfoStrings[] = {
    "UNKNOWN",
    "D",
    "DC",
    "DEFAULT",
    "DNR",
    "DOT",
    "DP",
    "ERR",
    "F",
    "INIT",
    "NS",
    "NSOK",
    "SL",
    "SLNONE",
    "W",
};

static const char * ftsInfoStr(int fts_info) {
    if (!(fts_info >= 1 && fts_info <= 14))
	fts_info = 0;
    return ftsInfoStrings[ fts_info ];
}

const char * rpmgiNext(/*@null@*/ rpmgi gi)
	/*@modifies gi @*/
{
    const char * val = NULL;
    Header h = NULL;

    if (gi != NULL && ++gi->i >= 0)
    switch (gi->tag) {
    default:
    case RPMGI_RPMDB:
	h = rpmdbNextIterator(gi->mi);
	if (h != NULL) {
	    const char * fmt = "%{NAME}-%{VERSION}-%{RELEASE}";
	    val = headerSprintf(h, fmt, rpmTagTable, rpmHeaderFormats, NULL);
	} else {
	    gi->mi = rpmdbFreeIterator(gi->mi);
	    gi->i = -1;
	}
	break;
    case RPMGI_HDLIST:
	break;
    case RPMGI_ARGLIST:
	if (gi->argv != NULL && gi->argv[gi->i] != NULL) {
if (_rpmgi_debug  < 0)
fprintf(stderr, "*** gi %p\t%p[%d]: %s\n", gi, gi->argv, gi->i, gi->argv[gi->i]);
	    val = xstrdup(gi->argv[gi->i]);
	} else
	    gi->i = -1;
	break;
    case RPMGI_FTSWALK:
	if (gi->ftsp == NULL && gi->i == 0)
	    gi->ftsp = Fts_open(gi->argv, gi->ftsOpts, NULL);

	while (val == NULL && (gi->fts = Fts_read(gi->ftsp)) != NULL) {
	    FTSENT * fts = gi->fts;

if (_rpmgi_debug < 0)
fprintf(stderr, "FTS_%s\t%*s %s\n", ftsInfoStr(fts->fts_info),
		indent * (fts->fts_level < 0 ? 0 : fts->fts_level), "",
		fts->fts_name);

	    switch (fts->fts_info) {
	    case FTS_F:
	    case FTS_SL:
if (_rpmgi_debug  < 0)
fprintf(stderr, "*** gi %p\t%p[%d]: %s\n", gi, gi->ftsp, gi->i, fts->fts_path);
		val = xstrdup(fts->fts_path);
		break;
	    default:
		break;
	    }
	}
	if (gi->fts == NULL && gi->ftsp != NULL) {
	    int xx;
	    xx = Fts_close(gi->ftsp);
	    gi->ftsp = NULL;
	    gi->i = -1;
	}
	break;
    }

    return val;
}
/*@=modfilesys@*/
