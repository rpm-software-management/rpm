/** \ingroup rpmcli
 * \file lib/query.c
 * Display tag values from package metadata.
 */

#include "system.h"

#ifndef PATH_MAX
# define PATH_MAX 255
#endif

#include <rpm/rpmcli.h>

#include <rpm/rpmdb.h>
#include <rpm/rpmfi.h>

#include <rpm/rpmgi.h>
#include <rpm/rpmts.h>

#include "lib/manifest.h"
#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>	/* rpmCleanPath */

#include "debug.h"
#include "lib/misc.h"		/* XXX for currentDirectory */


/**
 */
static void printFileInfo(char * te, const char * name,
			  unsigned int size, unsigned short mode,
			  unsigned int mtime,
			  unsigned short rdev, unsigned int nlink,
			  const char * owner, const char * group,
			  const char * linkto)
{
    char sizefield[15];
    char ownerfield[8+1], groupfield[8+1];
    char timefield[100];
    time_t when = mtime;  /* important if sizeof(int32_t) ! sizeof(time_t) */
    struct tm * tm;
    static time_t now;
    static struct tm nowtm;
    const char * namefield = name;
    char * perms = rpmPermsString(mode);

    /* On first call, grab snapshot of now */
    if (now == 0) {
	now = time(NULL);
	tm = localtime(&now);
	if (tm) nowtm = *tm;	/* structure assignment */
    }

    strncpy(ownerfield, owner, sizeof(ownerfield));
    ownerfield[sizeof(ownerfield)-1] = '\0';

    strncpy(groupfield, group, sizeof(groupfield));
    groupfield[sizeof(groupfield)-1] = '\0';

    /* this is normally right */
    sprintf(sizefield, "%12u", size);

    /* this knows too much about dev_t */

    if (S_ISLNK(mode)) {
	char *nf = alloca(strlen(name) + sizeof(" -> ") + strlen(linkto));
	sprintf(nf, "%s -> %s", name, linkto);
	namefield = nf;
    } else if (S_ISCHR(mode)) {
	perms[0] = 'c';
	sprintf(sizefield, "%3u, %3u", ((unsigned)(rdev >> 8) & 0xff),
			((unsigned)rdev & 0xff));
    } else if (S_ISBLK(mode)) {
	perms[0] = 'b';
	sprintf(sizefield, "%3u, %3u", ((unsigned)(rdev >> 8) & 0xff),
			((unsigned)rdev & 0xff));
    }

    /* Convert file mtime to display format */
    tm = localtime(&when);
    timefield[0] = '\0';
    if (tm != NULL)
    {	const char *fmt;
	if (now > when + 6L * 30L * 24L * 60L * 60L ||	/* Old. */
	    now < when - 60L * 60L)			/* In the future.  */
	{
	/* The file is fairly old or in the future.
	 * POSIX says the cutoff is 6 months old;
	 * approximate this by 6*30 days.
	 * Allow a 1 hour slop factor for what is considered "the future",
	 * to allow for NFS server/client clock disagreement.
	 * Show the year instead of the time of day.
	 */        
	    fmt = "%b %e  %Y";
	} else {
	    fmt = "%b %e %H:%M";
	}
	(void)strftime(timefield, sizeof(timefield) - 1, fmt, tm);
    }

    sprintf(te, "%s %4d %-8s%-8s %10s %s %s", perms,
	(int)nlink, ownerfield, groupfield, sizefield, timefield, namefield);
    perms = _free(perms);
}

/**
 */
static inline char * queryHeader(Header h, const char * qfmt)
{
    const char * errstr = "(unkown error)";
    char * str;

    str = headerSprintf(h, qfmt, rpmTagTable, rpmHeaderFormats, &errstr);
    if (str == NULL)
	rpmlog(RPMLOG_ERR, _("incorrect format: %s\n"), errstr);
    return str;
}

/**
 */
static void flushBuffer(char ** tp, char ** tep, int nonewline)
{
    char *t, *te;

    t = *tp;
    te = *tep;
    if (te > t) {
	if (!nonewline) {
	    *te++ = '\n';
	    *te = '\0';
	}
	rpmlog(RPMLOG_NOTICE, "%s", t);
	te = t;
	*t = '\0';
    }
    *tp = t;
    *tep = te;
}

