/**
 * \file tools/rpmcache.c
 */

#include "system.h"
const char *__progname;

#include <fnmatch.h>
#include <fts.h>

#include <rpmcli.h>

#include "rpmps.h"
#include "rpmdb.h"
#include "rpmds.h"
#include "rpmts.h"

#include "debug.h"
#include "misc.h"

static int _debug = 0;

/* XXX should be flag in ts */
static int noCache = 0;

static char ** ftsSet;

const char * bhpath;
int bhpathlen = 0;
int bhlvl = -1;

struct ftsglob_s {
    const char ** patterns;
    int fnflags;
};

static struct ftsglob_s * bhglobs;
static int nbhglobs = 5;

static int indent = 2;

typedef struct Item_s {
    const char * path;
    int_32 size;
    int_32 mtime;
    rpmds this;
    Header h;
} * Item;

static Item * items = NULL;
static int nitems = 0;

static inline Item freeItem(Item item) {
    if (item != NULL) {
	item->path = _free(item->path);
	item->this = rpmdsFree(item->this);
	item->h = headerFree(item->h);
	item = _free(item);
    }
    return NULL;
}

static inline Item newItem(void) {
    Item item = xcalloc(1, sizeof(*item));
    return item;
}

static int cmpItem(const void * a, const void * b) {
    Item aitem = *(Item *)a;
    Item bitem = *(Item *)b;
    int rc = strcmp(rpmdsN(aitem->this), rpmdsN(bitem->this));
    return rc;
}

static void freeItems(void) {
    int i;
    for (i = 0; i < nitems; i++)
	items[i] = freeItem(items[i]);
    items = _free(items);
    nitems = 0;
}

static int ftsCachePrint(/*@unused@*/ rpmts ts, FILE * fp)
{
    int rc = 0;
    int i;

    if (fp == NULL) fp = stdout;
    for (i = 0; i < nitems; i++) {
	Item ip;

	ip = items[i];
	if (ip == NULL) {
	    rc = 1;
	    break;
	}

	fprintf(fp, "%s\n", ip->path);
    }
    return rc;
}

static int ftsCacheUpdate(rpmts ts)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    int_32 tid = rpmtsGetTid(ts);
    rpmdbMatchIterator mi;
    unsigned char * md5;
    int rc = 0;
    int i;

    rc = rpmtsCloseDB(ts);
    rc = rpmDefineMacro(NULL, "_dbpath %{_cache_dbpath}", RMIL_CMDLINE);
    rc = rpmtsOpenDB(ts, O_RDWR);
    if (rc != 0)
	return rc;

    for (i = 0; i < nitems; i++) {
	Item ip;

	ip = items[i];
	if (ip == NULL) {
	    rc = 1;
	    break;
	}

	/* --- Check that identical package is not already cached. */
 	if (!hge(ip->h, RPMTAG_SIGMD5, NULL, (void **) &md5, NULL)
	 || md5 == NULL)
	{
	    rc = 1;
	    break;
	}
        mi = rpmtsInitIterator(ts, RPMTAG_SIGMD5, md5, 16);
	rc = rpmdbGetIteratorCount(mi);
        mi = rpmdbFreeIterator(mi);
	if (rc) {
	    rc = 0;
	    continue;
	}

	/* --- Add cache tags to new cache header. */
	rc = headerAddOrAppendEntry(ip->h, RPMTAG_CACHECTIME,
		RPM_INT32_TYPE, &tid, 1);
	if (rc != 1) break;
	rc = headerAddOrAppendEntry(ip->h, RPMTAG_CACHEPKGPATH,
		RPM_STRING_ARRAY_TYPE, &ip->path, 1);
	if (rc != 1) break;
	rc = headerAddOrAppendEntry(ip->h, RPMTAG_CACHEPKGSIZE,
		RPM_INT32_TYPE, &ip->size, 1);
	if (rc != 1) break;
	rc = headerAddOrAppendEntry(ip->h, RPMTAG_CACHEPKGMTIME,
		RPM_INT32_TYPE, &ip->mtime, 1);
	if (rc != 1) break;

	/* --- Add new cache header to database. */
	rc = rpmdbAdd(rpmtsGetRdb(ts), tid, ip->h, NULL, NULL);
	if (rc) break;

    }
    return rc;
}

/**
 */
static int archOkay(/*@null@*/ const char * pkgArch)
        /*@*/
{
    if (pkgArch == NULL) return 0;
    return (rpmMachineScore(RPM_MACHTABLE_INSTARCH, pkgArch) ? 1 : 0);
}

