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

#include "rpmio_internal.h"
#include <rpmcli.h>
#include <rpmbuild.h>

#include "depends.h"
#include "manifest.h"

#include "debug.h"

/*@access rpmTransactionSet@*/		/* XXX ts->rpmdb */
/*@access rpmdbMatchIterator@*/		/* XXX compared with NULL */
/*@access Header@*/			/* XXX compared with NULL */
/*@access rpmdb@*/			/* XXX compared with NULL */
/*@access FD_t@*/			/* XXX compared with NULL */

/**
 */
static void printFileInfo(char * te, const char * name,
			  unsigned int size, unsigned short mode,
			  unsigned int mtime,
			  unsigned short rdev, unsigned int nlink,
			  const char * owner, const char * group,
			  int uid, int gid, const char * linkto)
	/*@modifies *te @*/
{
    char sizefield[15];
    char ownerfield[9], groupfield[9];
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
	if (tm) nowtm = *tm;	/* structure assignment */
    }

    if (owner) 
	strncpy(ownerfield, owner, 8);
    else
	sprintf(ownerfield, "%-8d", uid);
    ownerfield[8] = '\0';

    if (group) 
	strncpy(groupfield, group, 8);
    else 
	sprintf(groupfield, "%-8d", gid);
    groupfield[8] = '\0';

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
    const char * errstr;
    const char * str;

    str = headerSprintf(h, qfmt, rpmTagTable, rpmHeaderFormats, &errstr);
    if (str == NULL)
	rpmError(RPMERR_QFMT, _("incorrect format: %s\n"), errstr);
    return str;
}

/**
 */
static int countLinks(int_16 * fileRdevList, int_32 * fileInodeList, int nfiles,
		int xfile)
	/*@*/
{
    int nlink = 0;

    /* XXX rpm-3.3.12 has not RPMTAG_FILEINODES */
    if (!(fileRdevList[xfile] != 0 && fileRdevList &&
		fileInodeList[xfile] != 0 && fileInodeList && nfiles > 0))
	return 1;
    while (nfiles-- > 0) {
	if (fileRdevList[nfiles] == 0)
	    continue;
	if (fileRdevList[nfiles] != fileRdevList[xfile])
	    continue;
	if (fileInodeList[nfiles] == 0)
	    continue;
	if (fileInodeList[nfiles] != fileInodeList[xfile])
	    continue;
	nlink++;
    }
    if (nlink == 0) nlink = 1;
    return nlink;
}