int showQueryPackage(QVA_t qva, rpmts ts, Header h)
{
    int scareMem = 0;
    rpmfi fi = NULL;
    size_t tb = 2 * BUFSIZ;
    size_t sb;
    char * t, * te;
    char * prefix = NULL;
    int rc = 0;		/* XXX FIXME: need real return code */
    int i;

    te = t = xmalloc(tb);
    *te = '\0';

    if (qva->qva_queryFormat != NULL) {
	char * str = queryHeader(h, qva->qva_queryFormat);
	if (str) {
	    size_t tx = (te - t);

	    sb = strlen(str);
	    if (sb) {
		tb += sb;
		t = xrealloc(t, tb);
		te = t + tx;
	    }
	    te = stpcpy(te, str);
	    str = _free(str);
	    flushBuffer(&t, &te, 1);
	}
    }

    if (!(qva->qva_flags & QUERY_FOR_LIST))
	goto exit;

    fi = rpmfiNew(ts, h, RPMTAG_BASENAMES, scareMem);
    if (rpmfiFC(fi) <= 0) {
	te = stpcpy(te, _("(contains no files)"));
	goto exit;
    }

    fi = rpmfiInit(fi, 0);
    if (fi != NULL)
    while ((i = rpmfiNext(fi)) >= 0) {
	rpmfileAttrs fflags;
	unsigned short fmode;
 	unsigned short frdev;
	unsigned int fmtime;
	rpmfileState fstate;
	size_t fsize;
	const char * fn;
	char fmd5[32+1];
	const char * fuser;
	const char * fgroup;
	const char * flink;
	int32_t fnlink;

	fflags = rpmfiFFlags(fi);
	fmode = rpmfiFMode(fi);
	frdev = rpmfiFRdev(fi);
	fmtime = rpmfiFMtime(fi);
	fstate = rpmfiFState(fi);
	fsize = rpmfiFSize(fi);
	fn = rpmfiFN(fi);
	{   static char hex[] = "0123456789abcdef";
	    unsigned const char * s = rpmfiMD5(fi);
	    char * p = fmd5;
	    int j;
	    for (j = 0; j < 16; j++) {
		unsigned k = *s++;
		*p++ = hex[ (k >> 4) & 0xf ];
		*p++ = hex[ (k     ) & 0xf ];
	    }
	    *p = '\0';
	}
	fuser = rpmfiFUser(fi);
	fgroup = rpmfiFGroup(fi);
	flink = rpmfiFLink(fi);
	fnlink = rpmfiFNlink(fi);

	/* If querying only docs, skip non-doc files. */
	if ((qva->qva_flags & QUERY_FOR_DOCS) && !(fflags & RPMFILE_DOC))
	    continue;

	/* If querying only configs, skip non-config files. */
	if ((qva->qva_flags & QUERY_FOR_CONFIG) && !(fflags & RPMFILE_CONFIG))
	    continue;

	/* If not querying %ghost, skip ghost files. */
	if ((qva->qva_fflags & RPMFILE_GHOST) && (fflags & RPMFILE_GHOST))
	    continue;

	/* Insure space for header derived data */
	sb = strlen(fn) + strlen(fmd5) + strlen(fuser) + strlen(fgroup) + strlen(flink);
	if ((sb + BUFSIZ) > tb) {
	    size_t tx = (te - t);
	    tb += sb + BUFSIZ;
	    t = xrealloc(t, tb);
	    te = t + tx;
	}

	if (!rpmIsVerbose() && prefix)
	    te = stpcpy(te, prefix);

	if (qva->qva_flags & QUERY_FOR_STATE) {
	    switch (fstate) {
	    case RPMFILE_STATE_NORMAL:
		te = stpcpy(te, _("normal        "));
		break;
	    case RPMFILE_STATE_REPLACED:
		te = stpcpy(te, _("replaced      "));
		break;
	    case RPMFILE_STATE_NOTINSTALLED:
		te = stpcpy(te, _("not installed "));
		break;
	    case RPMFILE_STATE_NETSHARED:
		te = stpcpy(te, _("net shared    "));
		break;
	    case RPMFILE_STATE_WRONGCOLOR:
		te = stpcpy(te, _("wrong color   "));
		break;
	    case RPMFILE_STATE_MISSING:
		te = stpcpy(te, _("(no state)    "));
		break;
	    default:
		sprintf(te, _("(unknown %3d) "), fstate);
		te += strlen(te);
		break;
	    }
	}

	if (qva->qva_flags & QUERY_FOR_DUMPFILES) {
	    sprintf(te, "%s %d %d %s 0%o ", fn, (int)fsize, fmtime, fmd5, fmode);
	    te += strlen(te);

	    if (fuser && fgroup) {
		sprintf(te, "%s %s", fuser, fgroup);
		te += strlen(te);
	    } else {
		rpmlog(RPMLOG_ERR,
			_("package has not file owner/group lists\n"));
	    }

	    sprintf(te, " %s %s %u ", 
				 fflags & RPMFILE_CONFIG ? "1" : "0",
				 fflags & RPMFILE_DOC ? "1" : "0",
				 frdev);
	    te += strlen(te);

	    sprintf(te, "%s", (flink && *flink ? flink : "X"));
	    te += strlen(te);
	} else
	if (!rpmIsVerbose()) {
	    te = stpcpy(te, fn);
	}
	else {

	    /* XXX Adjust directory link count and size for display output. */
	    if (S_ISDIR(fmode)) {
		fnlink++;
		fsize = 0;
	    }

	    if (fuser && fgroup) {
		printFileInfo(te, fn, fsize, fmode, fmtime, frdev, fnlink,
					fuser, fgroup, flink);
		te += strlen(te);
	    } else {
		rpmlog(RPMLOG_ERR,
			_("package has neither file owner or id lists\n"));
	    }
	}
	flushBuffer(&t, &te, 0);
    }
	    
    rc = 0;

exit:
    flushBuffer(&t, &te, 0);
    t = _free(t);

    fi = rpmfiFree(fi);
    return rc;
}

