#include "system.h"
const char *__progname;

#include <err.h>	/* XXX !HAVE_ERR_H: get from misc */

#include <rpm/rpmbuild.h>
#include <rpm/header.h>

#include "lib/rpmlead.h"
#include "build/buildio.h"

#include "debug.h"

typedef enum injmode_e { INJ_UNKNOWN, INJ_ADD, INJ_DELETE, INJ_MODIFY } injmode_t;

injmode_t injmode = INJ_UNKNOWN;

typedef struct cmd_s {
    injmode_t	injmode;
    char *	tag;
    int32_t	tagval;
    int		done;
    int		oldcnt;
    int		nvals;
    char **	vals;
} cmd_t;

#define	MAXCMDS	40
cmd_t *cmds[MAXCMDS];
int ncmds = 0;

static const char * pr_injmode(injmode_t injmode)
{
    switch(injmode) {
    case INJ_ADD:	return("add");
    case INJ_DELETE:	return("delete");
    case INJ_MODIFY:	return("modify");
    case INJ_UNKNOWN:	return("unknown");
    default:		return("???");
    }
}

enum cvtaction {CA_OLD, CA_NEW, CA_OMIT, CA_ERR};

static enum cvtaction convertAMD(enum cvtaction ca, rpmTagType type,
	void ** nvalsp, rpm_count_t *ncountp, cmd_t *newc)
{
    int i;

    if (newc == NULL)
	return ca;
    if (!(nvalsp && ncountp))
	return CA_ERR;

    *nvalsp = NULL;
    *ncountp = 0;

    switch (ca) {
    case CA_OLD:
    case CA_OMIT:
    case CA_ERR:
    default:
	break;
    case CA_NEW:
	switch (type) {
	case RPM_INT32_TYPE:
	{   int32_t *intp = xmalloc(newc->nvals * sizeof(*intp));
	    for (i = 0; i < newc->nvals; i++) {
		long ival;
		char *end;
		end = NULL;
		ival = strtol(newc->vals[i], &end, 0);
		if (end && *end)
		    break;
		if ((((unsigned long)ival) >> (8*sizeof(*intp))) != 0)
		    break;
		intp[i] = ival;
	    }
	    if (i < newc->nvals) {
		ca = CA_ERR;
		free(intp);
		break;
	    }
	    *nvalsp = intp;
	    *ncountp = newc->nvals;
	}   break;
	case RPM_BIN_TYPE:	/* icons & signatures */
	case RPM_STRING_TYPE:
	    if (newc->nvals != 1) {
		newc->done = 0;
		ca = CA_ERR;
		break;
	    }
	    *nvalsp = xstrdup(newc->vals[0]);
	    *ncountp = newc->nvals;
	    break;
	case RPM_STRING_ARRAY_TYPE:
	{   const char **av = xmalloc((newc->nvals+1) * sizeof(char *));
	    for (i = 0; i < newc->nvals; i++) {
		av[i] = newc->vals[i];
	    }
	    av[newc->nvals] = NULL;
	    *nvalsp = av;
	    *ncountp = newc->nvals;
	}   break;
	case RPM_NULL_TYPE:
	case RPM_CHAR_TYPE:
	case RPM_INT8_TYPE:	/* arch & os */
	case RPM_INT16_TYPE:	/* file modes & rdevs */
	case RPM_I18NSTRING_TYPE:
	default:	/* this conversion cannot be performed (yet) */
	    newc->done = 0;
	    ca = CA_ERR;
	    break;
	}
	break;
    }

    return ca;
}

