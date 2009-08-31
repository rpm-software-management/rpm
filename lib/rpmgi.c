/** \ingroup rpmio
 * \file lib/rpmgi.c
 */
#include "system.h"

#include <rpm/rpmtypes.h>
#include <rpm/rpmlib.h>		/* rpmReadPackageFile */
#include <rpm/rpmte.h>		/* XXX rpmElementType */
#include <rpm/rpmts.h>
#include <rpm/rpmgi.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmmacro.h>		/* XXX rpmExpand */
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlog.h>

#include "rpmio/fts.h"
#include "lib/manifest.h"

#include "debug.h"

int _rpmgi_debug = 0;

rpmgiFlags giFlags = RPMGI_NONE;

/** \ingroup rpmgi
 */
struct rpmgi_s {
    rpmts ts;			/*!< Iterator transaction set. */
    rpmTag tag;		/*!< Iterator type. */
    const void * keyp;		/*!< Iterator key. */
    size_t keylen;		/*!< Iterator key length. */

    rpmgiFlags flags;		/*!< Iterator control bits. */
    int active;			/*!< Iterator is active? */
    int i;			/*!< Element index. */
    int errors;
    char * hdrPath;		/*!< Path to current iterator header. */
    Header h;			/*!< Current iterator header. */

    rpmtsi tsi;

    rpmdbMatchIterator mi;

    FD_t fd;

    ARGV_t argv;
    int argc;

    int ftsOpts;
    FTS * ftsp;
    FTSENT * fts;

    int nrefs;			/*!< Reference count. */
};