void rpmDisplayQueryTags(FILE * fp)
{
    const struct headerTagTableEntry_s * t;
    int i, ttype;
    const struct headerSprintfExtension_s * ext = rpmHeaderFormats;

    for (i = 0, t = rpmTagTable; i < rpmTagTableSize; i++, t++) {
	if (t->name == NULL)
	    continue;
	if (rpmIsVerbose()) {
	    static const char * tagtypes[] = {
		"", "char", "int8", "int16", "int32", "int64",
		"string", "blob", "argv", "i18nstring"
	    };
	    fprintf(fp, "%-20s %6d", t->name + 7, t->val);
	    ttype = t->type & RPM_MASK_TYPE;
	    if (ttype > RPM_NULL_TYPE && ttype <= RPM_MAX_TYPE)
		fprintf(fp, " %s", tagtypes[ttype]);
	} else {
	    fprintf(fp, "%s", t->name + 7);
	}
	    
	fprintf(fp, "\n");
    }

    while (ext->name != NULL) {
	if (ext->type == HEADER_EXT_MORE) {
	    ext = ext->u.more;
	    continue;
	}
	/* XXX don't print query tags twice. */
	for (i = 0, t = rpmTagTable; i < rpmTagTableSize; i++, t++) {
	    if (t->name == NULL)	/* XXX programmer error. */
		continue;
	    if (!strcmp(t->name, ext->name))
	    	break;
	}
	if (i >= rpmTagTableSize && ext->type == HEADER_EXT_TAG)
	    fprintf(fp, "%s\n", ext->name + 7);
	ext++;
    }
}

static int rpmgiShowMatches(QVA_t qva, rpmts ts)
{
    rpmgi gi = qva->qva_gi;
    int ec = 0;

    while (rpmgiNext(gi) == RPMRC_OK) {
	Header h;
	int rc;

	h = rpmgiHeader(gi);
	if (h == NULL)		/* XXX perhaps stricter break instead? */
	    continue;
	if ((rc = qva->qva_showPackage(qva, ts, h)) != 0)
	    ec = rc;
	if (qva->qva_source == RPMQV_DBOFFSET)
	    break;
    }
    return rpmgiNumErrors(gi);
}

int rpmcliShowMatches(QVA_t qva, rpmts ts)
{
    Header h;
    int ec = 0;

    while ((h = rpmdbNextIterator(qva->qva_mi)) != NULL) {
	int rc;
	if ((rc = qva->qva_showPackage(qva, ts, h)) != 0)
	    ec = rc;
	if (qva->qva_source == RPMQV_DBOFFSET)
	    break;
    }
    qva->qva_mi = rpmdbFreeIterator(qva->qva_mi);
    return ec;
}

/**
 * Convert hex to binary nibble.
 * @param c            hex character
 * @return             binary nibble
 */
static inline unsigned char nibble(char c)
{
    if (c >= '0' && c <= '9')
	return (c - '0');
    if (c >= 'A' && c <= 'F')
	return (c - 'A') + 10;
    if (c >= 'a' && c <= 'f')
	return (c - 'a') + 10;
    return 0;
}

