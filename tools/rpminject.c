#include "system.h"

#include "rpmbuild.h"
#include "buildio.h"

#include "header.h"
#include "rpmlead.h"
#include "signature.h"

#include <err.h>	/* XXX !HAVE_ERR_H: get from misc */
#include "debug.h"

typedef enum injmode_e { INJ_UNKNOWN, INJ_ADD, INJ_DELETE, INJ_MODIFY } injmode_t;

injmode_t injmode = INJ_UNKNOWN;

typedef struct cmd_s {
    injmode_t	injmode;
    char *	tag;
    int_32	tagval;
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
    /*@notreached@*/
}

static const char *hdri18ntbl = "HEADER_I18NTABLE";

static const char * getTagString(int tval)
{
    const struct headerTagTableEntry *t;

    for (t = rpmTagTable; t->name != NULL; t++) {
	if (t->val == tval)
	    return t->name;
    }
    if (tval == HEADER_I18NTABLE)
	return hdri18ntbl;
    return NULL;
}

static int getTagVal(const char *tname)
{
    const struct headerTagTableEntry *t;
    int tval;

    if (strncasecmp("RPMTAG_", tname, sizeof("RPMTAG_"))) {
	char *tagname = alloca(sizeof("RPMTAG_") + strlen(tname));
	sprintf(tagname, "RPMTAG_%s", tname);
	tname = tagname;
    }

    for (t = rpmTagTable; t->name != NULL; t++) {
	if (!strncasecmp(tname, t->name, strlen(t->name)))
	    return t->val;
    }
    if (!strcasecmp(tname, hdri18ntbl))
	return HEADER_I18NTABLE;

    tval = atoi(tname);
    return tval;
}

static const struct headerTypeTableEntry {
    char *name;
    int_32 val;
} rpmTypeTable[] = {
    {"RPM_NULL_TYPE",	0},
    {"RPM_CHAR_TYPE",	1},
    {"RPM_INT8_TYPE",	2},
    {"RPM_INT16_TYPE",	3},
    {"RPM_INT32_TYPE",	4},
    {"RPM_INT64_TYPE",	5},
    {"RPM_STRING_TYPE",	6},
    {"RPM_BIN_TYPE",	7},
    {"RPM_STRING_ARRAY_TYPE",	8},
    {"RPM_I18NSTRING_TYPE",	9},
    {NULL,	0}
};

static char *
getTypeString(int tval)
{
    const struct headerTypeTableEntry *t;
    static char buf[128];

    for (t = rpmTypeTable; t->name != NULL; t++) {
	if (t->val == tval)
	    return t->name;
    }
    sprintf(buf, "<RPM_%d_TYPE>", tval);
    return buf;
}

/* ========================================================================= */

enum cvtaction {CA_OLD, CA_NEW, CA_OMIT, CA_ERR};

