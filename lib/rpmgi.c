/*@-modfilesys@*/
/** \ingroup rpmio
 * \file rpmio/rpmio.c
 */
#include "system.h"

#define	_RPMGI_INTERNAL
#include <rpmgi.h>

#include <rpmdb.h>
#include "manifest.h"

#include "debug.h"

/*@unchecked@*/
int _rpmgi_debug = 0;

static const char * hdlistpath = "/usr/share/comps/i386/hdlist";

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

    gi->queryFormat = _free(gi->queryFormat);
    gi->argv = argvFree(gi->argv);
    if (gi->ftsp != NULL) {
	int xx;
	xx = Fts_close(gi->ftsp);
	gi->ftsp = NULL;
	gi->fts = NULL;
    }
    if (gi->fd != NULL) {
	(void) Fclose(gi->fd);
	gi->fd = NULL;
    }
    gi->mi = rpmdbFreeIterator(gi->mi);
    gi->ts = rpmtsFree(gi->ts);

    memset(gi, 0, sizeof(*gi));		/* XXX trash and burn */
    gi = _free(gi);
    return NULL;
}

rpmgi rpmgiNew(rpmts ts, int tag, void *const keyp, size_t keylen)
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
	gi->fd = Fopen(hdlistpath, "r.ufdio");
	break;
    case RPMGI_ARGLIST:
    case RPMGI_FTSWALK:
    {   ARGV_t pav = keyp;
	const char * arg;
	unsigned flags = keylen;
	int xx;

	gi->argv = xcalloc(1, sizeof(*gi->argv));
	gi->argc = 0;
	if (pav != NULL)
	while ((arg = *pav++) != NULL) {
	    ARGV_t av = NULL;
	    int ac;

	    xx = rpmGlob(arg, &ac, &av);
	    xx = argvAppend(&gi->argv, av);
	    gi->argc += ac;
	    av = argvFree(av);
	}
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

/*@only@*/
static const char * rpmgiPathOrQF(rpmgi gi, const char * fn,
		/*@null@*/ Header * hdrp)
	/*@modifies gi, *hdrp @*/
{
    const char * fmt = ((gi->queryFormat != NULL)
	? gi->queryFormat : "%{name}-%{version}-%{release}");
    const char * val = NULL;
    Header h = NULL;

    if (hdrp != NULL && *hdrp != NULL)
	h = headerLink(*hdrp);

    if (h != NULL)
	val = headerSprintf(h, fmt, rpmTagTable, rpmHeaderFormats, NULL);
    else
	val = xstrdup(fn);

    h = headerFree(h);

    return val;
}

/*@null@*/
static rpmRC rpmgiReadManifest(rpmgi gi, const char * fileURL)
	/*@*/
{
    rpmRC rpmrc;
    FD_t fd;

    fd = Fopen(fileURL, "r.ufdio");
    if (fd == NULL || Ferror(fd)) {
	rpmError(RPMERR_OPEN, _("open of %s failed: %s\n"), fileURL,
                        Fstrerror(fd));
	if (fd != NULL) (void) Fclose(fd);
	return RPMRC_FAIL;
    }

    rpmrc = rpmReadPackageManifest(fd, &gi->argc, &gi->argv);

    (void) Fclose(fd);

    return rpmrc;
}

/*@null@*/
static Header rpmgiReadHeader(rpmgi gi, const char * fileURL)
	/*@*/
{
    Header h = NULL;
    rpmRC rpmrc;
    FD_t fd;

    fd = Fopen(fileURL, "r.ufdio");
    if (fd == NULL || Ferror(fd)) {
	rpmError(RPMERR_OPEN, _("open of %s failed: %s\n"), fileURL,
                        Fstrerror(fd));
	if (fd != NULL) (void) Fclose(fd);
	return h;
    }

    rpmrc = rpmReadPackageFile(gi->ts, fd, fileURL, &h);

    (void) Fclose(fd);

    switch (rpmrc) {
    case RPMRC_NOTFOUND:
	/* XXX Try to read a package manifest. Restart ftswalk on success. */
    case RPMRC_FAIL:
    default:
	h = headerFree(h);
	break;
    case RPMRC_NOTTRUSTED:
    case RPMRC_NOKEY:
    case RPMRC_OK:
	break;
    }

    return h;
}

const char * rpmgiNext(/*@null@*/ rpmgi gi)
	/*@modifies gi @*/
{
    const char * val = NULL;
    const char * fn = NULL;
    Header h = NULL;
    rpmRC rpmrc;

    if (gi != NULL && ++gi->i >= 0)
    switch (gi->tag) {
    default:
    case RPMGI_RPMDB:
	h = rpmdbNextIterator(gi->mi);
	if (h != NULL) {
	    val = rpmgiPathOrQF(gi, "rpmdb", &h);
	} else {
	    gi->mi = rpmdbFreeIterator(gi->mi);
	    gi->i = -1;
	}
	break;
    case RPMGI_HDLIST:
	h = headerRead(gi->fd, HEADER_MAGIC_YES);
	if (h != NULL) {
	    val = rpmgiPathOrQF(gi, "hdlist", &h);
	} else {
	    (void) Fclose(gi->fd);
	    gi->fd = NULL;
	    gi->i = -1;
	}
	break;
    case RPMGI_ARGLIST:
	if (gi->argv != NULL && gi->argv[gi->i] != NULL) {
if (_rpmgi_debug  < 0)
fprintf(stderr, "*** gi %p\t%p[%d]: %s\n", gi, gi->argv, gi->i, gi->argv[gi->i]);
	    /* Read next header, lazily expanding manifests as found. */
	    do {
		fn = gi->argv[gi->i];
		h = rpmgiReadHeader(gi, fn);
		if (h != NULL)
		    break;
		/* Not a header, so try for a manifest. */
		gi->argv[gi->i] = NULL;
		rpmrc = rpmgiReadManifest(gi, fn);
		if (rpmrc != RPMRC_OK) {
		    gi->argv[gi->i] = fn;
		    break;
		}
		fn = _free(fn);
	    } while (1);
	    /* XXX check rpmrc */
	    val = rpmgiPathOrQF(gi, fn, &h);
	    h = headerFree(h);
	} else
	    gi->i = -1;
	break;
    case RPMGI_FTSWALK:
	if (gi->argv == NULL)
	    break;
	if (gi->ftsp == NULL && gi->i == 0) {
	    gi->ftsp = Fts_open((char *const *)gi->argv, gi->ftsOpts, NULL);
	    /* XXX NULL with open(2)/malloc(3) errno set */
	}

	if (gi->ftsp != NULL)
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
		fn = fts->fts_path;
		h = rpmgiReadHeader(gi, fn);
		val = rpmgiPathOrQF(gi, fn, &h);
		h = headerFree(h);
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

int rpmgiSetQueryFormat(rpmgi gi, const char * queryFormat)
{
    int rc = 0;
                                                                                
    if (gi != NULL) {
	gi->queryFormat = _free(gi->queryFormat);
        gi->queryFormat = (queryFormat != NULL ? xstrdup(queryFormat) : NULL);
    }
    return rc;

}

/*@=modfilesys@*/