/* LCL: segfault (realpath annotation?) */
int rpmQueryVerify(QVA_t qva, rpmts ts, const char * arg)
{
    int res = 0;
    const char * s;
    int i;
    int provides_checked = 0;

    (void) rpmdbCheckSignals();

    if (qva->qva_showPackage == NULL)
	return 1;

    switch (qva->qva_source) {
    case RPMQV_RPM:
	res = rpmgiShowMatches(qva, ts);
	break;

    case RPMQV_ALL:
	res = rpmgiShowMatches(qva, ts);
	break;

    case RPMQV_HDLIST:
	res = rpmgiShowMatches(qva, ts);
	break;

    case RPMQV_FTSWALK:
	res = rpmgiShowMatches(qva, ts);
	break;

    case RPMQV_SPECFILE:
	res = ((qva->qva_specQuery != NULL)
		? qva->qva_specQuery(ts, qva, arg) : 1);
	break;

    case RPMQV_GROUP:
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_GROUP, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_NOTICE,
		_("group %s does not contain any packages\n"), arg);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);
	break;

    case RPMQV_TRIGGEREDBY:
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_TRIGGERNAME, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_NOTICE, _("no package triggers %s\n"), arg);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);
	break;

    case RPMQV_PKGID:
    {	unsigned char MD5[16];
	unsigned char * t;

	for (i = 0, s = arg; *s && isxdigit(*s); s++, i++)
	    {};
	if (i != 32) {
	    rpmlog(RPMLOG_NOTICE, _("malformed %s: %s\n"), "pkgid", arg);
	    return 1;
	}

	MD5[0] = '\0';
        for (i = 0, t = MD5, s = arg; i < 16; i++, t++, s += 2)
            *t = (nibble(s[0]) << 4) | nibble(s[1]);
	
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_SIGMD5, MD5, sizeof(MD5));
	if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_NOTICE, _("no package matches %s: %s\n"),
			"pkgid", arg);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);
    }	break;

    case RPMQV_HDRID:
	for (i = 0, s = arg; *s && isxdigit(*s); s++, i++)
	    {};
	if (i != 40) {
	    rpmlog(RPMLOG_NOTICE, _("malformed %s: %s\n"), "hdrid", arg);
	    return 1;
	}

	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_SHA1HEADER, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_NOTICE, _("no package matches %s: %s\n"),
			"hdrid", arg);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);
	break;

    case RPMQV_FILEID:
    {	unsigned char MD5[16];
	unsigned char * t;

	for (i = 0, s = arg; *s && isxdigit(*s); s++, i++)
	    {};
	if (i != 32) {
	    rpmlog(RPMLOG_ERR, _("malformed %s: %s\n"), "fileid", arg);
	    return 1;
	}

	MD5[0] = '\0';
        for (i = 0, t = MD5, s = arg; i < 16; i++, t++, s += 2)
            *t = (nibble(s[0]) << 4) | nibble(s[1]);

	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_FILEMD5S, MD5, sizeof(MD5));
	if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_NOTICE, _("no package matches %s: %s\n"),
			"fileid", arg);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);
    }	break;

    case RPMQV_TID:
    {	int mybase = 10;
	const char * myarg = arg;
	char * end = NULL;
	unsigned long iid;

	/* XXX should be in strtoul */
	if (*myarg == '0') {
	    myarg++;
	    mybase = 8;
	    if (*myarg == 'x') {
		myarg++;
		mybase = 16;
	    }
	}
	iid = strtoul(myarg, &end, mybase);
	if ((*end) || (end == arg) || (iid == ULONG_MAX)) {
	    rpmlog(RPMLOG_ERR, _("malformed %s: %s\n"), "tid", arg);
	    return 1;
	}
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_INSTALLTID, &iid, sizeof(iid));
	if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_NOTICE, _("no package matches %s: %s\n"),
			"tid", arg);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);
    }	break;

    case RPMQV_WHATREQUIRES:
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_REQUIRENAME, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_NOTICE, _("no package requires %s\n"), arg);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);
	break;

    case RPMQV_WHATPROVIDES:
	if (arg[0] != '/') {
	    provides_checked = 1;
	    qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_PROVIDENAME, arg, 0);
	    if (qva->qva_mi == NULL) {
		rpmlog(RPMLOG_NOTICE, _("no package provides %s\n"), arg);
		res = 1;
	    } else
		res = rpmcliShowMatches(qva, ts);
	    break;
	}
    case RPMQV_PATH:
    {   char * fn;
	int myerrno = 0;

	for (s = arg; *s != '\0'; s++)
	    if (!(*s == '.' || *s == '/'))
		break;

	if (*s == '\0') {
	    char fnbuf[PATH_MAX];
	    fn = realpath(arg, fnbuf);
	    if (fn)
		fn = xstrdup(fn);
	    else
		fn = xstrdup(arg);
	} else if (*arg != '/') {
	    char *curDir = currentDirectory();
	    fn = (char *) rpmGetPath(curDir, "/", arg, NULL);
	    curDir = _free(curDir);
	} else
	    fn = xstrdup(arg);
	(void) rpmCleanPath(fn);

	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_BASENAMES, fn, 0);
	if (qva->qva_mi == NULL) {
	    if (access(fn, F_OK) != 0)
		myerrno = errno;
	    else if (!provides_checked)
		qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_PROVIDENAME, fn, 0);
	}

	if (myerrno != 0) {
	    rpmlog(RPMLOG_ERR, _("file %s: %s\n"), fn, strerror(myerrno));
	    res = 1;
	} else if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_NOTICE,
		_("file %s is not owned by any package\n"), fn);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);

	fn = _free(fn);
    }	break;

    case RPMQV_DBOFFSET:
    {	int mybase = 10;
	const char * myarg = arg;
	char * end = NULL;
	unsigned long recOffset;

	/* XXX should be in strtoul */
	if (*myarg == '0') {
	    myarg++;
	    mybase = 8;
	    if (*myarg == 'x') {
		myarg++;
		mybase = 16;
	    }
	}
	recOffset = strtoul(myarg, &end, mybase);
	if ((*end) || (end == arg) || (recOffset == ULONG_MAX)) {
	    rpmlog(RPMLOG_NOTICE, _("invalid package number: %s\n"), arg);
	    return 1;
	}
	rpmlog(RPMLOG_DEBUG, _("package record number: %u\n"), recOffset);
	/* RPMDBI_PACKAGES */
	qva->qva_mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, &recOffset, sizeof(recOffset));
	if (qva->qva_mi == NULL) {
	    rpmlog(RPMLOG_NOTICE,
		_("record %u could not be read\n"), recOffset);
	    res = 1;
	} else
	    res = rpmcliShowMatches(qva, ts);
    }	break;

    case RPMQV_PACKAGE:
    {
	int matches = 0;
	rpmdbMatchIterator mi;
	mi = rpmtsInitIterator(ts, RPMDBI_LABEL, arg, 0);
	while (rpmdbNextIterator(mi) != NULL) {
	    matches++;
	}
	rpmdbFreeIterator(mi);
	if (! matches) {
	    rpmlog(RPMLOG_NOTICE, _("package %s is not installed\n"), arg);
	    res = 1;
	} else {
	    qva->qva_mi = rpmtsInitIterator(ts, RPMDBI_LABEL, arg, 0);
	    res = rpmcliShowMatches(qva, ts);
	}
	break;
    }
    
    }
   
    return res;
}