int showQueryPackage(QVA_t qva, /*@unused@*/ rpmTransactionSet ts, Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    char * t, * te;
    rpmQueryFlags queryFlags = qva->qva_flags;
    const char * queryFormat = qva->qva_queryFormat;
    rpmTagType type;
    int_32 count;
    char * prefix = NULL;
    const char ** dirNames = NULL;
    const char ** baseNames = NULL;
    rpmTagType bnt, dnt;
    const char ** fileMD5List = NULL;
    const char ** fileOwnerList = NULL;
    const char ** fileGroupList = NULL;
    const char ** fileLinktoList = NULL;
    rpmTagType m5t, fot, fgt, ltt;
    const char * fileStatesList;
    int_32 * fileFlagsList, * fileMTimeList, * fileSizeList;
    int_32 * fileUIDList = NULL;
    int_32 * fileGIDList = NULL;
    int_32 * fileInodeList = NULL;
    uint_16 * fileModeList;
    uint_16 * fileRdevList;
    int_32 * dirIndexes;
    int rc = 0;		/* XXX FIXME: need real return code */
    int nonewline = 0;
    int i;

    te = t = xmalloc(BUFSIZ);
    *te = '\0';

    if (queryFormat == NULL && queryFlags == QUERY_FOR_DEFAULT) {
	const char * name, * version, * release;
	(void) headerNVR(h, &name, &version, &release);
	te = stpcpy(te, name);
	te = stpcpy( stpcpy(te, "-"), version);
	te = stpcpy( stpcpy(te, "-"), release);
	goto exit;
    }

    if (queryFormat) {
	const char * str = queryHeader(h, queryFormat);
	nonewline = 1;
	/*@-branchstate@*/
	if (str) {
	    size_t tb = (te - t);
	    size_t sb = strlen(str);

	    if (sb >= (BUFSIZ - tb)) {
		t = xrealloc(t, BUFSIZ+sb);
		te = t + tb;
	    }
	    /*@-usereleased@*/
	    te = stpcpy(te, str);
	    /*@=usereleased@*/
	    str = _free(str);
	}
	/*@=branchstate@*/
    }

    if (!(queryFlags & QUERY_FOR_LIST))
	goto exit;

    if (!hge(h, RPMTAG_BASENAMES, &bnt, (void **) &baseNames, &count)) {
	te = stpcpy(te, _("(contains no files)"));
	goto exit;
    }
    if (!hge(h, RPMTAG_FILESTATES, &type, (void **) &fileStatesList, NULL))
	fileStatesList = NULL;
    if (!hge(h, RPMTAG_DIRNAMES, &dnt, (void **) &dirNames, NULL))
	dirNames = NULL;
    if (!hge(h, RPMTAG_DIRINDEXES, NULL, (void **) &dirIndexes, NULL))
	dirIndexes = NULL;
    if (!hge(h, RPMTAG_FILEFLAGS, &type, (void **) &fileFlagsList, NULL))
	fileFlagsList = NULL;
    if (!hge(h, RPMTAG_FILESIZES, &type, (void **) &fileSizeList, NULL))
	fileSizeList = NULL;
    if (!hge(h, RPMTAG_FILEMODES, &type, (void **) &fileModeList, NULL))
	fileModeList = NULL;
    if (!hge(h, RPMTAG_FILEMTIMES, &type, (void **) &fileMTimeList, NULL))
	fileMTimeList = NULL;
    if (!hge(h, RPMTAG_FILERDEVS, &type, (void **) &fileRdevList, NULL))
	fileRdevList = NULL;
    if (!hge(h, RPMTAG_FILEINODES, &type, (void **) &fileInodeList, NULL))
	fileInodeList = NULL;
    if (!hge(h, RPMTAG_FILELINKTOS, &ltt, (void **) &fileLinktoList, NULL))
	fileLinktoList = NULL;
    if (!hge(h, RPMTAG_FILEMD5S, &m5t, (void **) &fileMD5List, NULL))
	fileMD5List = NULL;
    if (!hge(h, RPMTAG_FILEUIDS, &type, (void **) &fileUIDList, NULL))
	fileUIDList = NULL;
    if (!hge(h, RPMTAG_FILEGIDS, &type, (void **) &fileGIDList, NULL))
	fileGIDList = NULL;
    if (!hge(h, RPMTAG_FILEUSERNAME, &fot, (void **) &fileOwnerList, NULL))
	fileOwnerList = NULL;
    if (!hge(h, RPMTAG_FILEGROUPNAME, &fgt, (void **) &fileGroupList, NULL))
	fileGroupList = NULL;

    for (i = 0; i < count; i++) {

	/* If querying only docs, skip non-doc files. */
	if ((queryFlags & QUERY_FOR_DOCS)
	  && !(fileFlagsList[i] & RPMFILE_DOC))
	    continue;

	/* If querying only configs, skip non-config files. */
	if ((queryFlags & QUERY_FOR_CONFIG)
	  && !(fileFlagsList[i] & RPMFILE_CONFIG))
	    continue;

	/* If not querying %ghost, skip ghost files. */
	if (!(qva->qva_fflags & RPMFILE_GHOST)
	  && (fileFlagsList[i] & RPMFILE_GHOST))
	    continue;

	/*@-internalglobs@*/ /* FIX: shrug */
	if (!rpmIsVerbose() && prefix)
	    te = stpcpy(te, prefix);
	/*@=internalglobs@*/

	if (queryFlags & QUERY_FOR_STATE) {
	    if (fileStatesList) {
		rpmfileState fstate = fileStatesList[i];
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
		default:
		    sprintf(te, _("(unknown %3d) "), (int)fileStatesList[i]);
		    te += strlen(te);
		    /*@switchbreak@*/ break;
		}
	    } else {
		te = stpcpy(te, _("(no state)    "));
	    }
	}

	if (queryFlags & QUERY_FOR_DUMPFILES) {
	    sprintf(te, "%s%s %d %d %s 0%o ", 
				   dirNames[dirIndexes[i]], baseNames[i],
				   fileSizeList[i], fileMTimeList[i],
				   fileMD5List[i], (unsigned) fileModeList[i]);
	    te += strlen(te);

	    if (fileOwnerList && fileGroupList) {
		sprintf(te, "%s %s", fileOwnerList[i], fileGroupList[i]);
		te += strlen(te);
	    } else if (fileUIDList && fileGIDList) {
		sprintf(te, "%d %d", fileUIDList[i], fileGIDList[i]);
		te += strlen(te);
	    } else {
		rpmError(RPMERR_INTERNAL,
			_("package has neither file owner or id lists\n"));
	    }

	    sprintf(te, " %s %s %u ", 
				 fileFlagsList[i] & RPMFILE_CONFIG ? "1" : "0",
				 fileFlagsList[i] & RPMFILE_DOC ? "1" : "0",
				 (unsigned) fileRdevList[i]);
	    te += strlen(te);

	    if (strlen(fileLinktoList[i]))
		sprintf(te, "%s", fileLinktoList[i]);
	    else
		sprintf(te, "X");
	    te += strlen(te);
	} else
	/*@-internalglobs@*/ /* FIX: shrug */
	if (!rpmIsVerbose()) {
	    te = stpcpy(te, dirNames[dirIndexes[i]]);
	    te = stpcpy(te, baseNames[i]);
	}
	/*@=internalglobs@*/
	else {
	    char * filespec;
	    int nlink;
	    size_t fileSize;

	    filespec = xmalloc(strlen(dirNames[dirIndexes[i]])
					      + strlen(baseNames[i]) + 1);
	    strcpy(filespec, dirNames[dirIndexes[i]]);
	    strcat(filespec, baseNames[i]);
					
	    fileSize = fileSizeList[i];
	    nlink = countLinks(fileRdevList, fileInodeList, count, i);

if (S_ISDIR(fileModeList[i])) {
    nlink++;
    fileSize = 0;
}
	    if (fileOwnerList && fileGroupList) {
		printFileInfo(te, filespec, fileSize,
					      fileModeList[i], fileMTimeList[i],
					      fileRdevList[i], nlink,
					      fileOwnerList[i], 
					      fileGroupList[i], -1, 
					      -1, fileLinktoList[i]);
		te += strlen(te);
	    } else if (fileUIDList && fileGIDList) {
		printFileInfo(te, filespec, fileSize,
					      fileModeList[i], fileMTimeList[i],
					      fileRdevList[i], nlink,
					      NULL, NULL, fileUIDList[i], 
					      fileGIDList[i], 
					      fileLinktoList[i]);
		te += strlen(te);
	    } else {
		rpmError(RPMERR_INTERNAL,
			_("package has neither file owner or id lists\n"));
	    }

	    filespec = _free(filespec);
	}
	if (te > t) {
	    *te++ = '\n';
	    *te = '\0';
	    rpmMessage(RPMMESS_NORMAL, "%s", t);
	    te = t;
	    *t = '\0';
	}
    }
	    
    rc = 0;