static const char * const ftsInfoStrings[] = {
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

static const char * ftsInfoStr(int fts_info)
{

    if (!(fts_info >= 1 && fts_info <= 14))
	fts_info = 0;
    return ftsInfoStrings[ fts_info ];
}

/**
 * Open a file after macro expanding path.
 * @todo There are two error messages printed on header, then manifest failures.
 * @param path		file path
 * @param fmode		open mode
 * @return		file handle
 */
static FD_t rpmgiOpen(const char * path, const char * fmode)
{
    char * fn = rpmExpand(path, NULL);
    FD_t fd = Fopen(fn, fmode);

    if (fd == NULL || Ferror(fd)) {
	rpmlog(RPMLOG_ERR, _("open of %s failed: %s\n"), fn, Fstrerror(fd));
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
static Header rpmgiReadHeader(rpmgi gi, const char * path)
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
 * @todo An empty file read as manifest truncates argv returning RPMRC_NOTFOUND.
 * @todo Chained manifests lose an arg someplace.
 * @param gi		generalized iterator
 * @return		RPMRC_OK on success
 */
static rpmRC rpmgiLoadReadHeader(rpmgi gi)
{
    rpmRC rpmrc = RPMRC_NOTFOUND;
    Header h = NULL;

    if (gi->argv != NULL && gi->argv[gi->i] != NULL)
    do {
	char * fn;	/* XXX gi->hdrPath? */

	fn = gi->argv[gi->i];
	if (!(gi->flags & RPMGI_NOHEADER)) {
	    h = rpmgiReadHeader(gi, fn);
	    if (h != NULL)
		rpmrc = RPMRC_OK;
	} else
	    rpmrc = RPMRC_OK;

	if (rpmrc == RPMRC_OK || gi->flags & RPMGI_NOMANIFEST)
	    break;
	if (errno == ENOENT) {
	    break;
	}

	/* Not a header, so try for a manifest. */
	gi->argv[gi->i] = NULL;		/* Mark the insertion point */
	rpmrc = rpmgiLoadManifest(gi, fn);
	if (rpmrc != RPMRC_OK) {
	    gi->argv[gi->i] = fn;	/* Manifest failed, restore fn */
	    rpmlog(RPMLOG_ERR, 
		   _("%s: not an rpm package (or package manifest)\n"), fn);
	    break;
	}
	fn = _free(fn);
	rpmrc = RPMRC_NOTFOUND;
    } while (1);

    if (rpmrc == RPMRC_OK && h != NULL)
	gi->h = headerLink(h);
    h = headerFree(h);

    return rpmrc;
}

/**
 * Filter file tree walk path.
 * @param gi		generalized iterator
 * @return		RPMRC_OK on success
 */
static rpmRC rpmgiWalkPathFilter(rpmgi gi)
{
    FTSENT * fts = gi->fts;
    rpmRC rpmrc = RPMRC_NOTFOUND;
    static const int indent = 2;


if (_rpmgi_debug < 0)
rpmlog(RPMLOG_DEBUG, "FTS_%s\t%*s %s%s\n", ftsInfoStr(fts->fts_info),
		indent * (fts->fts_level < 0 ? 0 : fts->fts_level), "",
		fts->fts_name,
	((fts->fts_info == FTS_D || fts->fts_info == FTS_DP) ? "/" : ""));

    switch (fts->fts_info) {
    case FTS_F:
	/* Ignore all but *.rpm files. */
	if (!rpmFileHasSuffix(fts->fts_name, ".rpm"))
	    break;
	rpmrc = RPMRC_OK;
	break;
    default:
	break;
    }
    return rpmrc;
}

/**
 * Read header from next package, lazily walking file tree.
 * @param gi		generalized iterator
 * @return		RPMRC_OK on success
 */
static rpmRC rpmgiWalkReadHeader(rpmgi gi)
{
    rpmRC rpmrc = RPMRC_NOTFOUND;

    if (gi->ftsp != NULL)
    while ((gi->fts = Fts_read(gi->ftsp)) != NULL) {
	rpmrc = rpmgiWalkPathFilter(gi);
	if (rpmrc == RPMRC_OK)
	    break;
    }

    if (rpmrc == RPMRC_OK) {
	Header h = NULL;
	if (!(gi->flags & RPMGI_NOHEADER)) {
	    /* XXX rpmrc = rpmgiLoadReadHeader(gi); */
	    if (gi->fts != NULL)	/* XXX can't happen */
		h = rpmgiReadHeader(gi, gi->fts->fts_path);
	}
	if (h != NULL)
	    gi->h = headerLink(h);
	h = headerFree(h);
    }

    return rpmrc;
}

/**
 * Append globbed arg list to iterator.
 * @param gi		generalized iterator
 * @param argv		arg list to be globbed (or NULL)
 * @returns		RPMRC_OK on success
 */
static rpmRC rpmgiGlobArgv(rpmgi gi, ARGV_const_t argv)
{
    const char * arg;
    rpmRC rpmrc = RPMRC_OK;
    int ac = 0;
    int xx;

    /* XXX Expand globs only if requested or for gi specific tags */
    if ((gi->flags & RPMGI_NOGLOB)
     || !(gi->tag == RPMDBI_HDLIST || gi->tag == RPMDBI_ARGLIST || gi->tag == RPMDBI_FTSWALK))
    {
	if (argv != NULL) {
	    while (argv[ac] != NULL)
		ac++;
/* XXX argv is not NULL */
	    xx = argvAppend(&gi->argv, argv);
	}
	gi->argc = ac;
	return rpmrc;
    }

    if (argv != NULL)
    while ((arg = *argv++) != NULL) {
	char * t = rpmEscapeSpaces(arg);
	char ** av = NULL;

	xx = rpmGlob(t, &ac, &av);
	xx = argvAppend(&gi->argv, av);
	gi->argc += ac;
	av = argvFree(av);
	t = _free(t);
	ac = 0;
    }
    return rpmrc;
}

/**
 * Return rpmdb match iterator with filters (if any) set.
 * @param gi		generalized iterator
 * @returns		RPMRC_OK on success
 */
static rpmRC rpmgiInitFilter(rpmgi gi)
{
    rpmRC rpmrc = RPMRC_OK;
    ARGV_t av;
    int res = 0;

    gi->mi = rpmtsInitIterator(gi->ts, gi->tag, gi->keyp, gi->keylen);

if (_rpmgi_debug < 0)
fprintf(stderr, "*** gi %p\tmi %p\n", gi, gi->mi);

    if (gi->argv != NULL)
    for (av = gi->argv; *av != NULL; av++) {
	rpmTag tag = RPMTAG_NAME;
	const char * pat;
	char * a, * ae;

	pat = a = xstrdup(*av);
	tag = RPMTAG_NAME;

	/* Parse for "tag=pattern" args. */
	if ((ae = strchr(a, '=')) != NULL) {
	    *ae++ = '\0';
	    tag = rpmTagGetValue(a);
	    if (tag == RPMTAG_NOT_FOUND) {
		rpmlog(RPMLOG_ERR, _("unknown tag: \"%s\"\n"), a);
		res = 1;
	    }
	    pat = ae;
	}

	if (!res) {
if (_rpmgi_debug  < 0)
fprintf(stderr, "\tav %p[%ld]: \"%s\" -> %s ~= \"%s\"\n", gi->argv, (long) (av - gi->argv), *av, rpmTagGetName(tag), pat);
	    res = rpmdbSetIteratorRE(gi->mi, tag, RPMMIRE_DEFAULT, pat);
	}
	a = _free(a);

	if (res == 0)
	    continue;

	rpmrc = RPMRC_FAIL;
	break;
    }

    return rpmrc;
}

rpmgi rpmgiUnlink(rpmgi gi, const char * msg)
{
    if (gi == NULL) return NULL;

if (_rpmgi_debug && msg != NULL)
fprintf(stderr, "--> gi %p -- %d: %s\n", gi, gi->nrefs, msg);

    gi->nrefs--;
    return NULL;
}

rpmgi rpmgiLink(rpmgi gi, const char * msg)
{
    if (gi == NULL) return NULL;
    gi->nrefs++;

if (_rpmgi_debug && msg != NULL)
fprintf(stderr, "--> gi %p ++ %d: %s\n", gi, gi->nrefs, msg);

    return gi;
}

rpmgi rpmgiFree(rpmgi gi)
{
    if (gi == NULL)
	return NULL;

    if (gi->nrefs > 1)
	return rpmgiUnlink(gi, __FUNCTION__);

    (void) rpmgiUnlink(gi, __FUNCTION__);


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
    gi->tsi = rpmtsiFree(gi->tsi);
    gi->mi = rpmdbFreeIterator(gi->mi);
    gi->ts = rpmtsFree(gi->ts);

    memset(gi, 0, sizeof(*gi));		/* XXX trash and burn */
    gi = _free(gi);
    return NULL;
}

rpmgi rpmgiNew(rpmts ts, rpmTag tag, const void * keyp, size_t keylen)
{
    rpmgi gi = xcalloc(1, sizeof(*gi));

    if (gi == NULL)
	return NULL;

    gi->ts = rpmtsLink(ts, __FUNCTION__);
    gi->tag = tag;
    gi->keyp = keyp;
    gi->keylen = keylen;

    gi->flags = 0;
    gi->active = 0;
    gi->i = -1;
    gi->errors = 0;
    gi->hdrPath = NULL;
    gi->h = NULL;

    gi->tsi = NULL;
    gi->mi = NULL;
    gi->fd = NULL;
    gi->argv = argvNew();
    gi->argc = 0;
    gi->ftsOpts = 0;
    gi->ftsp = NULL;
    gi->fts = NULL;

    gi = rpmgiLink(gi, __FUNCTION__);

    return gi;
}

rpmRC rpmgiNext(rpmgi gi)
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

    if (++gi->i >= 0)
    switch (gi->tag) {
    default:
    case RPMDBI_PACKAGES:
	if (!gi->active) {
	    rpmrc = rpmgiInitFilter(gi);
	    if (rpmrc != RPMRC_OK)
	        goto enditer;
	    gi->active = 1;
	}
	if (gi->mi != NULL) {	/* XXX unnecessary */
	    Header h = rpmdbNextIterator(gi->mi);
	    if (h != NULL) {
		if (!(gi->flags & RPMGI_NOHEADER))
		    gi->h = headerLink(h);
		sprintf(hnum, "%u", rpmdbGetIteratorOffset(gi->mi));
		gi->hdrPath = rpmExpand("rpmdb h# ", hnum, NULL);
		rpmrc = RPMRC_OK;
		/* XXX header reference held by iterator, so no headerFree */
	    } else
	        rpmrc = RPMRC_NOTFOUND;
	}
	if (rpmrc != RPMRC_OK) {
	    goto enditer;
	}
	break;
    case RPMDBI_ADDED:
    {	rpmte p;

	if (!gi->active) {
	    gi->tsi = rpmtsiInit(gi->ts);
	    gi->active = 1;
	}
	if ((p = rpmtsiNext(gi->tsi, TR_ADDED)) != NULL) {
	    Header h = rpmteHeader(p);
	    if (h != NULL)
		if (!(gi->flags & RPMGI_NOHEADER)) {
		    gi->h = headerLink(h);
		sprintf(hnum, "%u", (unsigned)gi->i);
		gi->hdrPath = rpmExpand("added h# ", hnum, NULL);
		rpmrc = RPMRC_OK;
		h = headerFree(h);
	    }
	}
	if (rpmrc != RPMRC_OK) {
	    gi->tsi = rpmtsiFree(gi->tsi);
	    goto enditer;
	}
    }	break;
    case RPMDBI_HDLIST:
	if (!gi->active) {
	    const char * path = "/usr/share/comps/%{_arch}/hdlist";
	    gi->fd = rpmgiOpen(path, "r.ufdio");
	    gi->active = 1;
	}
	if (gi->fd != NULL) {
	    Header h = headerRead(gi->fd, HEADER_MAGIC_YES);
	    if (h != NULL) {
		if (!(gi->flags & RPMGI_NOHEADER))
		    gi->h = headerLink(h);
		sprintf(hnum, "%u", (unsigned)gi->i);
		gi->hdrPath = rpmExpand("hdlist h# ", hnum, NULL);
		rpmrc = RPMRC_OK;
		h = headerFree(h);
	    }
	}
	if (rpmrc != RPMRC_OK) {
	    if (gi->fd != NULL) (void) Fclose(gi->fd);
	    gi->fd = NULL;
	    goto enditer;
	}
	break;
    case RPMDBI_ARGLIST: 
	/* XXX gi->active initialize? */
if (_rpmgi_debug  < 0)
fprintf(stderr, "*** gi %p\t%p[%d]: %s\n", gi, gi->argv, gi->i, gi->argv[gi->i]);

	/* 
 	 * Read next header, lazily expanding manifests as found,
 	 * count + skip errors.
 	 */
	rpmrc = RPMRC_NOTFOUND;	
	while (gi->i < gi->argc) {
	    if ((rpmrc = rpmgiLoadReadHeader(gi)) == RPMRC_OK) 
		break;
	    gi->errors++;
	    gi->i++;
        }

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

    if ((gi->flags & RPMGI_TSADD) && gi->h != NULL) {
	/* XXX rpmgi hack: Save header in transaction element. */
	xx = rpmtsAddInstallElement(gi->ts, gi->h, (fnpyKey)gi->hdrPath, 2, NULL);
    }

    return rpmrc;

enditer:
    gi->mi = rpmdbFreeIterator(gi->mi);
    if (gi->flags & RPMGI_TSORDER) {
	rpmts ts = gi->ts;
	rpmps ps;

	/* XXX installed database needs close here. */
	xx = rpmtsCloseDB(ts);
	xx = rpmtsSetDBMode(ts, -1);	/* XXX disable lazy opens */

	xx = rpmtsCheck(ts);

	/* XXX query/verify will need the glop added to a buffer instead. */
	ps = rpmtsProblems(ts);
	if (rpmpsNumProblems(ps) > 0) {
	    /* XXX rpminstall will need RPMLOG_ERR */
	    rpmlog(RPMLOG_INFO, _("Failed dependencies:\n"));
	    if (rpmIsVerbose())
		rpmpsPrint(NULL, ps);
	}
	ps = rpmpsFree(ps);
	rpmtsCleanProblems(ts);

	xx = rpmtsOrder(ts);

	gi->tag = RPMDBI_ADDED;			/* XXX hackery */
	gi->flags &= ~(RPMGI_TSADD|RPMGI_TSORDER);

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
    return (gi != NULL ? gi->h : NULL);
}

rpmts rpmgiTs(rpmgi gi)
{
    return (gi != NULL ? gi->ts : NULL);
}

rpmRC rpmgiSetArgs(rpmgi gi, ARGV_const_t argv, int ftsOpts, rpmgiFlags flags)
{
    gi->ftsOpts = ftsOpts;
    gi->flags = flags;
    return rpmgiGlobArgv(gi, argv);
}

rpmgiFlags rpmgiGetFlags(rpmgi gi)
{
    return (gi != NULL ? gi->flags : RPMGI_NONE);
}

int rpmgiNumErrors(rpmgi gi)
{
    return (gi != NULL ? gi->errors : -1);
}
