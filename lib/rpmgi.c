/*@-modfilesys@*/
/** \ingroup rpmio
 * \file rpmio/rpmio.c
 */
#include "system.h"

#define	_RPMGI_INTERNAL
#include <rpmgi.h>

#include <rpmdb.h>
#include <rpmmacro.h>		/* XXX rpmExpand */
#include "manifest.h"

#include "debug.h"

/*@access rpmdbMatchIterator @*/

/*@unchecked@*/
int _rpmgi_debug = 0;

/*@unchecked@*/
static int indent = 2;

/*@unchecked@*/ /*@observer@*/
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

/*@observer@*/
static const char * ftsInfoStr(int fts_info)
	/*@*/
{

    if (!(fts_info >= 1 && fts_info <= 14))
	fts_info = 0;
/*@-compmempass@*/
    return ftsInfoStrings[ fts_info ];
/*@=compmempass@*/
}

/**
 * Open a file after macro expanding path.
 * @param path		file path
 * @param fmode		open mode
 * @return		file handle
 */
/*@null@*/
static FD_t rpmgiOpen(const char * path, const char * fmode)
	/*@globals rpmGlobalMacroContext, h_errno, internalState @*/
	/*@modifies rpmGlobalMacroContext, h_errno, internalState @*/
{
    const char * fn = rpmExpand(path, NULL);
    FD_t fd = Fopen(fn, fmode);

    if (fd == NULL || Ferror(fd)) {
	rpmError(RPMERR_OPEN, _("open of %s failed: %s\n"), fn, Fstrerror(fd));
	if (fd != NULL) (void) Fclose(fd);
	fd = NULL;
    }
    fn = _free(fn);

    return fd;
}

/**
 * Load manifest into iterator arg list.
 * @param gi		generalized iterator
 * @param path		file path
 * @return		RPMRC_OK on success
 */
static rpmRC rpmgiLoadManifest(rpmgi gi, const char * path)
	/*@globals rpmGlobalMacroContext, h_errno, internalState @*/
	/*@modifies gi, rpmGlobalMacroContext, h_errno, internalState @*/
{
    FD_t fd = rpmgiOpen(path, "r.ufdio");
    rpmRC rpmrc = RPMRC_FAIL;

    if (fd != NULL) {
	rpmrc = rpmReadPackageManifest(fd, &gi->argc, &gi->argv);
	(void) Fclose(fd);
    }
    return rpmrc;
}

/**
 * Return header from package.
 * @param gi		generalized iterator
 * @param path		file path
 * @return		header (NULL on failure)
 */
/*@null@*/
static Header rpmgiReadHeader(rpmgi gi, const char * path)
	/*@globals rpmGlobalMacroContext, h_errno, internalState @*/
	/*@modifies gi, rpmGlobalMacroContext, h_errno, internalState @*/
{
    FD_t fd = rpmgiOpen(path, "r.ufdio");
    Header h = NULL;

    if (fd != NULL) {
	/* XXX what if path needs expansion? */
	rpmRC rpmrc = rpmReadPackageFile(gi->ts, fd, path, &h);

	(void) Fclose(fd);

	switch (rpmrc) {
	case RPMRC_NOTFOUND:
	    /* XXX Read a package manifest. Restart ftswalk on success. */
	case RPMRC_FAIL:
	default:
	    h = headerFree(h);
	    break;
	case RPMRC_NOTTRUSTED:
	case RPMRC_NOKEY:
	case RPMRC_OK:
	    break;
	}
    }

    return h;
}

/**
 * Return next header, lazily expanding manifests as found.
 * @param gi		generalized iterator
 * @return		header (NULL on failure)
 */
/*@null@*/
static Header rpmgiLoadReadHeader(rpmgi gi)
	/*@globals rpmGlobalMacroContext, h_errno, internalState @*/
	/*@modifies gi, rpmGlobalMacroContext, h_errno, internalState @*/
{
    Header h;
    rpmRC rpmrc = RPMRC_OK;

    do {
	const char * fn;

	fn = gi->argv[gi->i];
	h = rpmgiReadHeader(gi, fn);
	if (h != NULL)
	    break;
	/* Not a header, so try for a manifest. */
	gi->argv[gi->i] = NULL;		/* HACK */
	rpmrc = rpmgiLoadManifest(gi, fn);
	if (rpmrc != RPMRC_OK) {
	    gi->argv[gi->i] = fn;	/* HACK */
	    break;
	}
	fn = _free(fn);
    } while (1);

    /* XXX check rpmrc */

    return h;
}