static enum cvtaction convertExistingAMD(rpmTag tag, rpmTagType type,
	rpm_data_t valsp, rpm_count_t *countp, void ** nvalsp, rpm_count_t *ncountp,
	cmd_t *cmds[], int ncmds)
{
    cmd_t *newc = NULL;
    enum cvtaction ca = CA_OLD;
    int i;

    if (!((tag >= RPMTAG_NAME && tag < RPMTAG_FIRSTFREE_TAG)
        || tag >= RPMTAG_EXTERNAL_TAG))
	return ca;

    for (i = 0; i < ncmds; i++) {
	cmd_t *c;
	c = cmds[i];

	if (tag != c->tagval)
	    continue;
	if (c->done)
	    continue;

	switch (c->injmode) {
	case INJ_ADD:
	    if (ca != CA_OMIT) {/* old tag was deleted, now adding again */
		c->done = -1;
		continue;
	    }
	    ca = CA_NEW;
	    newc = c;
	    c->done = 1;
	    break;
	case INJ_MODIFY:	/* XXX for now, this is delete, then add */
	    if (ca == CA_OMIT) {/* old tag was deleted, can't modify */
		c->done = -1;
		continue;
	    }
	    ca = CA_NEW;
	    newc = c;
	    c->done = 1;
	    break;
	case INJ_DELETE:
	    if (ca == CA_OMIT)	{/* old tag was deleted, now deleting again */
		c->done = -1;
		continue;
	    }
	    ca = CA_OMIT;
	    newc = c;
	    c->done = 1;
	    break;
	case INJ_UNKNOWN:
	default:
	    c->done = -1;
	    break;
	}
    }

    if (newc) {
	ca = convertAMD(ca, type, nvalsp, ncountp, newc);
	switch (ca) {
	case CA_OMIT:
	case CA_NEW:
	    newc->oldcnt = *countp;
	    break;
	case CA_OLD:
	case CA_ERR:
	    break;
	}
    }
    return ca;
}

static
Header headerCopyWithConvert(Header h, cmd_t *cmds[], int ncmds)
{
    HeaderIterator headerIter;
    Header res = headerNew();
    struct rpmtd_s td;
   
    headerIter = headerInitIterator(h);

    while (headerNext(headerIter, &td)) {
	enum cvtaction ca;
	struct rpmtd_s ntd = { .type = td.type, .tag = td.tag, 
			       .count = 0, .data = NULL
	};

	ca = convertExistingAMD(td.tag, td.type, &td.data, &td.count, 
				&ntd.data, &ntd.count, cmds, ncmds);
	switch (ca) {
	case CA_ERR:
	case CA_OLD:		/* copy old tag and values to header */	
	default:
	    /* Don't copy the old changelog, we'll do that later. */
	    switch (td.tag) {
	    case RPMTAG_CHANGELOGTIME:
	    case RPMTAG_CHANGELOGNAME:
	    case RPMTAG_CHANGELOGTEXT:
		break;
	    default:
		headerPut(res, &td, HEADERPUT_DEFAULT);
		break;
	    }
	    break;
	case CA_NEW:		/* copy new tag and values to header */	
	    headerPut(res, &ntd, HEADERPUT_DEFAULT);
	    break;
	case CA_OMIT:		/* delete old tag and values from header */
	    break;
	}

	rpmtdFreeData(&td);
	if (ntd.data)
	    free(ntd.data);
    }

    headerFreeIterator(headerIter);

    return res;
}

static char * genChangelog(cmd_t *cmds[], int ncmds)
{
#define	MYBUFSIZ (2*BUFSIZ)
    char *b, *buf = xmalloc(MYBUFSIZ);
    int i;

    b = buf;
    for (i = 0; i < ncmds; i++) {
	cmd_t *c;

	if ((c = cmds[i]) == NULL)
	    continue;

	b += sprintf(b, "- %s tag %s(%d)",
		pr_injmode(c->injmode), c->tag, c->tagval);

	if (c->oldcnt || c->nvals) {
	    *b++ = '\t';
	    *b++ = '(';
	    if (c->oldcnt)
		b += sprintf(b, "oldcnt %d", c->oldcnt);
	    if (c->oldcnt && c->nvals) {
		*b++ = ',';
		*b++ = ' ';
	    }
	    if (c->nvals)
		b += sprintf(b, "nvals %d", c->nvals);
	    *b++ = ')';
	}
	*b++ = '\n';
    }
    *b = '\0';

    return buf;
}