int rpmcliArgIter(rpmts ts, QVA_t qva, ARGV_t argv)
{
    rpmRC rpmrc = RPMRC_NOTFOUND;
    int ec = 0;

    switch (qva->qva_source) {
    case RPMQV_ALL:
	qva->qva_gi = rpmgiNew(ts, RPMDBI_PACKAGES, NULL, 0);
	qva->qva_rc = rpmgiSetArgs(qva->qva_gi, argv, ftsOpts, RPMGI_NONE);

	if (qva->qva_gi != NULL && (rpmgiGetFlags(qva->qva_gi) & RPMGI_TSADD))	/* Load the ts with headers. */
	while ((rpmrc = rpmgiNext(qva->qva_gi)) == RPMRC_OK)
	    {};
	if (rpmrc != RPMRC_NOTFOUND)
	    return 1;	/* XXX should be no. of failures. */
	
	/* FIX: argv can be NULL, cast to pass argv array */
	ec = rpmQueryVerify(qva, ts, (const char *) argv);
	rpmtsEmpty(ts);
	break;
    case RPMQV_RPM:
	qva->qva_gi = rpmgiNew(ts, RPMDBI_ARGLIST, NULL, 0);
	qva->qva_rc = rpmgiSetArgs(qva->qva_gi, argv, ftsOpts, giFlags);

	if (qva->qva_gi != NULL && (rpmgiGetFlags(qva->qva_gi) & RPMGI_TSADD))	/* Load the ts with headers. */
	while ((rpmrc = rpmgiNext(qva->qva_gi)) == RPMRC_OK)
	    {};
	if (rpmrc != RPMRC_NOTFOUND)
	    return 1;	/* XXX should be no. of failures. */
	
	/* FIX: argv can be NULL, cast to pass argv array */
	ec = rpmQueryVerify(qva, ts, NULL);
	rpmtsEmpty(ts);
	break;
    case RPMQV_HDLIST:
	qva->qva_gi = rpmgiNew(ts, RPMDBI_HDLIST, NULL, 0);
	qva->qva_rc = rpmgiSetArgs(qva->qva_gi, argv, ftsOpts, giFlags);

	if (qva->qva_gi != NULL && (rpmgiGetFlags(qva->qva_gi) & RPMGI_TSADD))	/* Load the ts with headers. */
	while ((rpmrc = rpmgiNext(qva->qva_gi)) == RPMRC_OK)
	    {};
	if (rpmrc != RPMRC_NOTFOUND)
	    return 1;	/* XXX should be no. of failures. */
	
	/* FIX: argv can be NULL, cast to pass argv array */
	ec = rpmQueryVerify(qva, ts, NULL);
	rpmtsEmpty(ts);
	break;
    case RPMQV_FTSWALK:
	if (ftsOpts == 0)
	    ftsOpts = (RPMGI_COMFOLLOW | RPMGI_LOGICAL | RPMGI_NOSTAT);
	qva->qva_gi = rpmgiNew(ts, RPMDBI_FTSWALK, NULL, 0);
	qva->qva_rc = rpmgiSetArgs(qva->qva_gi, argv, ftsOpts, giFlags);

	if (qva->qva_gi != NULL && (rpmgiGetFlags(qva->qva_gi) & RPMGI_TSADD))	/* Load the ts with headers. */
	while ((rpmrc = rpmgiNext(qva->qva_gi)) == RPMRC_OK)
	    {};
	if (rpmrc != RPMRC_NOTFOUND)
	    return 1;	/* XXX should be no. of failures. */
	
	/* FIX: argv can be NULL, cast to pass argv array */
	ec = rpmQueryVerify(qva, ts, NULL);
	rpmtsEmpty(ts);
	break;
    default:
	qva->qva_gi = rpmgiNew(ts, RPMDBI_ARGLIST, NULL, 0);
	qva->qva_rc = rpmgiSetArgs(qva->qva_gi, argv, ftsOpts,
		(giFlags | (RPMGI_NOGLOB|RPMGI_NOHEADER)));
	while (rpmgiNext(qva->qva_gi) == RPMRC_OK) {
	    ec += rpmQueryVerify(qva, ts, rpmgiHdrPath(qva->qva_gi));
	    rpmtsEmpty(ts);
	}
	break;
    }

    qva->qva_gi = rpmgiFree(qva->qva_gi);

    return ec;
}