/**
 * Return next header, lazily walking file tree.
 * @param gi		generalized iterator
 * @return		header (NULL on failure)
 */
/*@null@*/
static Header rpmgiWalkReadHeader(rpmgi gi)
	/*@globals rpmGlobalMacroContext, h_errno, internalState @*/
	/*@modifies gi, rpmGlobalMacroContext, h_errno, internalState @*/
{
    Header h = NULL;

    if (gi->ftsp != NULL)
    while (h == NULL && (gi->fts = Fts_read(gi->ftsp)) != NULL) {
	FTSENT * fts = gi->fts;

if (_rpmgi_debug < 0)
fprintf(stderr, "FTS_%s\t%*s %s\n", ftsInfoStr(fts->fts_info),
		indent * (fts->fts_level < 0 ? 0 : fts->fts_level), "",
		fts->fts_name);

/*@-branchstate@*/
	switch (fts->fts_info) {
	case FTS_F:
	case FTS_SL:
if (_rpmgi_debug  < 0)
fprintf(stderr, "*** gi %p\t%p[%d]: %s\n", gi, gi->ftsp, gi->i, fts->fts_path);
	    h = rpmgiReadHeader(gi, fts->fts_path);
	    /*@switchbreak@*/ break;
	default:
	    /*@switchbreak@*/ break;
	}
/*@=branchstate@*/
    }

    return h;
}

/**
 * Load globbed arg list into iterator.
 * @param gi		generalized iterator
 * @param argv		arg list to be globbed
 * @returns		RPMRC_OK on success
 */
static rpmRC rpmgiGlobArgv(rpmgi gi, ARGV_t argv)
	/*@globals internalState @*/
	/*@modifies gi, internalState @*/
{
    const char * arg;
    rpmRC rpmrc = RPMRC_OK;

    if (gi->argv != NULL)
	gi->argv = argvFree(gi->argv);

    gi->argv = xcalloc(1, sizeof(*gi->argv));
    gi->argc = 0;
    if (argv != NULL)
    while ((arg = *argv++) != NULL) {
	ARGV_t av = NULL;
	int ac = 0;
	int xx;

	xx = rpmGlob(arg, &ac, &av);
	xx = argvAppend(&gi->argv, av);
	gi->argc += ac;
	av = argvFree(av);
	ac = 0;
    }
    return rpmrc;
}

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

/*@-usereleased@*/
#ifdef	NOTYET
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
#endif

    gi->queryFormat = _free(gi->queryFormat);
    gi->hdrPath = _free(gi->hdrPath);
    gi->h = headerFree(gi->h);

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
/*@-refcounttrans@*/
    gi = _free(gi);
/*@=refcounttrans@*/
/*@=usereleased@*/
    return NULL;
}

rpmgi rpmgiNew(rpmts ts, int tag, void *const keyp, size_t keylen)
{
    rpmgi gi = xcalloc(1, sizeof(*gi));

    if (gi == NULL)
	return NULL;

    gi->ts = rpmtsLink(ts, NULL);
    gi->tag = tag;
    gi->active = 0;
    gi->i = -1;

    gi->queryFormat = NULL;
    gi->hdrPath = NULL;
    gi->h = NULL;
    gi->mi = NULL;
    gi->fd = NULL;
    gi->argv = NULL;
    gi->argc = 0;
    gi->ftsOpts = 0;
    gi->ftsp = NULL;
    gi->fts = NULL;

    switch (gi->tag) {
    default:
    case RPMGI_RPMDB:
	break;
    case RPMGI_HDLIST:
	break;
    case RPMGI_ARGLIST:
    case RPMGI_FTSWALK:
    {	ARGV_t argv = keyp;		/* HACK */
	unsigned flags = keylen;	/* HACK */
	rpmRC rpmrc;

	rpmrc = rpmgiGlobArgv(gi, argv);
	gi->ftsOpts = flags;

if (_rpmgi_debug < 0)
fprintf(stderr, "*** gi %p\t%p[%d]\n", gi, gi->argv, gi->argc);

    }	break;
    }

    gi = rpmgiLink(gi, NULL);

    return gi;
}

/*@only@*/
static const char * rpmgiPathOrQF(const rpmgi gi, const char * fn,
		/*@null@*/ Header * hdrp)
	/*@modifies *hdrp @*/
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