exit:
    if (te > t) {
	if (!nonewline) {
	    *te++ = '\n';
	    *te = '\0';
	}
	rpmMessage(RPMMESS_NORMAL, "%s", t);
    }
    t = _free(t);
    dirNames = hfd(dirNames, dnt);
    baseNames = hfd(baseNames, bnt);
    fileLinktoList = hfd(fileLinktoList, ltt);
    fileMD5List = hfd(fileMD5List, m5t);
    fileOwnerList = hfd(fileOwnerList, fot);
    fileGroupList = hfd(fileGroupList, fgt);
    return rc;
}

/**
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

	    strcpy(fmt, t->t_msgid);
	    for (fe = fmt; *fe && *fe != '('; fe++)
		{} ;
	    if (*fe == '(') *fe = '\0';
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
	(void) stpcpy( stpcpy( stpcpy( fmt, "%{"), tn), "}");
	msgstr = _free(msgstr);

	msgstr = headerSprintf(h, fmt, rpmTagTable, rpmHeaderFormats, &errstr);
	if (msgstr == NULL) {
	    rpmError(RPMERR_QFMT, _("can't query %s: %s\n"), tn, errstr);
	    return;
	}

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
    }
    /*@=branchstate@*/
    msgstr = _free(msgstr);

    for (i = 0; i < sl->sl_nlines; i++) {
	const char * s = sl->sl_lines[i];
	if (s == NULL)
	    continue;
	printf("%s", s);
	if (strchr(s, '\n') == NULL && s[strlen(s)-1] != '\n')
	    printf("\n");
    }
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

int showMatches(QVA_t qva, rpmTransactionSet ts)
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

