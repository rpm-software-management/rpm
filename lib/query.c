/** \ingroup rpmcli
 * \file lib/query.c
 * Display tag values from package metadata.
 */

#include "system.h"

#ifndef PATH_MAX
# define PATH_MAX 255
#endif

#include <rpmbuild.h>
#include <rpmurl.h>
#include "manifest.h"
#include "debug.h"

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
	nowtm = *tm;	/* structure assignment */
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
static inline const char * queryHeader(Header h, const char * qfmt)
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
{
    int nlink = 0;

    /* XXX rpm-3.3.12 has not RPMTAG_FILEINODES */
    if (!(fileRdevList && fileInodeList && nfiles > 0))
	return 1;
    while (nfiles-- > 0) {
	if (fileRdevList[nfiles] != fileRdevList[xfile])
	    continue;
	if (fileInodeList[nfiles] != fileInodeList[xfile])
	    continue;
	nlink++;
    }
    return nlink;
}

int showQueryPackage(QVA_t *qva, /*@unused@*/rpmdb rpmdb, Header h)
{
    char * t, * te;
    
    int queryFlags = qva->qva_flags;
    const char *queryFormat = qva->qva_queryFormat;

    int_32 count, type;
    char * prefix = NULL;
    const char ** dirNames = NULL;
    const char ** baseNames = NULL;
    const char ** fileMD5List = NULL;
    const char ** fileOwnerList = NULL;
    const char ** fileGroupList = NULL;
    const char ** fileLinktoList = NULL;
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

    if (!queryFormat && !queryFlags) {
	const char * name, * version, * release;
	headerNVR(h, &name, &version, &release);
	te = stpcpy(te, name);
	te = stpcpy( stpcpy(te, "-"), version);
	te = stpcpy( stpcpy(te, "-"), release);
	goto exit;
    }

    if (queryFormat) {
	const char * str = queryHeader(h, queryFormat);
	nonewline = 1;
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
    }

    if (!(queryFlags & QUERY_FOR_LIST))
	goto exit;

    if (!headerGetEntry(h, RPMTAG_BASENAMES, &type, 
				(void **) &baseNames, &count)) {
	te = stpcpy(te, _("(contains no files)"));
	goto exit;
    }
    if (!headerGetEntry(h, RPMTAG_FILESTATES, &type, 
			 (void **) &fileStatesList, &count)) {
	fileStatesList = NULL;
    }
    headerGetEntry(h, RPMTAG_DIRNAMES, NULL, (void **) &dirNames, NULL);
    headerGetEntry(h, RPMTAG_DIRINDEXES, NULL, (void **) &dirIndexes, NULL);
    headerGetEntry(h, RPMTAG_FILEFLAGS, &type, (void **)&fileFlagsList, &count);
    headerGetEntry(h, RPMTAG_FILESIZES, &type, (void **) &fileSizeList, &count);
    headerGetEntry(h, RPMTAG_FILEMODES, &type, (void **) &fileModeList, &count);
    headerGetEntry(h, RPMTAG_FILEMTIMES, &type, (void **)&fileMTimeList,&count);
    headerGetEntry(h, RPMTAG_FILERDEVS, &type, (void **) &fileRdevList, &count);
    headerGetEntry(h, RPMTAG_FILEINODES, &type, (void **)&fileInodeList,&count);
    headerGetEntry(h, RPMTAG_FILELINKTOS,&type,(void **)&fileLinktoList,&count);
    headerGetEntry(h, RPMTAG_FILEMD5S, &type, (void **) &fileMD5List, &count);

    if (!headerGetEntry(h, RPMTAG_FILEUIDS, &type, 
			 (void **) &fileUIDList, &count)) {
	fileUIDList = NULL;
    } else if (!headerGetEntry(h, RPMTAG_FILEGIDS, &type, 
			     (void **) &fileGIDList, &count)) {
	fileGIDList = NULL;
    }

    if (!headerGetEntry(h, RPMTAG_FILEUSERNAME, &type, 
			 (void **) &fileOwnerList, &count)) {
	fileOwnerList = NULL;
    } else if (!headerGetEntry(h, RPMTAG_FILEGROUPNAME, &type, 
			     (void **) &fileGroupList, &count)) {
	fileGroupList = NULL;
    }

    for (i = 0; i < count; i++) {
	/* If querying only docs, skip non-doc files. */
	if ((queryFlags & QUERY_FOR_DOCS)
	  && !(fileFlagsList[i] & RPMFILE_DOC))
	    continue;
	/* If querying only configs, skip non-config files. */
	if ((queryFlags & QUERY_FOR_CONFIG)
	  && !(fileFlagsList[i] & RPMFILE_CONFIG))
	    continue;

	if (!rpmIsVerbose() && prefix)
	    te = stpcpy(te, prefix);

	if (queryFlags & QUERY_FOR_STATE) {
	    if (fileStatesList) {
		switch (fileStatesList[i]) {
		case RPMFILE_STATE_NORMAL:
		    te = stpcpy(te, _("normal        ")); break;
		case RPMFILE_STATE_REPLACED:
		    te = stpcpy(te, _("replaced      ")); break;
		case RPMFILE_STATE_NOTINSTALLED:
		    te = stpcpy(te, _("not installed ")); break;
		case RPMFILE_STATE_NETSHARED:
		    te = stpcpy(te, _("net shared    ")); break;
		default:
		    sprintf(te, _("(unknown %3d) "), (int)fileStatesList[i]);
		    te += strlen(te);
		    break;
		}
	    } else {
		te = stpcpy(te, _("(no state)    "));
	    }
	}

	if (queryFlags & QUERY_FOR_DUMPFILES) {
	    sprintf(te, "%s%s %d %d %s 0%o ", 
				   dirNames[dirIndexes[i]], baseNames[i],
				   fileSizeList[i], fileMTimeList[i],
				   fileMD5List[i], fileModeList[i]);
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
				 (unsigned)fileRdevList[i]);
	    te += strlen(te);

	    if (strlen(fileLinktoList[i]))
		sprintf(te, "%s", fileLinktoList[i]);
	    else
		sprintf(te, "X");
	    te += strlen(te);
	} else if (!rpmIsVerbose()) {
	    te = stpcpy(te, dirNames[dirIndexes[i]]);
	    te = stpcpy(te, baseNames[i]);
	} else {
	    char * filespec;
	    int nlink;

	    filespec = xmalloc(strlen(dirNames[dirIndexes[i]])
					      + strlen(baseNames[i]) + 1);
	    strcpy(filespec, dirNames[dirIndexes[i]]);
	    strcat(filespec, baseNames[i]);
					
	    nlink = countLinks(fileRdevList, fileInodeList, count, i);
	    if (fileOwnerList && fileGroupList) {
		printFileInfo(te, filespec, fileSizeList[i],
					      fileModeList[i], fileMTimeList[i],
					      fileRdevList[i], nlink,
					      fileOwnerList[i], 
					      fileGroupList[i], -1, 
					      -1, fileLinktoList[i]);
		te += strlen(te);
	    } else if (fileUIDList && fileGIDList) {
		printFileInfo(te, filespec, fileSizeList[i],
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
    dirNames = headerFreeData(dirNames, -1);
    baseNames = headerFreeData(baseNames, -1);
    fileLinktoList = headerFreeData(fileLinktoList, -1);
    fileMD5List = headerFreeData(fileMD5List, -1);
    fileOwnerList = headerFreeData(fileOwnerList, -1);
    fileGroupList = headerFreeData(fileGroupList, -1);
    return rc;
}

/**
 */
static void
printNewSpecfile(Spec spec)
{
    Header h = spec->packages->header;
    struct speclines *sl = spec->sl;
    struct spectags *st = spec->st;
    const char * msgstr = NULL;
    int i, j;

    if (sl == NULL || st == NULL)
	return;

    for (i = 0; i < st->st_ntags; i++) {
	struct spectag * t = st->st_t + i;
	const char * tn = tagName(t->t_tag);
	const char * errstr;
	char fmt[128];

	fmt[0] = '\0';
	(void) stpcpy( stpcpy( stpcpy( fmt, "%{"), tn), "}\n");
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
	    break;
	case RPMTAG_DESCRIPTION:
	    for (j = 1; j < t->t_nlines; j++) {
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
	    break;
	}
    }
    msgstr = _free(msgstr);

    for (i = 0; i < sl->sl_nlines; i++) {
	if (sl->sl_lines[i] == NULL)
	    continue;
	printf("%s", sl->sl_lines[i]);
    }
}

void rpmDisplayQueryTags(FILE * f)
{
    const struct headerTagTableEntry * t;
    int i;
    const struct headerSprintfExtension * ext = rpmHeaderFormats;

    for (i = 0, t = rpmTagTable; i < rpmTagTableSize; i++, t++) {
	fprintf(f, "%s\n", t->name + 7);
    }

    while (ext->name) {
	if (ext->type == HEADER_EXT_MORE) {
	    ext = ext->u.more;
	    continue;
	}
	/* XXX don't print query tags twice. */
	for (i = 0, t = rpmTagTable; i < rpmTagTableSize; i++, t++) {
	    if (!strcmp(t->name, ext->name))
	    	break;
	}
	if (i >= rpmTagTableSize && ext->type == HEADER_EXT_TAG)
	    fprintf(f, "%s\n", ext->name + 7);
	ext++;
    }
}

int showMatches(QVA_t *qva, rpmdbMatchIterator mi, QVF_t showPackage)
{
    Header h;
    int ec = 0;

    while ((h = rpmdbNextIterator(mi)) != NULL) {
	int rc;
	if ((rc = showPackage(qva, rpmdbGetIteratorRpmDB(mi), h)) != 0)
	    ec = rc;
    }
    mi = rpmdbFreeIterator(mi);
    return ec;
}

/**
 * @todo Eliminate linkage loop into librpmbuild.a
 */
int	(*parseSpecVec) (Spec *specp, const char *specFile, const char *rootdir,
		const char *buildRoot, int inBuildArch, const char *passPhrase,
		char *cookie, int anyarch, int force) = NULL;
/**
 * @todo Eliminate linkage loop into librpmbuild.a
 */
void	(*freeSpecVec) (Spec spec) = NULL;

int rpmQueryVerify(QVA_t *qva, rpmQVSources source, const char * arg,
	rpmdb rpmdb, QVF_t showPackage)
{
    rpmdbMatchIterator mi = NULL;
    Header h;
    int rc;
    int isSource;
    int retcode = 0;
    char *end = NULL;

    switch (source) {
    case RPMQV_RPM:
    {	int ac = 0;
	const char ** av = NULL;
	const char * fileURL = NULL;
	rpmRC rpmrc;
	int i;

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
		if (fd) Fclose(fd);
		retcode = 1;
		break;
	    }

	    rpmrc = rpmReadPackageHeader(fd, &h, &isSource, NULL, NULL);
	    Fclose(fd);

	    if (!(rpmrc == RPMRC_OK || rpmrc == RPMRC_BADMAGIC)) {
		rpmError(RPMERR_QUERY, _("query of %s failed\n"), fileURL);
		retcode = 1;
		break;
	    }
	    if (rpmrc == RPMRC_OK && h == NULL) {
		rpmError(RPMERR_QUERY,
			_("old format source packages cannot be queried\n"));
		retcode = 1;
		break;
	    }

	    /* Query a package file. */
	    if (rpmrc == RPMRC_OK) {
		retcode = showPackage(qva, rpmdb, h);
		headerFree(h);
		continue;
	    }

	    /* Try to read a package manifest. */
	    fd = Fopen(fileURL, "r.fpio");
	    if (fd == NULL || Ferror(fd)) {
		rpmError(RPMERR_OPEN, _("open of %s failed: %s\n"), fileURL,
			Fstrerror(fd));
		if (fd) Fclose(fd);
		retcode = 1;
		break;
	    }
	    
	    /* Read list of packages from manifest. */
	    retcode = rpmReadPackageManifest(fd, &ac, &av);
	    if (retcode) {
		rpmError(RPMERR_MANIFEST, _("%s: read manifest failed: %s\n"),
			fileURL, Fstrerror(fd));
		retcode = 1;
	    }
	    Fclose(fd);

	    /* If successful, restart the query loop. */
	    if (retcode == 0)
		goto restart;

	    break;
	}

	fileURL = _free(fileURL);
	if (av) {
	    for (i = 0; i < ac; i++)
		av[i] = _free(av[i]);
	    av = _free(av);
	}
    }	break;

    case RPMQV_SPECFILE:
	if (showPackage != showQueryPackage)
	    return 1;

	/* XXX Eliminate linkage dependency loop */
	if (parseSpecVec == NULL || freeSpecVec == NULL)
	    return 1;

      { Spec spec = NULL;
	Package pkg;
	char * buildRoot = NULL;
	int inBuildArch = 0;
	char * passPhrase = "";
	char *cookie = NULL;
	int anyarch = 1;
	int force = 1;

	rc = parseSpecVec(&spec, arg, "/", buildRoot, inBuildArch, passPhrase,
		cookie, anyarch, force);
	if (rc || spec == NULL) {
	    
	    rpmError(RPMERR_QUERY,
	    		_("query of specfile %s failed, can't parse\n"), arg);
	    if (spec != NULL) freeSpecVec(spec);
	    retcode = 1;
	    break;
	}

	if (specedit) {
	    printNewSpecfile(spec);
	    freeSpecVec(spec);
	    retcode = 0;
	    break;
	}

	for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	    showPackage(qva, NULL, pkg->header);
	}
	freeSpecVec(spec);
      }	break;

    case RPMQV_ALL:
	/* RPMDBI_PACKAGES */
	mi = rpmdbInitIterator(rpmdb, RPMDBI_PACKAGES, NULL, 0);
	if (mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("no packages\n"));
	    retcode = 1;
	} else {
	    retcode = showMatches(qva, mi, showPackage);
	}
	break;

    case RPMQV_GROUP:
	mi = rpmdbInitIterator(rpmdb, RPMTAG_GROUP, arg, 0);
	if (mi == NULL) {
	    rpmError(RPMERR_QUERYINFO,
		_("group %s does not contain any packages\n"), arg);
	    retcode = 1;
	} else {
	    retcode = showMatches(qva, mi, showPackage);
	}
	break;

    case RPMQV_TRIGGEREDBY:
	mi = rpmdbInitIterator(rpmdb, RPMTAG_TRIGGERNAME, arg, 0);
	if (mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("no package triggers %s\n"), arg);
	    retcode = 1;
	} else {
	    retcode = showMatches(qva, mi, showPackage);
	}
	break;

    case RPMQV_WHATREQUIRES:
	mi = rpmdbInitIterator(rpmdb, RPMTAG_REQUIRENAME, arg, 0);
	if (mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("no package requires %s\n"), arg);
	    retcode = 1;
	} else {
	    retcode = showMatches(qva, mi, showPackage);
	}
	break;

    case RPMQV_WHATPROVIDES:
	if (arg[0] != '/') {
	    mi = rpmdbInitIterator(rpmdb, RPMTAG_PROVIDENAME, arg, 0);
	    if (mi == NULL) {
		rpmError(RPMERR_QUERYINFO, _("no package provides %s\n"), arg);
		retcode = 1;
	    } else {
		retcode = showMatches(qva, mi, showPackage);
	    }
	    break;
	}
	/*@fallthrough@*/
    case RPMQV_PATH:
    {   const char * s;
	char * fn;

	for (s = arg; *s != '\0'; s++)
	    if (!(*s == '.' || *s == '/')) break;

	if (*s == '\0') {
	    char fnbuf[PATH_MAX];
	    fn = /*@-unrecog@*/ realpath(arg, fnbuf) /*@=unrecog@*/;
	    fn = xstrdup( (fn ? fn : arg) );
	} else
	    fn = xstrdup(arg);
	(void) rpmCleanPath(fn);

	mi = rpmdbInitIterator(rpmdb, RPMTAG_BASENAMES, fn, 0);
	if (mi == NULL) {
	    int myerrno = 0;
	    if (access(fn, F_OK) != 0)
		myerrno = errno;
	    switch (myerrno) {
	    default:
		rpmError(RPMERR_QUERY,
			_("file %s: %s\n"), fn, strerror(myerrno));
		break;
	    case 0:
		rpmError(RPMERR_QUERYINFO,
			_("file %s is not owned by any package\n"), fn);
		break;
	    }
	    retcode = 1;
	} else {
	    retcode = showMatches(qva, mi, showPackage);
	}
	fn = _free(fn);
    }	break;

    case RPMQV_DBOFFSET:
    {	int mybase = 10;
	const char * myarg = arg;
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
	    rpmError(RPMERR_QUERY, _("invalid package number: %s\n"), arg);
	    return 1;
	}
	rpmMessage(RPMMESS_DEBUG, _("package record number: %u\n"), recOffset);
	/* RPMDBI_PACKAGES */
	mi = rpmdbInitIterator(rpmdb, RPMDBI_PACKAGES, &recOffset, sizeof(recOffset));
	if (mi == NULL) {
	    rpmError(RPMERR_QUERY,
		_("record %u could not be read\n"), recOffset);
	    retcode = 1;
	} else {
	    retcode = showMatches(qva, mi, showPackage);
	}
    }	break;

    case RPMQV_PACKAGE:
	/* XXX HACK to get rpmdbFindByLabel out of the API */
	mi = rpmdbInitIterator(rpmdb, RPMDBI_LABEL, arg, 0);
	if (mi == NULL) {
	    rpmError(RPMERR_QUERYINFO, _("package %s is not installed\n"), arg);
	    retcode = 1;
	} else {
	    retcode = showMatches(qva, mi, showPackage);
	}
	break;
    }
   
    return retcode;
}

int rpmQuery(QVA_t *qva, rpmQVSources source, const char * arg)
{
    rpmdb rpmdb = NULL;
    int rc;

    switch (source) {
    case RPMQV_RPM:
    case RPMQV_SPECFILE:
	break;
    default:
	if (rpmdbOpen(qva->qva_prefix, &rpmdb, O_RDONLY, 0644))
	    return 1;
	break;
    }

    rc = rpmQueryVerify(qva, source, arg, rpmdb, showQueryPackage);

    if (rpmdb != NULL)
	rpmdbClose(rpmdb);

    return rc;
}
