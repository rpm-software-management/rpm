#include "system.h"
#include <fnmatch.h>
#include <fts.h>

#include <rpmlib.h>
#include <rpmpgp.h>

#include "depends.h"

#include "debug.h"

static int _debug = 0;

#define	BHPATH	"/mnt/redhat/beehive/comps/dist"
#define	BHCOLL	"7.2"

#if 0
#define	BHN	"{basesystem,bash,filesystem,glibc-common,glibc,ldconfig,libtermcap,mktemp,setup,termcap}"
#else
#define	BHN	"setup"
#endif

#define	BHVR	"*"
#define	BHA	"*"

const char * bhpath = BHPATH;
int bhpathlen = sizeof(BHPATH)-1;
const char * bhcoll = BHCOLL;
const char * bhn = BHN;
const char * bhvr = BHVR;
const char * bha = BHA;

int bhlvl = -1;

struct ftsglob_s {
    const char ** patterns;
    int fnflags;
};

const char * lvl0[] = {
    NULL, NULL
};

const char * lvl1[] = {
    NULL, NULL
};

const char * lvl2[] = {
    NULL, NULL
};

const char * lvl3[] = {
    NULL, NULL
};

const char * lvl4[] = {
    NULL, NULL
};

const char * lvl5[] = {
    NULL, NULL
};

const char * lvl6[] = {
    NULL, NULL
};

const char * lvl7[] = {
    NULL, NULL
};

static struct ftsglob_s bhglobs[] = {
#if	0
    { BHPATH "/" BHCOLL "/" BHN "/" BHVR "/" BHA "/*",	0 },
    { BHCOLL,	FNM_PERIOD },
    { BHN,	FNM_PERIOD },
    { BHVR,	FNM_PERIOD },
    { BHA,	FNM_PERIOD },
#else
    { lvl0, 	FNM_PERIOD },
    { lvl1, 	FNM_PERIOD },
    { lvl2, 	FNM_PERIOD },
    { lvl3, 	FNM_PERIOD },
    { lvl4, 	FNM_PERIOD },
    { lvl5, 	FNM_PERIOD },
    { lvl6, 	FNM_PERIOD },
    { lvl7, 	FNM_PERIOD },
#endif
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
#ifdef DYING
	if (lvl <= 0) {
	    break;
	} else if (lvl == 1) {
	    /* Skip collections we're not interested in. */
	    if (!strcmp(fts->fts_name, bhcoll))
		break;
	    xx = fts_set(ftsp, fts, FTS_SKIP);
	    break;
	} else if (lvl == 2) {
	    /* Skip names we're not interested in. */
	    if (!strcmp(fts->fts_name, bhn))
		break;
	    xx = fts_set(ftsp, fts, FTS_SKIP);
	    break;
	} else if (lvl == 3) {
	    /* Skip version-release we're not interested in. */
	    if (!strcmp(fts->fts_name, bhvr))
		break;
	    xx = fts_set(ftsp, fts, FTS_SKIP);
	    break;
	} else if (lvl == 4) {
	    /* Skip arch we're not interested in. */
	    if (!strcmp(fts->fts_name, bha))
		break;
	    xx = fts_set(ftsp, fts, FTS_SKIP);
	    break;
	}
#else
	if (lvl < 0 || lvl >= nbhglobs)
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
	if (lvl == 0)
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
#endif
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
	xx = rpmtransAddPackage(ts, h, NULL, fts->fts_path, 1, NULL);
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

    h = headerFree(h);
    if (fd)
	xx = Fclose(fd);
    return 0;
}

static char * const ftsSet[] = {
    BHPATH, NULL,
};


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

    ts = rpmtransCreateSet(db, rootDir);
    (void) rpmtsOpenDB(ts, O_RDONLY);

    args = poptGetArgs(optCon);
    if (args)
    while ((arg = *args++) != NULL) {
    
	t = buf;
	*t = '\0';
	t = stpcpy(t, BHPATH);
	*t++ = '/';
	t = stpcpy(t, BHCOLL);
	*t++ = '/';
	t = stpcpy(t, arg);
	*t++ = '/';
	t = stpcpy(t, "*");
	*t++ = '/';
	t = stpcpy(t, "i386");
	*t++ = '/';
	t = stpcpy(t, "*.rpm");

fprintf(stderr, "*** \"%s\"\n", buf);
	lvl0[0] = buf;
	lvl1[0] = BHCOLL;
	lvl2[0] = arg;

if (!_debug) {
	ftsp = fts_open(ftsSet, ftsOpts, NULL);
	while((fts = fts_read(ftsp)) != NULL)
	    xx = ftsPrint(ftsp, fts, ts);
	xx = fts_close(ftsp);
}

    }

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

exit:
    ts = rpmtransFree(ts);

    return ec;
}