/**
 */
static int osOkay(/*@null@*/ const char * pkgOs)
        /*@*/
{
    if (pkgOs == NULL) return 0;
    return (rpmMachineScore(RPM_MACHTABLE_INSTOS, pkgOs) ? 1 : 0);
}

static int ftsStashLatest(FTSENT * fts, rpmts ts)
{
    Header h = NULL;
    rpmds add = NULL;
    const char * arch;
    const char * os;
    struct stat sb, * st;
    int ec = -1;	/* assume not found */
    int i = 0;

    rpmMessage(RPMMESS_DEBUG, "============== %s\n", fts->fts_accpath);

    /* Read header from file. */
    {   FD_t fd = Fopen(fts->fts_accpath, "r");
	rpmRC rpmrc;
	int xx;

	if (fd == NULL || Ferror(fd)) {
	    if (fd) xx = Fclose(fd);
	    goto exit;
	}

	rpmrc = rpmReadPackageFile(ts, fd, fts->fts_path, &h);
	xx = Fclose(fd);
	if (rpmrc != RPMRC_OK || h == NULL)
	    goto exit;
    }

    if (!headerGetEntry(h, RPMTAG_ARCH, NULL, (void **) &arch, NULL)
     || !headerGetEntry(h, RPMTAG_OS, NULL, (void **) &os, NULL))
	goto exit;

    /* Make sure arch and os match this platform. */
    if (!archOkay(arch) || !osOkay(os)) {
	ec = 0;
	goto exit;
    }

    add = rpmdsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_EQUAL|RPMSENSE_LESS));

    if (items != NULL && nitems > 0) {
	Item needle = memset(alloca(sizeof(*needle)), 0, sizeof(*needle));
	Item * found, * fneedle = &needle;
	
	needle->this = add;

	found = bsearch(fneedle, items, nitems, sizeof(*found), cmpItem);

	/* Rewind to the first item with same name. */
	while (found > items && cmpItem(found-1, fneedle) == 0)
	    found--;

	/* Check that all saved items are newer than this item. */
	if (found != NULL)
	while (found < (items + nitems) && cmpItem(found, fneedle) == 0) {
	    ec = rpmdsCompare(needle->this, (*found)->this);
	    if (ec == 0) {
		found++;
		continue;
	    }
	    i = found - items;
	    break;
	}
    }

    /*
     * At this point, ec is
     *	-1	no item with the same name has been seen.
     *	0	item exists, but already saved item EVR is newer.
     *	1	item exists, but already saved item EVR is same/older.
     */
    if (ec == 0) {
	goto exit;
    } else if (ec == 1) {
	items[i] = freeItem(items[i]);
    } else {
	i = nitems++;
	items = xrealloc(items, nitems * sizeof(*items));
    }

    items[i] = newItem();
    items[i]->path = xstrdup(fts->fts_path);
    st = fts->fts_statp;
    if (st == NULL && Stat(fts->fts_accpath, &sb) == 0)
	st = &sb;

    if (st != NULL) {
	items[i]->size = st->st_size;
	items[i]->mtime = st->st_mtime;
    }
    st = NULL;
    items[i]->this = rpmdsThis(h, RPMTAG_PROVIDENAME, RPMSENSE_EQUAL);
    items[i]->h = headerLink(h);

    if (nitems > 1)
	qsort(items, nitems, sizeof(*items), cmpItem);

#if 0
    fprintf(stderr, "\t%*s [%d] %s\n",
		indent * (fts->fts_level < 0 ? 0 : fts->fts_level), "",
		i, fts->fts_name);
#endif

exit:
    h = headerFree(h);
    add = rpmdsFree(add);
    return ec;
}

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