int rpmcliQuery(rpmts ts, QVA_t qva, const char ** argv)
{
    rpmVSFlags vsflags, ovsflags;
    int ec = 0;

    if (qva->qva_showPackage == NULL)
	qva->qva_showPackage = showQueryPackage;

    /* If --queryformat unspecified, then set default now. */
    if (!(qva->qva_flags & _QUERY_FOR_BITS) && qva->qva_queryFormat == NULL) {
	char * fmt = rpmExpand("%{?_query_all_fmt}\n", NULL);
	if (fmt == NULL || strlen(fmt) <= 1) {
	    fmt = _free(fmt);
	    fmt = xstrdup("%{name}-%{version}-%{release}.%{arch}\n");
	}
	qva->qva_queryFormat = fmt;
    }

    vsflags = rpmExpandNumeric("%{?_vsflags_query}");
    if (qva->qva_flags & VERIFY_DIGEST)
	vsflags |= _RPMVSF_NODIGESTS;
    if (qva->qva_flags & VERIFY_SIGNATURE)
	vsflags |= _RPMVSF_NOSIGNATURES;
    if (qva->qva_flags & VERIFY_HDRCHK)
	vsflags |= RPMVSF_NOHDRCHK;

    ovsflags = rpmtsSetVSFlags(ts, vsflags);
    ec = rpmcliArgIter(ts, qva, argv);
    vsflags = rpmtsSetVSFlags(ts, ovsflags);

    if (qva->qva_showPackage == showQueryPackage)
	qva->qva_showPackage = NULL;

    return ec;
}
