/** \ingroup rpmcli
 * \file lib/query.c
 * Display tag values from package metadata.
 */

#include "system.h"

#ifndef PATH_MAX
/*@-incondefs@*/	/* FIX: long int? */
# define PATH_MAX 255
/*@=incondefs@*/
#endif

#include <rpmcli.h>
#include <rpmbuild.h>

#include "rpmdb.h"
#include "rpmfi.h"
#include "rpmts.h"

#include "manifest.h"

#include "debug.h"

/*@access rpmdbMatchIterator@*/		/* XXX compared with NULL */
/*@access Header@*/			/* XXX compared with NULL */
/*@access FD_t@*/			/* XXX compared with NULL */

/**
 */
static void printFileInfo(char * te, const char * name,
			  unsigned int size, unsigned short mode,
			  unsigned int mtime,
			  unsigned short rdev, unsigned int nlink,
			  const char * owner, const char * group,
			  const char * linkto)
	/*@modifies *te @*/
{
    char sizefield[15];
    char ownerfield[8+1], groupfield[8+1];
    char timefield[100];
    time_t when = mtime;  /* important if sizeof(int_32) ! sizeof(time_t) */
    struct tm * tm;
    static time_t now;
    static struct tm nowtm;
    const char * namefield = name;
    char * perms = rpmPermsString(mode);

    /* On first call, grab snapshot of now */
    if (now == 0) {
	now = time(NULL);
	tm = localtime(&now);
/*@-boundsread@*/
	if (tm) nowtm = *tm;	/* structure assignment */
/*@=boundsread@*/
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
static inline /*@null@*/ const char * queryHeader(Header h, const char * qfmt)
	/*@*/
{
    const char * errstr = "(unkown error)";
    const char * str;

/*@-modobserver@*/
    str = headerSprintf(h, qfmt, rpmTagTable, rpmHeaderFormats, &errstr);
/*@=modobserver@*/
    if (str == NULL)
	rpmError(RPMERR_QFMT, _("incorrect format: %s\n"), errstr);
    return str;
}

int showQueryPackage(QVA_t qva, /*@unused@*/ rpmts ts, Header h)
{
    int scareMem = 1;
    rpmfi fi = NULL;
    char * t, * te;
    char * prefix = NULL;
    int rc = 0;		/* XXX FIXME: need real return code */
    int nonewline = 0;
    int i;

    te = t = xmalloc(BUFSIZ);
/*@-boundswrite@*/
    *te = '\0';
/*@=boundswrite@*/

    if (!(qva->qva_flags & _QUERY_FOR_BITS) && qva->qva_queryFormat == NULL)
    {
	const char * name, * version, * release;
	(void) headerNVR(h, &name, &version, &release);
/*@-boundswrite@*/
	te = stpcpy(te, name);
	te = stpcpy( stpcpy(te, "-"), version);
	te = stpcpy( stpcpy(te, "-"), release);
/*@=boundswrite@*/
	goto exit;
    }

    if (qva->qva_queryFormat != NULL) {
	const char * str = queryHeader(h, qva->qva_queryFormat);
	nonewline = 1;
	/*@-branchstate@*/
	if (str) {
	    size_t tb = (te - t);
	    size_t sb = strlen(str);

	    if (sb >= (BUFSIZ - tb)) {
		t = xrealloc(t, BUFSIZ+sb);
		te = t + tb;
	    }
/*@-boundswrite@*/
	    /*@-usereleased@*/
	    te = stpcpy(te, str);
	    /*@=usereleased@*/
/*@=boundswrite@*/
	    str = _free(str);
	}
	/*@=branchstate@*/
    }

    if (!(qva->qva_flags & QUERY_FOR_LIST))
	goto exit;

    fi = rpmfiNew(ts, h, RPMTAG_BASENAMES, scareMem);
    if (rpmfiFC(fi) <= 0) {
/*@-boundswrite@*/
	te = stpcpy(te, _("(contains no files)"));
/*@=boundswrite@*/
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
	int_32 fnlink;

	fflags = rpmfiFFlags(fi);
	fmode = rpmfiFMode(fi);
	frdev = rpmfiFRdev(fi);
	fmtime = rpmfiFMtime(fi);
	fstate = rpmfiFState(fi);
	fsize = rpmfiFSize(fi);
	fn = rpmfiFN(fi);
/*@-bounds@*/
	{   static char hex[] = "0123456789abcdef";
	    const char * s = rpmfiMD5(fi);
	    char * p = fmd5;
	    int j;
	    for (j = 0; j < 16; j++) {
		unsigned k = *s++;
		*p++ = hex[ (k >> 4) & 0xf ];
		*p++ = hex[ (k     ) & 0xf ];
	    }
	    *p = '\0';
	}
/*@=bounds@*/
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
	if (!(qva->qva_fflags & RPMFILE_GHOST) && (fflags & RPMFILE_GHOST))
	    continue;

/*@-boundswrite@*/
	if (!rpmIsVerbose() && prefix)
	    te = stpcpy(te, prefix);

	if (qva->qva_flags & QUERY_FOR_STATE) {
	    switch (fstate) {
	    case RPMFILE_STATE_NORMAL:
		te = stpcpy(te, _("normal        "));
		/*@switchbreak@*/ break;
	    case RPMFILE_STATE_REPLACED:
		te = stpcpy(te, _("replaced      "));
		/*@switchbreak@*/ break;
	    case RPMFILE_STATE_NOTINSTALLED:
		te = stpcpy(te, _("not installed "));
		/*@switchbreak@*/ break;
	    case RPMFILE_STATE_NETSHARED:
		te = stpcpy(te, _("net shared    "));
		/*@switchbreak@*/ break;
	    case RPMFILE_STATE_MISSING:
		te = stpcpy(te, _("(no state)    "));
		/*@switchbreak@*/ break;
	    default:
		sprintf(te, _("(unknown %3d) "), fstate);
		te += strlen(te);
		/*@switchbreak@*/ break;
	    }
	}
/*@=boundswrite@*/

	if (qva->qva_flags & QUERY_FOR_DUMPFILES) {
	    sprintf(te, "%s %d %d %s 0%o ", fn, fsize, fmtime, fmd5, fmode);
	    te += strlen(te);

	    if (fuser && fgroup) {
/*@-nullpass@*/
		sprintf(te, "%s %s", fuser, fgroup);
/*@=nullpass@*/
		te += strlen(te);
	    } else {
		rpmError(RPMERR_INTERNAL,
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
/*@-boundswrite@*/
	    te = stpcpy(te, fn);
/*@=boundswrite@*/
	}
	else {

	    /* XXX Adjust directory link count and size for display output. */
	    if (S_ISDIR(fmode)) {
		fnlink++;
		fsize = 0;
	    }

	    if (fuser && fgroup) {
/*@-nullpass@*/
		printFileInfo(te, fn, fsize, fmode, fmtime, frdev, fnlink,
					fuser, fgroup, flink);
/*@=nullpass@*/
		te += strlen(te);
	    } else {
		rpmError(RPMERR_INTERNAL,
			_("package has neither file owner or id lists\n"));
	    }
	}
	if (te > t) {
/*@-boundswrite@*/
	    *te++ = '\n';
	    *te = '\0';
	    rpmMessage(RPMMESS_NORMAL, "%s", t);
	    te = t;
	    *t = '\0';
/*@=boundswrite@*/
	}
    }
	    
    rc = 0;

exit:
    if (te > t) {
	if (!nonewline) {
/*@-boundswrite@*/
	    *te++ = '\n';
	    *te = '\0';
/*@=boundswrite@*/
	}
	rpmMessage(RPMMESS_NORMAL, "%s", t);
    }
    t = _free(t);

    fi = rpmfiFree(fi);
    return rc;
}

/**
 * Print copy of spec file, filling in Group/Description/Summary from specspo.
 * @param spec		spec file control structure
 */
static void
printNewSpecfile(Spec spec)
	/*@globals fileSystem @*/
	/*@modifies spec->sl->sl_lines[], fileSystem @*/
{
    Header h;
    speclines sl = spec->sl;
    spectags st = spec->st;
    const char * msgstr = NULL;
    int i, j;

    if (sl == NULL || st == NULL)
	return;

    /*@-branchstate@*/
    for (i = 0; i < st->st_ntags; i++) {
	spectag t = st->st_t + i;
	const char * tn = tagName(t->t_tag);
	const char * errstr;
	char fmt[1024];

	fmt[0] = '\0';
	if (t->t_msgid == NULL)
	    h = spec->packages->header;
	else {
	    Package pkg;
	    char *fe;

/*@-bounds@*/
	    strcpy(fmt, t->t_msgid);
	    for (fe = fmt; *fe && *fe != '('; fe++)
		{} ;
	    if (*fe == '(') *fe = '\0';
/*@=bounds@*/
	    h = NULL;
	    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
		const char *pkgname;
		h = pkg->header;
		(void) headerNVR(h, &pkgname, NULL, NULL);
		if (!strcmp(pkgname, fmt))
		    /*@innerbreak@*/ break;
	    }
	    if (pkg == NULL || h == NULL)
		h = spec->packages->header;
	}

	if (h == NULL)
	    continue;

	fmt[0] = '\0';
/*@-boundswrite@*/
	(void) stpcpy( stpcpy( stpcpy( fmt, "%{"), tn), "}");
/*@=boundswrite@*/
	msgstr = _free(msgstr);

	/* XXX this should use queryHeader(), but prints out tn as well. */
	msgstr = headerSprintf(h, fmt, rpmTagTable, rpmHeaderFormats, &errstr);
	if (msgstr == NULL) {
	    rpmError(RPMERR_QFMT, _("can't query %s: %s\n"), tn, errstr);
	    return;
	}

/*@-boundswrite@*/
	switch(t->t_tag) {
	case RPMTAG_SUMMARY:
	case RPMTAG_GROUP:
	    /*@-unqualifiedtrans@*/
	    sl->sl_lines[t->t_startx] = _free(sl->sl_lines[t->t_startx]);
	    /*@=unqualifiedtrans@*/
	    if (t->t_lang && strcmp(t->t_lang, RPMBUILD_DEFAULT_LANG))
		continue;
	    {   char *buf = xmalloc(strlen(tn) + sizeof(": ") + strlen(msgstr));
		(void) stpcpy( stpcpy( stpcpy(buf, tn), ": "), msgstr);
		sl->sl_lines[t->t_startx] = buf;
	    }
	    /*@switchbreak@*/ break;
	case RPMTAG_DESCRIPTION:
	    for (j = 1; j < t->t_nlines; j++) {
		if (*sl->sl_lines[t->t_startx + j] == '%')
		    /*@innercontinue@*/ continue;
		/*@-unqualifiedtrans@*/
		sl->sl_lines[t->t_startx + j] =
			_free(sl->sl_lines[t->t_startx + j]);
		/*@=unqualifiedtrans@*/
	    }
	    if (t->t_lang && strcmp(t->t_lang, RPMBUILD_DEFAULT_LANG)) {
		sl->sl_lines[t->t_startx] = _free(sl->sl_lines[t->t_startx]);
		continue;
	    }
	    sl->sl_lines[t->t_startx + 1] = xstrdup(msgstr);
	    if (t->t_nlines > 2)
		sl->sl_lines[t->t_startx + 2] = xstrdup("\n\n");
	    /*@switchbreak@*/ break;
	}
/*@=boundswrite@*/
    }
    /*@=branchstate@*/
    msgstr = _free(msgstr);

/*@-boundsread@*/
    for (i = 0; i < sl->sl_nlines; i++) {
	const char * s = sl->sl_lines[i];
	if (s == NULL)
	    continue;
	printf("%s", s);
	if (strchr(s, '\n') == NULL && s[strlen(s)-1] != '\n')
	    printf("\n");
    }
/*@=boundsread@*/
}

void rpmDisplayQueryTags(FILE * fp)
{
    const struct headerTagTableEntry_s * t;
    int i;
    const struct headerSprintfExtension_s * ext = rpmHeaderFormats;

    for (i = 0, t = rpmTagTable; i < rpmTagTableSize; i++, t++)
	if (t->name) fprintf(fp, "%s\n", t->name + 7);

    while (ext->name != NULL) {
	if (ext->type == HEADER_EXT_MORE) {
	    ext = ext->u.more;
	    continue;
	}
	/* XXX don't print query tags twice. */
	for (i = 0, t = rpmTagTable; i < rpmTagTableSize; i++, t++) {
	    if (t->name == NULL)	/* XXX programmer error. */
		/*@innercontinue@*/ continue;
	    if (!strcmp(t->name, ext->name))
	    	/*@innerbreak@*/ break;
	}
	if (i >= rpmTagTableSize && ext->type == HEADER_EXT_TAG)
	    fprintf(fp, "%s\n", ext->name + 7);
	ext++;
    }
}

int rpmcliShowMatches(QVA_t qva, rpmts ts)
{
    Header h;
    int ec = 0;

    while ((h = rpmdbNextIterator(qva->qva_mi)) != NULL) {
	int rc;
	if ((rc = qva->qva_showPackage(qva, ts, h)) != 0)
	    ec = rc;
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
	/*@*/
{
    if (c >= '0' && c <= '9')
	return (c - '0');
    if (c >= 'A' && c <= 'F')
	return (c - 'A') + 10;
    if (c >= 'a' && c <= 'f')
	return (c - 'a') + 10;
    return 0;
}

/*@-redecl@*/
/**
 * @todo Eliminate linkage loop into librpmbuild.a
 */
int	(*parseSpecVec) (Spec *specp, const char *specFile, const char *rootdir,
		const char *buildRoot, int recursing, const char *passPhrase,
		char *cookie, int anyarch, int force) = NULL;
/**
 * @todo Eliminate linkage loop into librpmbuild.a
 */
/*@null@*/ Spec	(*freeSpecVec) (Spec spec) = NULL;
/*@=redecl@*/

/*@-bounds@*/ /* LCL: segfault (realpath annotation?) */
int rpmQueryVerify(QVA_t qva, rpmts ts, const char * arg)
{
    const char ** av = NULL;
    int res = 0;
    Header h;
    int rc;
    int xx;
    const char * s;
    int i;

    if (qva->qva_showPackage == NULL)
	return 1;

    /*@-branchstate@*/
    switch (qva->qva_source) {
    case RPMQV_RPM:
    {	int ac = 0;
	const char * fileURL = NULL;
	rpmRC rpmrc;

	rc = rpmGlob(arg, &ac, &av);
	if (rc) return 1;

restart:
	for (i = 0; i < ac; i++) {
	    FD_t fd;

	    fileURL = _free(fileURL);
	    fileURL = av[i];
	    av[i] = NULL;

	    /* Try to read the header from a package file. */
	    fd = Fopen(fileURL, "r.ufdio");
	    if (fd == NULL || Ferror(fd)) {
		rpmError(RPMERR_OPEN, _("open of %s failed: %s\n"), fileURL,
			Fstrerror(fd));
		if (fd) (void) Fclose(fd);
		res = 1;
		/*@loopbreak@*/ break;
	    }

    	    rpmrc = rpmReadPackageFile(ts, fd, fileURL, &h);

	    (void) Fclose(fd);

	    if (!(rpmrc == RPMRC_OK || rpmrc == RPMRC_NOTFOUND)) {
		rpmError(RPMERR_QUERY, _("query of %s failed\n"), fileURL);
		res = 1;
		/*@loopbreak@*/ break;
	    }
	    if (rpmrc == RPMRC_OK && h == NULL) {
		rpmError(RPMERR_QUERY,
			_("old format source packages cannot be queried\n"));
		res = 1;
		/*@loopbreak@*/ break;
	    }

	    /* Query a package file. */
	    if (rpmrc == RPMRC_OK) {
		res = qva->qva_showPackage(qva, ts, h);
		h = headerFree(h);
		rpmtsEmpty(ts);
		continue;
	    }

	    /* Try to read a package manifest. */
	    fd = Fopen(fileURL, "r.fpio");
	    if (fd == NULL || Ferror(fd)) {
		rpmError(RPMERR_OPEN, _("open of %s failed: %s\n"), fileURL,
			Fstrerror(fd));
		if (fd) (void) Fclose(fd);
		res = 1;
		/*@loopbreak@*/ break;
	    }
	    
	    /* Read list of packages from manifest. */
	    res = rpmReadPackageManifest(fd, &ac, &av);
	    if (res != RPMRC_OK) {
		rpmError(RPMERR_MANIFEST, _("%s: not an rpm package (or package manifest): %s\n"),
			fileURL, Fstrerror(fd));
		res = 1;
	    }
	    (void) Fclose(fd);

	    /* If successful, restart the query loop. */
	    if (res == 0)
		goto restart;

	    /*@loopbreak@*/ break;
	}

	fileURL = _free(fileURL);
	if (av) {
	    for (i = 0; i < ac; i++)
		av[i] = _free(av[i]);
	    av = _free(av);
	}
    }	break;

    case RPMQV_SPECFILE:
	if (qva->qva_showPackage != showQueryPackage)
	    return 1;

	/* XXX Eliminate linkage dependency loop */
	if (parseSpecVec == NULL || freeSpecVec == NULL)
	    return 1;

      { Spec spec = NULL;
	Package pkg;
	char * buildRoot = NULL;
	int recursing = 0;
	char * passPhrase = "";
	char *cookie = NULL;
	int anyarch = 1;
	int force = 1;

	/*@-mods@*/ /* FIX: make spec abstract */
	rc = parseSpecVec(&spec, arg, "/", buildRoot, recursing, passPhrase,
		cookie, anyarch, force);
	/*@=mods@*/
	if (rc || spec == NULL) {
	    rpmError(RPMERR_QUERY,
	    		_("query of specfile %s failed, can't parse\n"), arg);
	    spec = freeSpecVec(spec);
	    res = 1;
	    break;
	}

	if (specedit) {
	    printNewSpecfile(spec);
	    spec = freeSpecVec(spec);
	    res = 0;
	    break;
	}

	for (pkg = spec->packages; pkg != NULL; pkg = pkg->next)
	    xx = qva->qva_showPackage(qva, ts, pkg->header);
	spec = freeSpecVec(spec);
      }	break;

    case RPMQV_ALL:
	qva->qva_mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, NULL, 0);
	if (qva->qva_mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("no packages\n"));
	    res = 1;
	} else {
	    if (arg != NULL)
	    for (av = (const char **) arg; *av; av++) {
		if (!rpmdbSetIteratorRE(qva->qva_mi, RPMTAG_NAME, RPMMIRE_DEFAULT, *av))
		    continue;
		qva->qva_mi = rpmdbFreeIterator(qva->qva_mi);
		res = 1;
		/*@loopbreak@*/ break;
	    }
	    if (!res)
		res = rpmcliShowMatches(qva, ts);
	}
	break;

    case RPMQV_GROUP:
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_GROUP, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmError(RPMERR_QUERYINFO,
		_("group %s does not contain any packages\n"), arg);
	    res = 1;
	} else {
	    res = rpmcliShowMatches(qva, ts);
	}
	break;

    case RPMQV_TRIGGEREDBY:
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_TRIGGERNAME, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("no package triggers %s\n"), arg);
	    res = 1;
	} else {
	    res = rpmcliShowMatches(qva, ts);
	}
	break;

    case RPMQV_PKGID:
    {	unsigned char MD5[16];
	unsigned char * t;

	for (i = 0, s = arg; *s && isxdigit(*s); s++, i++)
	    {};
	if (i != 32) {
	    rpmError(RPMERR_QUERYINFO, _("malformed %s: %s\n"), "pkgid", arg);
	    return 1;
	}

	MD5[0] = '\0';
        for (i = 0, t = MD5, s = arg; i < 16; i++, t++, s += 2)
            *t = (nibble(s[0]) << 4) | nibble(s[1]);
	
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_SIGMD5, MD5, sizeof(MD5));
	if (qva->qva_mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("no package matches %s: %s\n"),
			"pkgid", arg);
	    res = 1;
	} else {
	    res = rpmcliShowMatches(qva, ts);
	}
    }	break;

    case RPMQV_HDRID:
	for (i = 0, s = arg; *s && isxdigit(*s); s++, i++)
	    {};
	if (i != 40) {
	    rpmError(RPMERR_QUERYINFO, _("malformed %s: %s\n"), "hdrid", arg);
	    return 1;
	}

	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_SHA1HEADER, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("no package matches %s: %s\n"),
			"hdrid", arg);
	    res = 1;
	} else {
	    res = rpmcliShowMatches(qva, ts);
	}
	break;

    case RPMQV_FILEID:
    {	unsigned char MD5[16];
	unsigned char * t;

	for (i = 0, s = arg; *s && isxdigit(*s); s++, i++)
	    {};
	if (i != 32) {
	    rpmError(RPMERR_QUERY, _("malformed %s: %s\n"), "fileid", arg);
	    return 1;
	}

	MD5[0] = '\0';
        for (i = 0, t = MD5, s = arg; i < 16; i++, t++, s += 2)
            *t = (nibble(s[0]) << 4) | nibble(s[1]);

	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_FILEMD5S, MD5, sizeof(MD5));
	if (qva->qva_mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("no package matches %s: %s\n"),
			"fileid", arg);
	    res = 1;
	} else {
	    res = rpmcliShowMatches(qva, ts);
	}
    }	break;

    case RPMQV_TID:
    {	int mybase = 10;
	const char * myarg = arg;
	char * end = NULL;
	unsigned iid;

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
	    rpmError(RPMERR_QUERY, _("malformed %s: %s\n"), "tid", arg);
	    return 1;
	}
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_INSTALLTID, &iid, sizeof(iid));
	if (qva->qva_mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("no package matches %s: %s\n"),
			"tid", arg);
	    res = 1;
	} else {
	    res = rpmcliShowMatches(qva, ts);
	}
    }	break;

    case RPMQV_WHATREQUIRES:
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_REQUIRENAME, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("no package requires %s\n"), arg);
	    res = 1;
	} else {
	    res = rpmcliShowMatches(qva, ts);
	}
	break;

    case RPMQV_WHATPROVIDES:
	if (arg[0] != '/') {
	    qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_PROVIDENAME, arg, 0);
	    if (qva->qva_mi == NULL) {
		rpmError(RPMERR_QUERYINFO, _("no package provides %s\n"), arg);
		res = 1;
	    } else {
		res = rpmcliShowMatches(qva, ts);
	    }
	    break;
	}
	/*@fallthrough@*/
    case RPMQV_PATH:
    {   char * fn;

	for (s = arg; *s != '\0'; s++)
	    if (!(*s == '.' || *s == '/'))
		/*@loopbreak@*/ break;

	if (*s == '\0') {
	    char fnbuf[PATH_MAX];
	    fn = realpath(arg, fnbuf);
	    if (fn)
		fn = xstrdup(fn);
	    else
		fn = xstrdup(arg);
	} else
	    fn = xstrdup(arg);
	(void) rpmCleanPath(fn);

	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_BASENAMES, fn, 0);
	if (qva->qva_mi == NULL) {
	    int myerrno = 0;
	    if (access(fn, F_OK) != 0)
		myerrno = errno;
	    switch (myerrno) {
	    default:
		rpmError(RPMERR_QUERY,
			_("file %s: %s\n"), fn, strerror(myerrno));
		/*@innerbreak@*/ break;
	    case 0:
		rpmError(RPMERR_QUERYINFO,
			_("file %s is not owned by any package\n"), fn);
		/*@innerbreak@*/ break;
	    }
	    res = 1;
	} else {
	    res = rpmcliShowMatches(qva, ts);
	}
	fn = _free(fn);
    }	break;

    case RPMQV_DBOFFSET:
    {	int mybase = 10;
	const char * myarg = arg;
	char * end = NULL;
	unsigned recOffset;

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
	    rpmError(RPMERR_QUERYINFO, _("invalid package number: %s\n"), arg);
	    return 1;
	}
	rpmMessage(RPMMESS_DEBUG, _("package record number: %u\n"), recOffset);
	/* RPMDBI_PACKAGES */
	qva->qva_mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, &recOffset, sizeof(recOffset));
	if (qva->qva_mi == NULL) {
	    rpmError(RPMERR_QUERYINFO,
		_("record %u could not be read\n"), recOffset);
	    res = 1;
	} else {
	    res = rpmcliShowMatches(qva, ts);
	}
    }	break;

    case RPMQV_PACKAGE:
	/* XXX HACK to get rpmdbFindByLabel out of the API */
	qva->qva_mi = rpmtsInitIterator(ts, RPMDBI_LABEL, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("package %s is not installed\n"), arg);
	    res = 1;
	} else {
	    res = rpmcliShowMatches(qva, ts);
	}
	break;
    }
    /*@=branchstate@*/
   
    return res;
}
/*@=bounds@*/