static int
headerInject(Header *hdrp, cmd_t *cmds[], int ncmds)
{
    Header h;
    int ec = 0;
    int i;

    if (!(hdrp && cmds && ncmds > 0))
	return -1;

    h = headerCopyWithConvert(*hdrp, cmds, ncmds);
    for (i = 0; i < ncmds; i++) {
	cmd_t *c;
	int rc;

	if ((c = cmds[i]) == NULL)
	    continue;

	rc = headerIsEntry(h, c->tagval);
	if (!rc && !c->done && c->injmode != INJ_DELETE) {
	    struct rpmtd_s td;
	    enum cvtaction ca;

	    td.type = (c->nvals > 0) ? RPM_STRING_ARRAY_TYPE : RPM_STRING_TYPE;
	    td.tag = c->tagval;
	    ca = convertAMD(CA_NEW, td.type, &td.data, &td.count, c);
	    if (ca == CA_NEW)
		headerPut(h, &td, HEADERPUT_DEFAULT);
	    rc = headerIsEntry(h, c->tagval);
	}

	switch(c->injmode) {
	case INJ_ADD:
	    if (!(rc && c->done > 0)) {
		warnx("failed to add tag %s", rpmTagGetName(c->tagval));
		ec = 1;
	    }
	    break;
	case INJ_DELETE:
	    if (!(!rc && c->done > 0)) {
		warnx("failed to delete tag %s", rpmTagGetName(c->tagval));
		ec = 1;
	    }
	    break;
	case INJ_MODIFY:
	    if (!(rc && c->done > 0)) {
		warnx("failed to modify tag %s", rpmTagGetName(c->tagval));
		ec = 1;
	    }
	    break;
	case INJ_UNKNOWN:
	default:
	    ec = 1;
	    break;
	}

	/* XXX possibly need strict mode to exit immediately here */
    }

    if (ec == 0 && *hdrp) {
	static char name[512] = "";
	static const char *text = NULL;
	static rpmTag cltags[] = {
	    RPMTAG_CHANGELOGTIME,
	    RPMTAG_CHANGELOGNAME,
	    RPMTAG_CHANGELOGTEXT,
	    0
	};

	if (name[0] == '\0')
	    sprintf(name, "rpminject <%s@%s>", getUname(getuid()), buildHost());
	if (text == NULL)
	    text = genChangelog(cmds, ncmds);
	
	addChangelogEntry(h, *getBuildTime(), name, text);
	headerCopyTags(*hdrp, h, cltags);
	headerSort(h);
	*hdrp = headerFree(*hdrp);
	*hdrp = h;
    } else {
	h = headerFree(h);
    }

    return ec;
}

/* ========================================================================= */

static int
rewriteRPM(const char *fni, const char *fno, cmd_t *cmds[], int ncmds)
{
    Header sigs;
    rpmSpec spec;
    struct cpioSourceArchive_s csabuf, *csa = &csabuf;
    int rc;

    csa->cpioArchiveSize = 0;
    csa->cpioFdIn = fdNew("init (rewriteRPM)");
    csa->cpioList = NULL;

    /* Read rpm and (partially) recreate spec/pkg control structures */
    if ((rc = readRPM(fni, &spec, &sigs, csa)) != 0)
	return rc;

    /* Inject new strings into header tags */
    if ((rc = headerInject(&spec->packages->header, cmds, ncmds)) != 0)
	goto exit;

    /* Rewrite the rpm */
    if (headerIsSource(spec->packages->header)) {
	rc = writeRPM(&spec->packages->header, NULL, fno, 
		csa, spec->passPhrase, &(spec->cookie));
    } else {
	rc = writeRPM(&spec->packages->header, NULL, fno,
		csa, spec->passPhrase, NULL);
    }

exit:
    Fclose(csa->cpioFdIn);
    return rc;

}

/* ========================================================================= */

static int
do_inject(cmd_t *cmds[], int ncmds, const char *argv[])
{
    const char *arg;
    int ec = 0;

    if (argv == NULL || *argv == NULL) {
	/* XXX generate lead/header to stdout */
	return 0;
    }

    while ((arg = *argv++) != NULL) {
	char *fni = xmalloc(strlen(arg) + sizeof("-SAVE"));
	const char *fno = arg;

	strcpy(fni, arg);
	strcat(fni, "-SAVE");
	unlink(fni);
	if (link(fno, fni)) {
	    warn("can't link temp input file %s", fni);
	    ec++;
	    continue;
	}
	if (rewriteRPM(fni, fno, cmds, ncmds)) {
	    unlink(fno);
	    if (rename(fni, fno))
		warn("can't rename %s to %s", fni, fno);
	    ec++;
	}
	if (fni) free(fni);
    }

    return ec;
}

