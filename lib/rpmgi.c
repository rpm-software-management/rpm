/** \ingroup rpmio
 * \file lib/rpmgi.c
 */
#include "system.h"

#include <errno.h>

#include <rpm/rpmtypes.h>
#include <rpm/rpmlib.h>		/* rpmReadPackageFile */
#include <rpm/rpmte.h>		/* XXX rpmElementType */
#include <rpm/rpmts.h>
#include <rpm/rpmgi.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmmacro.h>		/* XXX rpmExpand */
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlog.h>

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

    ARGV_t argv;
    int argc;
};

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
    if ((gi->flags & RPMGI_NOGLOB) || !(gi->tag == RPMDBI_ARGLIST)) {
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

rpmgi rpmgiFree(rpmgi gi)
{
    if (gi == NULL)
	return NULL;

    gi->hdrPath = _free(gi->hdrPath);
    gi->h = headerFree(gi->h);

    gi->argv = argvFree(gi->argv);

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
    gi->argv = argvNew();
    gi->argc = 0;

    return gi;
}

rpmRC rpmgiNext(rpmgi gi)
{
    char hnum[32];
    rpmRC rpmrc = RPMRC_NOTFOUND;

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
    }

    return rpmrc;

enditer:
    gi->mi = rpmdbFreeIterator(gi->mi);
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

rpmRC rpmgiSetArgs(rpmgi gi, ARGV_const_t argv, rpmgiFlags flags)
{
    gi->flags = flags;
    return rpmgiGlobArgv(gi, argv);
}

int rpmgiNumErrors(rpmgi gi)
{
    return (gi != NULL ? gi->errors : -1);
}