int rpmcliQuery(rpmts ts, QVA_t qva, const char ** argv)
{
    const char * arg;
    rpmVSFlags vsflags, ovsflags;
    int ec = 0;

    if (qva->qva_showPackage == NULL)
	qva->qva_showPackage = showQueryPackage;

    vsflags = rpmExpandNumeric("%{?_vsflags_query}");
    if (qva->qva_flags & VERIFY_DIGEST)
	vsflags |= _RPMVSF_NODIGESTS;
    if (qva->qva_flags & VERIFY_SIGNATURE)
	vsflags |= _RPMVSF_NOSIGNATURES;
    if (qva->qva_flags & VERIFY_HDRCHK)
	vsflags |= RPMVSF_NOHDRCHK;

    ovsflags = rpmtsSetVSFlags(ts, vsflags);
    if (qva->qva_source == RPMQV_ALL) {
	/*@-nullpass@*/ /* FIX: argv can be NULL, cast to pass argv array */
	ec = rpmQueryVerify(qva, ts, (const char *) argv);
	/*@=nullpass@*/
    } else {
/*@-boundsread@*/
	if (argv != NULL)
	while ((arg = *argv++) != NULL) {
	    ec += rpmQueryVerify(qva, ts, arg);
	    rpmtsEmpty(ts);
	}
/*@=boundsread@*/
    }
    vsflags = rpmtsSetVSFlags(ts, ovsflags);

    if (qva->qva_showPackage == showQueryPackage)
	qva->qva_showPackage = NULL;

    return ec;
}
