#include "system.h"
#include <fnmatch.h>
#include <fts.h>

#include <rpmlib.h>
#include <rpmpgp.h>

#include "depends.h"

#include "debug.h"

static int _debug = 1;

#define	BHPATH	"/mnt/redhat/beehive/comps/dist"
#define	BHCOLL	"7.2"

#define	BHN	"@(basesystem|bash|filesystem|glibc-common|glibc|ldconfig|libtermcap|mktemp|setup|termcap)"

#define	BHVR	"*"
#define	BHA	"@(i[3456]86|noarch)"

const char * bhpath = BHPATH;
int bhpathlen = sizeof(BHPATH)-1;

int bhlvl = -1;

static char * const ftsSet[] = {
    BHPATH, NULL,
};

struct ftsglob_s {
    const char ** patterns;
    int fnflags;
};

const char * lvl0[] = {
    BHPATH, NULL
};

const char * lvl1[] = {
    BHCOLL, NULL
};

const char * lvl2[] = {
    BHN, NULL
};

const char * lvl3[] = {
    BHVR, NULL
};

const char * lvl4[] = {
    BHA, NULL
};

static struct ftsglob_s bhglobs[] = {
    { lvl0,	(FNM_PATHNAME | FNM_PERIOD | FNM_EXTMATCH) },
    { lvl1,	(FNM_PATHNAME | FNM_PERIOD | FNM_EXTMATCH) },
    { lvl2,	(FNM_PATHNAME | FNM_PERIOD | FNM_EXTMATCH) },
    { lvl3,	(FNM_PATHNAME | FNM_PERIOD | FNM_EXTMATCH) },
    { lvl4,	(FNM_PATHNAME | FNM_PERIOD | FNM_EXTMATCH) },
};
static int nbhglobs = sizeof(bhglobs)/sizeof(bhglobs[0]);

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
    const char * n, * v, * r;
    const char * s;
    FD_t fd = NULL;
    Header h = NULL;
    rpmRC rc;
    int indent = 2;
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
		xx = fts_set(ftsp, fts, FTS_SKIP);
	}

	break;
    case FTS_DP:	/* postorder directory */
#if 0
	fprintf(stderr, "FTS_%s\t%*s %s\n", ftsInfoStr(fts->fts_info),
		indent * (fts->fts_level < 0 ? 0 : fts->fts_level), "",
		fts->fts_name);
#endif
	break;
    case FTS_F:		/* regular file */
	if (fts->fts_level >= 0) {
	    if (strcmp(fts->fts_parent->fts_name, "SRPMS")) {
		xx = fts_set(ftsp, fts->fts_parent, FTS_SKIP);
		break;
	    }
	}
	s = fts->fts_name + fts->fts_namelen + 1 - sizeof(".rpm");
	if (strcmp(s, ".rpm"))
	    break;
	rpmMessage(RPMMESS_DEBUG, "============== %s\n", fts->fts_accpath);
	fd = Fopen(fts->fts_accpath, "r");
	if (fd == NULL || Ferror(fd))
	    break;
	rc = rpmReadPackageFile(ts, fd, fts->fts_path, &h);
	xx = Fclose(fd);
	fd = NULL;
	if (rc != RPMRC_OK)
	    break;
	xx = headerNVR(h, &n, &v, &r);
	fprintf(stderr, "\t%*s %s-%s-%s\n",
		indent * (fts->fts_level < 0 ? 0 : fts->fts_level), "",
		n, v, r);
#ifdef NOTYET
	xx = rpmtransAddPackage(ts, h, NULL, fts->fts_path, 1, NULL);
#endif

	break;
    case FTS_NS:	/* stat(2) failed */
    case FTS_DNR:	/* unreadable directory */
    case FTS_ERR:	/* error; errno is set */
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
	fprintf(stderr, "FTS_%s\t%*s %s\n", ftsInfoStr(fts->fts_info),
		indent * (fts->fts_level < 0 ? 0 : fts->fts_level), "",
		fts->fts_name);
	break;
    }

    h = headerFree(h, "ftsPrint exit");
    if (fd)
	xx = Fclose(fd);
    return 0;
}

static int depth;
static int level;
static int globx;

static void go (const char * sb, const char * se)
{
    const char * s = sb;
    int blvl = 0;
    int plvl = 0;
    int c;

    globx = -1;
    while ( s < se && (c = *s) != '\0' ) {
	s++;
	switch (c) {
	case '+':
	case '@':
	case '!':
	    continue;
	case '?':
	case '*':
	    if (*s == '(')
		continue;
	    if (globx < 0)
		globx = (s - sb - 1);
	    continue;
	case '[':
	    blvl++;
	    continue;
	case ']':
	    if (!blvl)
		continue;
	    --blvl;
	    if (!blvl && globx < 0)
		globx = (s - sb - 1);
	    continue;
	case '(':
	    plvl++;
	    continue;
	case '|':
	    continue;
	case ')':
	    if (!plvl)
		continue;
	    --plvl;
	    if (!plvl && globx < 0)
		globx = (s - sb - 1);
	    continue;
	case '\\':
	    if (*s)
		s++;
	    continue;
	case '/':
	    depth++;
	    continue;
	default:
	    continue;
	}
    }
}

static struct poptOption optionsTable[] = {
 { "verbose", 'v', 0, 0, 'v',				NULL, NULL },
 { "debug", 'd', POPT_ARG_VAL,	&_debug, -1,		NULL, NULL },
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
    int ftsOpts = (FTS_COMFOLLOW | FTS_LOGICAL | FTS_NOSTAT);
    FTSENT * fts;
    char buf[BUFSIZ];
    const char ** args;
    const char * arg;
    char * t;
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

    args = poptGetArgs(optCon);
    
    t = buf;
    *t = '\0';
    t = stpcpy(t, BHPATH);
    *t++ = '/';
    t = stpcpy(t, BHCOLL);
    *t++ = '/';

    if (args == NULL)
	t = stpcpy(t, BHN);
    else {
	t = stpcpy(t, "@(");
	while ((arg = *args++) != NULL) {
	    t = stpcpy(t, arg);
	    *t++ = '|';
	}
	t[-1] = ')';
    }

    *t++ = '/';
    t = stpcpy(t, BHVR);
    *t++ = '/';
    t = stpcpy(t, BHA);

fprintf(stderr, "*** \"%s\"\n", buf);

    level = 0;
    depth = (buf[0] == '/') ? -1 : 0;
    globx = -1;
    go(buf, buf+strlen(buf));

fprintf(stderr, "*** \"%s/%s/%s/%s/%s\"\n", *lvl0, *lvl1, *lvl2, *lvl3, *lvl4);

#if 0
    lvl0[0] = buf;
#endif

if (!_debug) {
    ftsp = fts_open(ftsSet, ftsOpts, NULL);
    while((fts = fts_read(ftsp)) != NULL)
	xx = ftsPrint(ftsp, fts, ts);
    xx = fts_close(ftsp);
}

#ifdef	NOTYET
if (!_debug) {
    {	rpmDependencyConflict conflicts = NULL;
	int numConflicts = 0;

	(void) rpmdepCheck(ts, &conflicts, &numConflicts);

	/*@-branchstate@*/
	if (conflicts) {
	    rpmMessage(RPMMESS_ERROR, _("failed dependencies:\n"));
	    printDepProblems(stderr, conflicts, numConflicts);
	    conflicts = rpmdepFreeConflicts(conflicts, numConflicts);
	}
	/*@=branchstate@*/
    }

    (void) rpmdepOrder(ts);
}
#endif

exit:
    ts = rpmtransFree(ts);

    return ec;
}
