#include "system.h"
#include <fnmatch.h>
#include <fts.h>

#include <rpmlib.h>
#include <rpmmacro.h>
#include <rpmurl.h>
#include <rpmpgp.h>

#include "rpmds.h"
#include "rpmts.h"
#include "misc.h"	/* XXX myGlobPatternP */

#include "debug.h"

static int _debug = 0;
/*@unchecked@*/
extern int _ftp_debug;
/*@unchecked@*/
extern int _rpmio_debug;

static int verify_legacy = 0;

static char ** ftsSet;
static int ftsOpts = 0;

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
    rpmDepSet this;
    Header h;
} * Item;

static Item * items = NULL;
static int nitems = 0;

static inline Item freeItem(Item item) {
    if (item != NULL) {
	item->path = _free(item->path);
	item->this = dsFree(item->this);
	item->h = headerFree(item->h, NULL);
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
    int rc = strcmp(dsiGetN(aitem->this), dsiGetN(bitem->this));
    return rc;
}

static void freeItems(void) {
    int i;
    for (i = 0; i < nitems; i++)
	items[i] = freeItem(items[i]);
    items = _free(items);
    nitems = 0;
}

static void ftsPrintPaths(FILE * fp) {
    int i;
    for (i = 0; i < nitems; i++)
	fprintf(fp, "%s\n", items[i]->path);
}

static int ftsStashLatest(FTSENT * fts, rpmTransactionSet ts)
{
    Header h = NULL;
    rpmDepSet add;
    int ec = -1;	/* assume not found */
    int i = 0;

    rpmMessage(RPMMESS_DEBUG, "============== %s\n", fts->fts_accpath);

    /* Read header from file. */
    {   FD_t fd = Fopen(fts->fts_accpath, "r");
	rpmRC rc;
	int xx;

	if (fd == NULL || Ferror(fd)) {
	    if (fd) xx = Fclose(fd);
	    return ec;	/* XXX -1 */
	}

	rc = rpmReadPackageFile(ts, fd, fts->fts_path, &h);
	xx = Fclose(fd);
	if (rc != RPMRC_OK)
	    return ec;	/* XXX -1 */
    }

    add = dsThis(h, RPMTAG_REQUIRENAME, (RPMSENSE_EQUAL|RPMSENSE_LESS));

    if (items != NULL && nitems > 0) {
	Item needle = memset(alloca(sizeof(*needle)), 0, sizeof(*needle));
	Item * found, * fneedle = &needle;
	
	needle->this = add;

	found = bsearch(fneedle, items, nitems,
                       sizeof(*found), cmpItem);

	/* Rewind to the first item with same name. */
	while (found > items && cmpItem(found-1, fneedle) == 0)
	    found--;

	/* Check that all saved items are newer than this item. */
	if (found != NULL)
	while (found < (items + nitems) && cmpItem(found, fneedle) == 0) {
	    ec = dsCompare(needle->this, (*found)->this);
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
    items[i]->this = dsThis(h, RPMTAG_PROVIDENAME, RPMSENSE_EQUAL);
    items[i]->h = headerLink(h, NULL);

    if (nitems > 1)
	qsort(items, nitems, sizeof(*items), cmpItem);

#if 0
    fprintf(stderr, "\t%*s [%d] %s\n",
		indent * (fts->fts_level < 0 ? 0 : fts->fts_level), "",
		i, fts->fts_name);
#endif

exit:
    h = headerFree(h, NULL);
    add = dsFree(add);
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

static int ftsPrint(FTS * ftsp, FTSENT * fts, rpmTransactionSet ts)
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
	    if (!strcmp(fts->fts_parent->fts_name, "SRPMS")) {
		xx = Fts_set(ftsp, fts->fts_parent, FTS_SKIP);
		break;
	    }
	}
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

static void initGlobs(const char ** argv)
{
    char buf[BUFSIZ];
    int i;

    buf[0] = '\0';
    if (argv != NULL && * argv != NULL) {
	const char * arg;
	int single = (myGlobPatternP(argv[0]) && argv[1] == NULL);
	char * t;

	t = buf;
	if (!single)
	    t = stpcpy(t, "@(");
	while ((arg = *argv++) != NULL) {
	    t = stpcpy(t, arg);
	    *t++ = '|';
	}
	t[-1] = (single ? '\0' : ')');
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

static struct poptOption optionsTable[] = {
 { "debug", 'd', POPT_ARG_VAL,		&_debug, -1,		NULL, NULL },

 { "verifylegacy", '\0', POPT_ARG_VAL,	&verify_legacy, -1,	NULL, NULL },

 { "comfollow", '\0', POPT_BIT_SET,	&ftsOpts, FTS_COMFOLLOW,
	N_("follow command line symlinks"), NULL },
 { "logical", '\0', POPT_BIT_SET,	&ftsOpts, FTS_LOGICAL,
	N_("logical walk"), NULL },
 { "nochdir", '\0', POPT_BIT_SET,	&ftsOpts, FTS_NOCHDIR,
	N_("don't change directories"), NULL },
 { "nostat", '\0', POPT_BIT_SET,	&ftsOpts, FTS_NOSTAT,
	N_("don't get stat info"), NULL },
 { "physical", '\0', POPT_BIT_SET,	&ftsOpts, FTS_PHYSICAL,
	N_("physical walk"), NULL },
 { "seedot", '\0', POPT_BIT_SET,	&ftsOpts, FTS_SEEDOT,
	N_("return dot and dot-dot"), NULL },
 { "xdev", '\0', POPT_BIT_SET,		&ftsOpts, FTS_XDEV,
	N_("don't cross devices"), NULL },
 { "whiteout", '\0', POPT_BIT_SET,	&ftsOpts, FTS_WHITEOUT,
	N_("return whiteout information"), NULL },

 { "ftpdebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_ftp_debug, -1,
	N_("debug protocol data stream"), NULL},
 { "rpmiodebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_rpmio_debug, -1,
	N_("debug rpmio I/O"), NULL},
 { "urldebug", '\0', POPT_ARG_VAL|POPT_ARGFLAG_DOC_HIDDEN, &_url_debug, -1,
	N_("debug URL cache handling"), NULL},
 { "verbose", 'v', 0, 0, 'v',				NULL, NULL },
  POPT_AUTOHELP
  POPT_TABLEEND
};

int
main(int argc, const char *argv[])
{
    poptContext optCon = poptGetContext(argv[0], argc, argv, optionsTable, 0);
    const char * rootDir = "";
    rpmdb db = NULL;
    rpmTransactionSet ts = NULL;
    FTS * ftsp;
    FTSENT * fts;
    const char ** av;
    int rc;
    int ec = 1;
    int xx;

    while ((rc = poptGetNextOpt(optCon)) > 0) {
	switch (rc) {
	case 'v':
	    rpmIncreaseVerbosity();
	    /*@switchbreak@*/ break;
	default:
            /*@switchbreak@*/ break;
	}
    }

    rpmReadConfigFiles(NULL, NULL);

    if (_debug) {
	rpmIncreaseVerbosity();
	rpmIncreaseVerbosity();
    }

    ts = rpmtransCreateSet(db, rootDir);
    (void) rpmtsOpenDB(ts, O_RDONLY);
    if (verify_legacy) {
	ts->dig = pgpNewDig();
	ts->verify_legacy = 1;
    }

    av = poptGetArgs(optCon);
    
    initGlobs(av);

    if (ftsOpts == 0)
	ftsOpts = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);

    ftsp = Fts_open(ftsSet, ftsOpts, NULL);
    while((fts = Fts_read(ftsp)) != NULL)
	xx = ftsPrint(ftsp, fts, ts);
    xx = Fts_close(ftsp);

    ftsPrintPaths(stdout);
    freeItems();

    ts = rpmtransFree(ts);

    return ec;
}
