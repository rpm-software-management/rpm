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
 * Read next header from package, lazily expanding manifests as found.
 * @param gi		generalized iterator
 * @return		RPMRC_OK on success
 */
/*@null@*/
static rpmRC rpmgiLoadReadHeader(rpmgi gi)
	/*@globals rpmGlobalMacroContext, h_errno, internalState @*/
	/*@modifies gi, rpmGlobalMacroContext, h_errno, internalState @*/
{
    rpmRC rpmrc = RPMRC_NOTFOUND;
    Header h = NULL;

    do {
	const char * fn;	/* XXX gi->hdrPath? */

	fn = gi->argv[gi->i];
	h = rpmgiReadHeader(gi, fn);
	if (h != NULL || (gi->flags & RPMGI_NOHEADER)) {
	    rpmrc = RPMRC_OK;
	    break;
	}

	if (gi->flags & RPMGI_NOMANIFEST)
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

    if (rpmrc == RPMRC_OK && h != NULL && !(gi->flags & RPMGI_NOHEADER))
	gi->h = headerLink(h);
    h = headerFree(h);

    return rpmrc;
}

/**
 * Read next header from package, lazily walking file tree.
 * @param gi		generalized iterator
 * @return		RPMRC_OK on success
 */
/*@null@*/
static rpmRC rpmgiWalkReadHeader(rpmgi gi)
	/*@globals rpmGlobalMacroContext, h_errno, internalState @*/
	/*@modifies gi, rpmGlobalMacroContext, h_errno, internalState @*/
{
    rpmRC rpmrc = RPMRC_NOTFOUND;
    Header h = NULL;
    int bingo = 0;

    if (gi->ftsp != NULL)
    while ((gi->fts = Fts_read(gi->ftsp)) != NULL) {
	FTSENT * fts = gi->fts;

if (_rpmgi_debug < 0)
fprintf(stderr, "FTS_%s\t%*s %s\n", ftsInfoStr(fts->fts_info),
		indent * (fts->fts_level < 0 ? 0 : fts->fts_level), "",
		fts->fts_name);

/*@=branchstate@*/
	switch (fts->fts_info) {
	case FTS_F:
	case FTS_SL:
if (_rpmgi_debug  < 0)
fprintf(stderr, "*** gi %p\t%p[%d]: %s\n", gi, gi->ftsp, gi->i, fts->fts_path);
	    bingo = 1;
	    /*@switchbreak@*/ break;
	default:
	    /*@switchbreak@*/ break;
	}
/*@=branchstate@*/
	if (bingo) {
	    if (!(gi->flags & RPMGI_NOHEADER))
		h = rpmgiReadHeader(gi, fts->fts_path);
	    rpmrc = RPMRC_OK;
	    break;
	}
    }

    if (rpmrc == RPMRC_OK && h != NULL)
	gi->h = headerLink(h);
    h = headerFree(h);

    return rpmrc;
}

/**
 * Append globbed arg list to iterator.
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
    int ac = 0;
    int xx;

    if (gi->flags & RPMGI_NOGLOB) {
	while (argv[ac] != NULL)
	    ac++;
	xx = argvAppend(&gi->argv, argv);
	gi->argc = ac;
	return rpmrc;
    }

    if (argv != NULL)
    while ((arg = *argv++) != NULL) {
	ARGV_t av = NULL;

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

rpmgi rpmgiNew(rpmts ts, int tag, const void * keyp, size_t keylen)
{
    rpmgi gi = xcalloc(1, sizeof(*gi));

    if (gi == NULL)
	return NULL;

    gi->ts = rpmtsLink(ts, NULL);
    gi->tag = tag;
    gi->keyp = keyp;
    gi->keylen = keylen;

    gi->flags = 0;
    gi->active = 0;
    gi->i = -1;
    gi->hdrPath = NULL;
    gi->h = NULL;

    gi->mi = NULL;
    gi->fd = NULL;
    gi->argv = xcalloc(1, sizeof(*gi->argv));
    gi->argc = 0;
    gi->ftsOpts = 0;
    gi->ftsp = NULL;
    gi->fts = NULL;

    gi = rpmgiLink(gi, NULL);

    return gi;
}

rpmRC rpmgiNext(/*@null@*/ rpmgi gi)
{
    char hnum[32];
    rpmRC rpmrc = RPMRC_NOTFOUND;
    int xx;

    if (gi == NULL)
	return rpmrc;

    /* Free header from previous iteration. */
    gi->h = headerFree(gi->h);
    gi->hdrPath = _free(gi->hdrPath);
    hnum[0] = '\0';

/*@-branchstate@*/
    if (++gi->i >= 0)
    switch (gi->tag) {
    default:
    case RPMDBI_PACKAGES:
	if (!gi->active) {
	    gi->mi = rpmtsInitIterator(gi->ts, gi->tag, gi->keyp, gi->keylen);
if (_rpmgi_debug < 0)
fprintf(stderr, "*** gi %p\t%p\n", gi, gi->mi);
	    gi->active = 1;
	}
	if (gi->mi != NULL) {	/* XXX unnecessary */
	    Header h = rpmdbNextIterator(gi->mi);
	    if (h != NULL) {
		if (!(gi->flags & RPMGI_NOHEADER))
		    gi->h = headerLink(h);
		sprintf(hnum, "%u", rpmdbGetIteratorOffset(gi->mi));
		rpmrc = RPMRC_OK;
		/* XXX header reference held by iterator, so no headerFree */
	    }
	}
	if (rpmrc != RPMRC_OK) {
	    gi->mi = rpmdbFreeIterator(gi->mi);
	    goto enditer;
	}
	gi->hdrPath = rpmExpand("rpmdb h# ", hnum, NULL);
	break;
    case RPMDBI_HDLIST:
	if (!gi->active) {
	    const char * path = "/usr/share/comps/%{_arch}/hdlist";
	    gi->fd = rpmgiOpen(path, "r.ufdio");
	    gi->active = 1;
	}
	if (gi->fd != NULL) {
	    Header h = headerRead(gi->fd, HEADER_MAGIC_YES);
	    if (h != NULL && !(gi->flags & RPMGI_NOHEADER))
		gi->h = headerLink(h);
	    sprintf(hnum, "%u", (unsigned)gi->i);
	    rpmrc = RPMRC_OK;
	    h = headerFree(h);
	}

	if (rpmrc != RPMRC_OK) {
	    if (gi->fd != NULL) (void) Fclose(gi->fd);
	    gi->fd = NULL;
	    goto enditer;
	}
	gi->hdrPath = rpmExpand("hdlist h# ", hnum, NULL);
	break;
    case RPMDBI_ARGLIST:
	if (gi->argv == NULL || gi->argv[gi->i] == NULL)
	    goto enditer;

if (_rpmgi_debug  < 0)
fprintf(stderr, "*** gi %p\t%p[%d]: %s\n", gi, gi->argv, gi->i, gi->argv[gi->i]);
	/* Read next header, lazily expanding manifests as found. */
	rpmrc = rpmgiLoadReadHeader(gi);

	if (rpmrc != RPMRC_OK)	/* XXX check this */
	    goto enditer;

	gi->hdrPath = xstrdup(gi->argv[gi->i]);
	break;
    case RPMDBI_FTSWALK:
	if (gi->argv == NULL)		/* HACK */
	    goto enditer;

	if (!gi->active) {
	    gi->ftsp = Fts_open((char *const *)gi->argv, gi->ftsOpts, NULL);
	    /* XXX NULL with open(2)/malloc(3) errno set */
	    gi->active = 1;
	}

	/* Read next header, lazily walking file tree. */
	rpmrc = rpmgiWalkReadHeader(gi);

	if (rpmrc != RPMRC_OK) {
	    xx = Fts_close(gi->ftsp);
	    gi->ftsp = NULL;
	    goto enditer;
	}

	if (gi->fts != NULL)
	    gi->hdrPath = xstrdup(gi->fts->fts_path);
	break;
    }
/*@=branchstate@*/

    if ((gi->flags & RPMGI_TSADD) && gi->h != NULL) {
	xx = rpmtsAddInstallElement(gi->ts, gi->h, (fnpyKey)gi->hdrPath, 0, NULL);
    }

    return rpmrc;

enditer:
    if (gi->flags & RPMGI_TSORDER) {
	xx = rpmtsCheck(gi->ts);
	xx = rpmtsOrder(gi->ts);
    }
    gi->h = headerFree(gi->h);
    gi->hdrPath = _free(gi->hdrPath);
    gi->i = -1;
    gi->active = 0;
    return rpmrc;
}

const char * rpmgiHdrPath(rpmgi gi)
{
    return (gi != NULL ? gi->hdrPath : NULL);
}

Header rpmgiHeader(rpmgi gi)
{
/*@-compdef -refcounttrans -retexpose -usereleased@*/
    return (gi != NULL ? gi->h : NULL);
/*@=compdef =refcounttrans =retexpose =usereleased@*/
}

rpmts rpmgiTs(rpmgi gi)
{
/*@-compdef -refcounttrans -retexpose -usereleased@*/
    return (gi != NULL ? gi->ts : NULL);
/*@=compdef =refcounttrans =retexpose =usereleased@*/
}

rpmRC rpmgiSetArgs(rpmgi gi, ARGV_t argv, int ftsOpts, rpmgiFlags flags)
{
    gi->ftsOpts = ftsOpts;
    gi->flags = flags;
    return rpmgiGlobArgv(gi, argv);
}

/*@=modfilesys@*/