const char * rpmgiNext(/*@null@*/ rpmgi gi)
{
    const char * val = NULL;
    const char * fn = NULL;
    Header h = NULL;

    if (gi != NULL && ++gi->i >= 0)
    switch (gi->tag) {
    default:
    case RPMGI_RPMDB:
	if (!gi->active) {
	    gi->mi = rpmtsInitIterator(gi->ts, RPMDBI_PACKAGES, NULL, 0);
if (_rpmgi_debug < 0)
fprintf(stderr, "*** gi %p\t%p\n", gi, gi->mi);
	    gi->active = 1;
	}
	if (gi->mi != NULL)	/* XXX unnecessary */
	    h = rpmdbNextIterator(gi->mi);
/*@-branchstate@*/
	if (h != NULL) {
	    val = rpmgiPathOrQF(gi, "rpmdb", &h);
	} else {
	    gi->mi = rpmdbFreeIterator(gi->mi);
	    gi->i = -1;
	    gi->active = 0;
	}
/*@=branchstate@*/
	break;
    case RPMGI_HDLIST:
	if (!gi->active) {
	    const char * hdrPath = gi->hdrPath;

/*@-branchstate@*/
	    if (hdrPath == NULL)
		hdrPath = "/usr/share/comps/%{_arch}/hdlist";
/*@=branchstate@*/
	    gi->fd = rpmgiOpen(hdrPath, "r.ufdio");
	    gi->active = 1;
	}
	if (gi->fd != NULL)
	    h = headerRead(gi->fd, HEADER_MAGIC_YES);
/*@-branchstate@*/

	if (h != NULL) {
	    val = rpmgiPathOrQF(gi, "hdlist", &h);
	} else {
	    if (gi->fd != NULL) (void) Fclose(gi->fd);
	    gi->fd = NULL;
	    gi->i = -1;
	    gi->active = 0;
	}
/*@=branchstate@*/
	break;
    case RPMGI_ARGLIST:
/*@-branchstate@*/
	if (gi->argv != NULL && gi->argv[gi->i] != NULL) {
if (_rpmgi_debug  < 0)
fprintf(stderr, "*** gi %p\t%p[%d]: %s\n", gi, gi->argv, gi->i, gi->argv[gi->i]);
	    /* Read next header, lazily expanding manifests as found. */
	    h = rpmgiLoadReadHeader(gi);

/*@-compdef@*/	/* XXX WTF? gi->argv undefined */
	    val = rpmgiPathOrQF(gi, fn, &h);
/*@=compdef@*/
	    h = headerFree(h);
	} else {
	    gi->i = -1;
	    gi->active = 0;
	}
/*@=branchstate@*/
	break;
    case RPMGI_FTSWALK:
	if (gi->argv == NULL)		/* HACK */
	    break;
	if (!gi->active) {
	    gi->ftsp = Fts_open((char *const *)gi->argv, gi->ftsOpts, NULL);
	    /* XXX NULL with open(2)/malloc(3) errno set */
	    gi->active = 1;
	}

	if (gi->ftsp != NULL)
	    h = rpmgiWalkReadHeader(gi);

/*@-branchstate@*/
	if (h != NULL) {
	    if (gi->fts != NULL)
		fn = gi->fts->fts_path;
	    val = rpmgiPathOrQF(gi, fn, &h);
	    h = headerFree(h);
	}
/*@=branchstate@*/

	if (gi->fts == NULL && gi->ftsp != NULL) {
	    int xx;
	    xx = Fts_close(gi->ftsp);
	    gi->ftsp = NULL;
	    gi->i = -1;
	    gi->active = 0;
	}
	break;
    }

    return val;
}

const char * rpmgiQueryFormat(rpmgi gi)
{
    const char * queryFormat = NULL;

    if (gi != NULL)
        queryFormat = gi->queryFormat;
    return queryFormat;
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

const char * rpmgiHdrPath(rpmgi gi)
{
    const char * hdrPath = NULL;

    if (gi != NULL)
        hdrPath = gi->hdrPath;
    return hdrPath;
}

int rpmgiSetHdrPath(rpmgi gi, const char * hdrPath)
{
    int rc = 0;

    if (gi != NULL) {
	gi->hdrPath = _free(gi->hdrPath);
        gi->hdrPath = (hdrPath != NULL ? xstrdup(hdrPath) : NULL);
    }
    return rc;
}

/*@=modfilesys@*/