static int ftsPrint(FTS * ftsp, FTSENT * fts, rpmts ts)
{
    struct ftsglob_s * bhg;
    const char ** patterns;
    const char * pattern;
    const char * s;
    int lvl;
    int xx;

    switch (fts->fts_info) {
    case FTS_D:		/* preorder directory */
	if (fts->fts_pathlen < bhpathlen)
	    break;

	/* Grab the level of the beehive top directory. */
	if (bhlvl < 0) {
	    if (fts->fts_pathlen == bhpathlen && !strcmp(fts->fts_path, bhpath))
		bhlvl = fts->fts_level;
	    else
		break;
	}
	lvl = fts->fts_level - bhlvl;

	if (lvl < 0)
	    break;

#if 0
	if (_debug)
	    fprintf(stderr, "FTS_%s\t%*s %s\n", ftsInfoStr(fts->fts_info),
		indent * (fts->fts_level < 0 ? 0 : fts->fts_level), "",
		fts->fts_name);
#endif

	/* Full path glob expression check. */
	bhg = bhglobs;

	if ((patterns = bhg->patterns) != NULL)
	while ((pattern = *patterns++) != NULL) {
	    if (*pattern == '/')
		xx = fnmatch(pattern, fts->fts_path, bhg->fnflags);
	    else
		xx = fnmatch(pattern, fts->fts_name, bhg->fnflags);
	    if (xx == 0)
		break;
	}

	/* Level specific glob expression check(s). */
	if (lvl == 0 || lvl >= nbhglobs)
	    break;
	bhg += lvl;

	if ((patterns = bhg->patterns) != NULL)
	while ((pattern = *patterns++) != NULL) {
	    if (*pattern == '/')
		xx = fnmatch(pattern, fts->fts_path, bhg->fnflags);
	    else
		xx = fnmatch(pattern, fts->fts_name, bhg->fnflags);
	    if (xx == 0)
		break;
	    else
		xx = Fts_set(ftsp, fts, FTS_SKIP);
	}

	break;
    case FTS_DP:	/* postorder directory */
#if 0
	if (_debug)
	    fprintf(stderr, "FTS_%s\t%*s %s\n", ftsInfoStr(fts->fts_info),
		indent * (fts->fts_level < 0 ? 0 : fts->fts_level), "",
		fts->fts_name);
#endif
	break;
    case FTS_F:		/* regular file */
#if 0
	if (_debug)
	    fprintf(stderr, "FTS_%s\t%*s %s\n", ftsInfoStr(fts->fts_info),
		indent * (fts->fts_level < 0 ? 0 : fts->fts_level), "",
		fts->fts_name);
#endif
	if (fts->fts_level >= 0) {
	    /* Ignore source packages. */
	    if (!strcmp(fts->fts_parent->fts_name, "SRPMS")) {
		xx = Fts_set(ftsp, fts->fts_parent, FTS_SKIP);
		break;
	    }
	}

	/* Ignore all but *.rpm files. */
	s = fts->fts_name + fts->fts_namelen + 1 - sizeof(".rpm");
	if (strcmp(s, ".rpm"))
	    break;

	xx = ftsStashLatest(fts, ts);

	break;
    case FTS_NS:	/* stat(2) failed */
    case FTS_DNR:	/* unreadable directory */
    case FTS_ERR:	/* error; errno is set */
	if (_debug)
	    fprintf(stderr, "FTS_%s\t%*s %s\n", ftsInfoStr(fts->fts_info),
		indent * (fts->fts_level < 0 ? 0 : fts->fts_level), "",
		fts->fts_name);
	break;
    case FTS_DC:	/* directory that causes cycles */
    case FTS_DEFAULT:	/* none of the above */
    case FTS_DOT:	/* dot or dot-dot */
    case FTS_INIT:	/* initialized only */
    case FTS_NSOK:	/* no stat(2) requested */
    case FTS_SL:	/* symbolic link */
    case FTS_SLNONE:	/* symbolic link without target */
    case FTS_W:		/* whiteout object */
    default:
	if (_debug)
	    fprintf(stderr, "FTS_%s\t%*s %s\n", ftsInfoStr(fts->fts_info),
		indent * (fts->fts_level < 0 ? 0 : fts->fts_level), "",
		fts->fts_name);
	break;
    }

    return 0;
}

/**
 * Initialize fts and glob structures.
 * @param ts		transaction set
 * @param argv		package names to match
 */