static struct poptOption optionsTable[] = {
 { "add",	'a', 0, 0, 'a',			NULL, NULL },
 { "del",	'd', 0, 0, 'd',			NULL, NULL },
 { "injtags",	'i', 0, 0, 'i',			NULL, NULL },
 { "modify",	'm', 0, 0, 'm',			NULL, NULL },
 { "tag",	't', POPT_ARG_STRING, 0, 't',	NULL, NULL },
 { "value",	'v', POPT_ARG_STRING, 0, 'v',	NULL, NULL },
 { NULL,	0, 0, 0, 0,			NULL, NULL }
};

int
main(int argc, char *argv[])
{
    poptContext optCon;
    const char * optArg;
    cmd_t *c = NULL;
    int arg;
    int ec = 0;
    injmode_t lastmode = INJ_UNKNOWN;

#if HAVE_MCHECK_H && HAVE_MTRACE
    mtrace();  /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif

    setprogname(argv[0]);	/* Retrofit glibc __progname */
#if defined(ENABLE_NLS)
    (void)setlocale(LC_ALL, "" );

    (void)bindtextdomain(PACKAGE, LOCALEDIR);
    (void)textdomain(PACKAGE);
#endif

    optCon = poptGetContext("rpminject", argc, (const char **) argv, optionsTable, 0);
#if RPM_USES_POPTREADDEFAULTCONFIG
    poptReadDefaultConfig(optCon, 1);
#endif

    while ((arg = poptGetNextOpt(optCon)) > 0) {
	optArg = poptGetOptArg(optCon);
	switch (arg) {
	case 'a':
	    injmode = INJ_ADD;
	    break;
	case 'd':
	    injmode = INJ_DELETE;
	    break;
	case 'm':
	    injmode = INJ_MODIFY;
	    break;
	case 't':
	    if (ncmds == 0 || c == NULL)
		errx(EXIT_FAILURE, "missing inject mode before \"--tag %s\"", optArg);
	    if (c->tag) {
		if (c->injmode != INJ_DELETE &&
		  (c->nvals <= 0 || c->vals == NULL))
		    errx(EXIT_FAILURE, "add/modify inject mode with \"--tag %s\" needs a value", c->tag);
		cmds[ncmds] = c = xcalloc(1, sizeof(cmd_t));
		cmds[ncmds]->injmode = cmds[ncmds-1]->injmode;
		ncmds++;
	    }
	    c->tagval = rpmTagGetValue(optArg);
	    if (c->tagval == RPMTAG_NOT_FOUND)	
		errx(EXIT_FAILURE, "unknown rpm tag \"--tag %s\"", optArg);
	    c->tag = xstrdup(optArg);
	    break;
	case 'v':
	    if (ncmds == 0 || c == NULL)
		errx(EXIT_FAILURE, "missing inject mode before \"--value %s\"", optArg);
	    if (c->tag == NULL)
		errx(EXIT_FAILURE, "missing tag name before \"--value %s\"", optArg);
	    if (c->nvals == 0 || c->vals == NULL) {
		c->vals = xcalloc(2, sizeof(char *));
	    } else {
		c->vals = xrealloc(c->vals,
				(c->nvals+2)*sizeof(char *));
	    }
	    c->vals[c->nvals++] = xstrdup(optArg);
	    c->vals[c->nvals] = NULL;
	    break;
	case 'i':
	    rpmDisplayQueryTags(stdout);
	    exit(EXIT_SUCCESS);
	    break;
	default:
	    errx(EXIT_FAILURE, "unknown popt return (%d)", arg);
	    break;
	}

	if (injmode != lastmode) {
	    cmds[ncmds] = c = xcalloc(1, sizeof(cmd_t));
	    cmds[ncmds]->injmode = lastmode = injmode;
	    ncmds++;
	}
    }

    /* XXX I don't want to read rpmrc */
    addMacro(NULL, "_tmppath", NULL, "/tmp", RMIL_DEFAULT);

    ec = do_inject(cmds, ncmds, poptGetArgs(optCon));

    optCon = poptFreeContext(optCon);
    return ec;
}