static enum cvtaction convertAMD(enum cvtaction ca, int_32 type,
	void ** nvalsp, int_32 *ncountp, cmd_t *newc)
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
	{   int_32 *intp = xmalloc(newc->nvals * sizeof(*intp));
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

static enum cvtaction convertExistingAMD(int_32 tag, int_32 type,
	void ** valsp, int_32 *countp, void ** nvalsp, int_32 *ncountp,
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
    int_32 tag, type, count;
    void *vals;
    HeaderIterator headerIter;
    Header res = headerNew();
   
    headerIter = headerInitIterator(h);

    while (headerNextIterator(headerIter, &tag, &type, &vals, &count)) {
	enum cvtaction ca;
	void *nvals;
	int_32 ncount;

	nvals = NULL;
	ncount = 0;
	ca = convertExistingAMD(tag, type, &vals, &count, &nvals, &ncount, cmds, ncmds);
	switch (ca) {
	case CA_ERR:
	case CA_OLD:		/* copy old tag and values to header */	
	default:
	    /* Don't copy the old changelog, we'll do that later. */
	    switch (tag) {
	    case RPMTAG_CHANGELOGTIME:
	    case RPMTAG_CHANGELOGNAME:
	    case RPMTAG_CHANGELOGTEXT:
		break;
	    default:
		headerAddEntry(res, tag, type, vals, count);
		break;
	    }
	    break;
	case CA_NEW:		/* copy new tag and values to header */	
	    headerAddEntry(res, tag, type, nvals, ncount);
	    break;
	case CA_OMIT:		/* delete old tag and values from header */
	    break;
	}

	if (type == RPM_STRING_ARRAY_TYPE || type == RPM_I18NSTRING_TYPE)
	    free(vals);
	if (nvals)
	    free(nvals);
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
	    int_32 type, ncount;
	    void *nvals;
	    enum cvtaction ca;

	    type = (c->nvals > 0) ? RPM_STRING_ARRAY_TYPE : RPM_STRING_TYPE;
	    ca = convertAMD(CA_NEW, type, &nvals, &ncount, c);
	    if (ca == CA_NEW)
		headerAddEntry(h, c->tagval, type, nvals, ncount);
	    rc = headerIsEntry(h, c->tagval);
	}

	switch(c->injmode) {
	case INJ_ADD:
	    if (!(rc && c->done > 0)) {
		warnx(_("failed to add tag %s"), getTagString(c->tagval));
		ec = 1;
	    }
	    break;
	case INJ_DELETE:
	    if (!(!rc && c->done > 0)) {
		warnx(_("failed to delete tag %s"), getTagString(c->tagval));
		ec = 1;
	    }
	    break;
	case INJ_MODIFY:
	    if (!(rc && c->done > 0)) {
		warnx(_("failed to modify tag %s"), getTagString(c->tagval));
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
	static int cltags[] = {
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
	headerFree(*hdrp);
	headerSort(h);
	*hdrp = h;
    } else {
	headerFree(h);
    }

    return ec;
}

/* ========================================================================= */

static int
rewriteRPM(const char *fni, const char *fno, cmd_t *cmds[], int ncmds)
{
    struct rpmlead lead;	/* XXX FIXME: exorcize lead/arch/os */
    Header sigs;
    Spec spec;
    CSA_t csabuf, *csa = &csabuf;
    int rc;

    csa->cpioArchiveSize = 0;
    csa->cpioFdIn = fdNew("init (rewriteRPM)");
    csa->cpioList = NULL;
    csa->cpioCount = 0;
    csa->lead = &lead;		/* XXX FIXME: exorcize lead/arch/os */

    /* Read rpm and (partially) recreate spec/pkg control structures */
    if ((rc = readRPM(fni, &spec, &lead, &sigs, csa)) != 0)
	return rc;

    /* Inject new strings into header tags */
    if ((rc = headerInject(&spec->packages->header, cmds, ncmds)) != 0)
	goto exit;

    /* Rewrite the rpm */
    if (lead.type == RPMLEAD_SOURCE) {
	rc = writeRPM(&spec->packages->header, fno, (int)lead.type,
		csa, spec->passPhrase, &(spec->cookie));
    } else {
	rc = writeRPM(&spec->packages->header, fno, (int)lead.type,
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
	    warn(_("can't link temp input file %s"), fni);
	    ec++;
	    continue;
	}
	if (rewriteRPM(fni, fno, cmds, ncmds)) {
	    unlink(fno);
	    if (rename(fni, fno))
		warn(_("can't rename %s to %s"), fni, fno);
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
    char * optArg;
    cmd_t *c = NULL;
    int arg;
    int ec = 0;
    injmode_t lastmode = INJ_UNKNOWN;

#if HAVE_MCHECK_H && HAVE_MTRACE
    mtrace();  /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif

    setprogname(argv[0]);	/* Retrofit glibc __progname */
    (void)setlocale(LC_ALL, "" );

#ifdef  __LCLINT__
#define LOCALEDIR	"/usr/share/locale"
#endif
    (void)bindtextdomain(PACKAGE, LOCALEDIR);
    (void)textdomain(PACKAGE);

    optCon = poptGetContext("rpminject", argc, argv, optionsTable, 0);
    poptReadDefaultConfig(optCon, 1);

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
		errx(EXIT_FAILURE, _("missing inject mode before \"--tag %s\""), optArg);
	    if (c->tag) {
		if (c->injmode != INJ_DELETE &&
		  (c->nvals <= 0 || c->vals == NULL))
		    errx(EXIT_FAILURE, _("add/modify inject mode with \"--tag %s\" needs a value"), c->tag);
		cmds[ncmds] = c = xcalloc(1, sizeof(cmd_t));
		cmds[ncmds]->injmode = cmds[ncmds-1]->injmode;
		ncmds++;
	    }
	    c->tagval = getTagVal(optArg);
	    if (!((c->tagval >= RPMTAG_NAME && c->tagval < RPMTAG_FIRSTFREE_TAG)
	        || c->tagval >= RPMTAG_EXTERNAL_TAG))
		errx(EXIT_FAILURE, _("unknown rpm tag \"--tag %s\""), optArg);
	    c->tag = xstrdup(optArg);
	    break;
	case 'v':
	    if (ncmds == 0 || c == NULL)
		errx(EXIT_FAILURE, _("missing inject mode before \"--value %s\""), optArg);
	    if (c->tag == NULL)
		errx(EXIT_FAILURE, _("missing tag name before \"--value %s\""), optArg);
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
	    errx(EXIT_FAILURE, _("unknown popt return (%d)"), arg);
	    /*@notreached@*/ break;
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

    poptFreeContext(optCon);
    return ec;
}