static void initGlobs(/*@unused@*/ rpmts ts, const char ** argv)
{
    char buf[BUFSIZ];
    int i;

    buf[0] = '\0';
    if (argv != NULL && * argv != NULL) {
	const char * arg;
	int single = (Glob_pattern_p(argv[0], 0) && argv[1] == NULL);
	char * t;

	t = buf;
	if (!single)
	    t = stpcpy(t, "@(");
	while ((arg = *argv++) != NULL) {
	    t = stpcpy(t, arg);
	    *t++ = '|';
	}
	t[-1] = (single ? '\0' : ')');
	*t = '\0';
    }

    bhpath = rpmExpand("%{_bhpath}", NULL);
    bhpathlen = strlen(bhpath);

    ftsSet = xcalloc(2, sizeof(*ftsSet));
    ftsSet[0] = rpmExpand("%{_bhpath}", NULL);

    nbhglobs = 5;
    bhglobs = xcalloc(nbhglobs, sizeof(*bhglobs));
    for (i = 0; i < nbhglobs; i++) {
	const char * pattern;
	const char * macro;

	switch (i) {
	case 0:
	    macro = "%{_bhpath}";
	    break;
	case 1:
	    macro = "%{_bhcoll}";
	    break;
	case 2:
	    macro = (buf[0] == '\0' ? "%{_bhN}" : buf);
	    break;
	case 3:
	    macro = "%{_bhVR}";
	    break;
	case 4:
	    macro = "%{_bhA}";
	    break;
	default:
	    macro = NULL;
	    break;
	}
	bhglobs[i].patterns = xcalloc(2, sizeof(*bhglobs[i].patterns));
	if (macro == NULL)
	    continue;
	pattern = rpmExpand(macro, NULL);
	if (pattern == NULL || *pattern == '\0') {
	    pattern = _free(pattern);
	    continue;
	}
	bhglobs[i].patterns[0] = pattern;
	bhglobs[i].fnflags = (FNM_PATHNAME | FNM_PERIOD | FNM_EXTMATCH);
	if (bhglobs[i].patterns[0] != NULL)
	    rpmMessage(RPMMESS_DEBUG, "\t%d \"%s\"\n",
		i, bhglobs[i].patterns[0]);
    }
}

static rpmVSFlags vsflags = 0;

static struct poptOption optionsTable[] = {
 { "nolegacy", '\0', POPT_BIT_SET,      &vsflags, RPMVSF_NEEDPAYLOAD,
	N_("don't verify header+payload signature"), NULL },

 { "nocache", '\0', POPT_ARG_VAL,   &noCache, -1,
	N_("don't update cache database, only print package paths"), NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliFtsPoptTable, 0,
        N_("File tree walk options:"),
        NULL },

 { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
	N_("Common options for all rpm modes and executables:"),
	NULL },

    POPT_AUTOALIAS
    POPT_AUTOHELP
    POPT_TABLEEND
};

int
main(int argc, char *const argv[])
{
    rpmts ts = NULL;
    poptContext optCon;
    const char * s;
    FTS * ftsp;
    FTSENT * fts;
    int ec = 1;
    rpmRC rpmrc;
    int xx;

    optCon = rpmcliInit(argc, argv, optionsTable);
    if (optCon == NULL)
        exit(EXIT_FAILURE);

    /* Configure the path to cache database, creating if necessary. */
    s = rpmExpand("%{?_cache_dbpath}", NULL);
    if (!(s && *s))
	rpmrc = RPMRC_FAIL;
    else
	rpmrc = rpmMkdirPath(s, "cache_dbpath");
    s = _free(s);
    if (rpmrc != RPMRC_OK) {
	fprintf(stderr, _("%s: %%{_cache_dbpath} macro is mis-configured.\n"),
		__progname);
        exit(EXIT_FAILURE);
    }

    ts = rpmtsCreate();

    if (rpmcliQueryFlags & VERIFY_DIGEST)
	vsflags |= _RPMVSF_NODIGESTS;
    if (rpmcliQueryFlags & VERIFY_SIGNATURE)
	vsflags |= _RPMVSF_NOSIGNATURES;
    if (rpmcliQueryFlags & VERIFY_HDRCHK)
	vsflags |= RPMVSF_NOHDRCHK;
    (void) rpmtsSetVSFlags(ts, vsflags);

    {   int_32 tid = (int_32) time(NULL);
	(void) rpmtsSetTid(ts, tid);
    }

    initGlobs(ts, poptGetArgs(optCon));
    if (ftsOpts == 0)
	ftsOpts = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);

    if (noCache)
	ftsOpts |= FTS_NOSTAT;
    else
	ftsOpts &= ~FTS_NOSTAT;

    /* Walk file tree, filter paths, save matched items. */
    ftsp = Fts_open(ftsSet, ftsOpts, NULL);
    while((fts = Fts_read(ftsp)) != NULL) {
	xx = ftsPrint(ftsp, fts, ts);
    }
    xx = Fts_close(ftsp);

    if (noCache)
	ec = ftsCachePrint(ts, stdout);
    else
	ec = ftsCacheUpdate(ts);
    if (ec) {
	fprintf(stderr, _("%s: cache operation failed: ec %d.\n"),
		__progname, ec);
    }

    freeItems();

    ts = rpmtsFree(ts);

    optCon = rpmcliFini(optCon);

    return ec;
}
