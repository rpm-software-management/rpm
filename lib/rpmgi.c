/** \ingroup rpmio
 * \file lib/rpmgi.c
 */
#include "system.h"

#include <errno.h>

#include <rpm/rpmtypes.h>
#include <rpm/rpmlib.h>		/* rpmReadPackageFile */
#include <rpm/rpmts.h>
#include <rpm/rpmgi.h>
#include <rpm/rpmmacro.h>		/* XXX rpmExpand */
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlog.h>

#include "lib/manifest.h"

#include "debug.h"

rpmgiFlags giFlags = RPMGI_NONE;

/** \ingroup rpmgi
 */
struct rpmgi_s {
    rpmts ts;			/*!< Iterator transaction set. */

    rpmgiFlags flags;		/*!< Iterator control bits. */
    int i;			/*!< Element index. */
    int errors;
    Header h;			/*!< Current iterator header. */

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
	char * fn = gi->argv[gi->i];
	h = rpmgiReadHeader(gi, fn);
	if (h != NULL)
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
 */
static void rpmgiGlobArgv(rpmgi gi, ARGV_const_t argv)
{
    const char * arg;
    int ac = 0;
    int xx;

    /* XXX Expand globs only if requested */
    if ((gi->flags & RPMGI_NOGLOB)) {
	if (argv != NULL) {
	    while (argv[ac] != NULL)
		ac++;
	    xx = argvAppend(&gi->argv, argv);
	}
	gi->argc = ac;
	return;
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
    return;
}

rpmgi rpmgiFree(rpmgi gi)
{
    if (gi == NULL)
	return NULL;

    gi->h = headerFree(gi->h);

    gi->argv = argvFree(gi->argv);

    memset(gi, 0, sizeof(*gi));		/* XXX trash and burn */
    gi = _free(gi);
    return NULL;
}

rpmgi rpmgiNew(rpmts ts, rpmgiFlags flags, ARGV_const_t argv)
{
    rpmgi gi = xcalloc(1, sizeof(*gi));

    gi->ts = rpmtsLink(ts, __FUNCTION__);

    gi->flags = flags;
    gi->i = -1;
    gi->errors = 0;
    gi->h = NULL;

    gi->flags = flags;
    gi->argv = argvNew();
    gi->argc = 0;
    rpmgiGlobArgv(gi, argv);

    return gi;
}

Header rpmgiNext(rpmgi gi)
{
    rpmRC rpmrc = RPMRC_NOTFOUND;

    if (gi == NULL)
	return NULL;

    /* Free header from previous iteration. */
    gi->h = headerFree(gi->h);

    if (++gi->i >= 0) {
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
    }

    return gi->h;

enditer:
    gi->h = headerFree(gi->h);
    gi->i = -1;
    return NULL;
}

int rpmgiNumErrors(rpmgi gi)
{
    return (gi != NULL ? gi->errors : -1);
}