int rpmQueryVerify(QVA_t qva, rpmTransactionSet ts, const char * arg)
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

	    ts->verify_legacy = 1;
	    /*@=mustmod@*/	/* LCL: something fishy here, was segfault */
    	    rpmrc = rpmReadPackageFile(ts, fd, fileURL, &h);
	    /*@=mustmod@*/
	    ts->verify_legacy = 0;

	    (void) Fclose(fd);

	    if (!(rpmrc == RPMRC_OK || rpmrc == RPMRC_BADMAGIC)) {
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
		h = headerFree(h, "QueryVerify");
		rpmtransClean(ts);
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
	    if (res) {
		rpmError(RPMERR_MANIFEST, _("%s: read manifest failed: %s\n"),
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
		res = showMatches(qva, ts);
	}
	break;

    case RPMQV_GROUP:
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_GROUP, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmError(RPMERR_QUERYINFO,
		_("group %s does not contain any packages\n"), arg);
	    res = 1;
	} else {
	    res = showMatches(qva, ts);
	}
	break;

    case RPMQV_TRIGGEREDBY:
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_TRIGGERNAME, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("no package triggers %s\n"), arg);
	    res = 1;
	} else {
	    res = showMatches(qva, ts);
	}
	break;

    case RPMQV_PKGID:
    {	unsigned char md5[16];
	unsigned char * t;

	for (i = 0, s = arg; *s && isxdigit(*s); s++, i++)
	    {};
	if (i != 32) {
	    rpmError(RPMERR_QUERYINFO, _("malformed %s: %s\n"), "pkgid", arg);
	    return 1;
	}

        for (i = 0, t = md5, s = arg; i < 16; i++, t++, s += 2)
            *t = (nibble(s[0]) << 4) | nibble(s[1]);
	
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_SIGMD5, md5, 16);
	if (qva->qva_mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("no package matches %s: %s\n"),
			"pkgid", arg);
	    res = 1;
	} else {
	    res = showMatches(qva, ts);
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
	    res = showMatches(qva, ts);
	}
	break;

    case RPMQV_FILEID:
	for (i = 0, s = arg; *s && isxdigit(*s); s++, i++)
	    {};
	if (i != 32) {
	    rpmError(RPMERR_QUERY, _("malformed %s: %s\n"), "fileid", arg);
	    return 1;
	}

	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_FILEMD5S, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("no package matches %s: %s\n"),
			"fileid", arg);
	    res = 1;
	} else {
	    res = showMatches(qva, ts);
	}
	break;

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
	    res = showMatches(qva, ts);
	}
    }	break;

    case RPMQV_WHATREQUIRES:
	qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_REQUIRENAME, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("no package requires %s\n"), arg);
	    res = 1;
	} else {
	    res = showMatches(qva, ts);
	}
	break;

    case RPMQV_WHATPROVIDES:
	if (arg[0] != '/') {
	    qva->qva_mi = rpmtsInitIterator(ts, RPMTAG_PROVIDENAME, arg, 0);
	    if (qva->qva_mi == NULL) {
		rpmError(RPMERR_QUERYINFO, _("no package provides %s\n"), arg);
		res = 1;
	    } else {
		res = showMatches(qva, ts);
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
	    res = showMatches(qva, ts);
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
	    res = showMatches(qva, ts);
	}
    }	break;

    case RPMQV_PACKAGE:
	/* XXX HACK to get rpmdbFindByLabel out of the API */
	qva->qva_mi = rpmtsInitIterator(ts, RPMDBI_LABEL, arg, 0);
	if (qva->qva_mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("package %s is not installed\n"), arg);
	    res = 1;
	} else {
	    res = showMatches(qva, ts);
	}
	break;
    }
    /*@=branchstate@*/
   
    return res;
}

int rpmcliQuery(rpmTransactionSet ts, QVA_t qva, const char ** argv)
{
    const char * arg;
    int ec = 0;

    if (qva->qva_showPackage == NULL)
	qva->qva_showPackage = showQueryPackage;

    switch (qva->qva_source) {
    case RPMQV_RPM:
    case RPMQV_SPECFILE:
	break;
    default:
	if (rpmtsOpenDB(ts, O_RDONLY))
	    return 1;	/* XXX W2DO? */
	break;
    }

    if (qva->qva_source == RPMQV_ALL) {
	/*@-nullpass@*/ /* FIX: argv can be NULL, cast to pass argv array */
	ec = rpmQueryVerify(qva, ts, (const char *) argv);
	/*@=nullpass@*/
    } else {
	if (argv != NULL)
	while ((arg = *argv++) != NULL) {
	    ec += rpmQueryVerify(qva, ts, arg);
	    rpmtransClean(ts);
	}
    }

    if (qva->qva_showPackage == showQueryPackage)
	qva->qva_showPackage = NULL;

    return ec;
}
