/**
 * \file lib/misc.c
 */

#include "system.h"

static int _debug = 0;

#include <rpmlib.h>
#include <rpmurl.h>
#include <rpmmacro.h>	/* XXX for rpmGetPath */

#include "misc.h"
#include "debug.h"

/*@access Header@*/		/* XXX compared with NULL */
/*@access FD_t@*/		/* XXX compared with NULL */

/*@-exportheadervar@*/
/* just to put a marker in librpm.a */
/*@unused@*/ /*@observer@*/ char * RPMVERSION = VERSION;
/*@=exportheadervar@*/

char ** splitString(const char * str, int length, char sep)
{
    const char * source;
    char * s, * dest;
    char ** list;
    int i;
    int fields;

    s = xmalloc(length + 1);

    fields = 1;
    for (source = str, dest = s, i = 0; i < length; i++, source++, dest++) {
	*dest = *source;
	if (*dest == sep) fields++;
    }

    *dest = '\0';

    list = xmalloc(sizeof(char *) * (fields + 1));

    dest = s;
    list[0] = dest;
    i = 1;
    while (i < fields) {
	if (*dest == sep) {
	    list[i++] = dest + 1;
	    *dest = 0;
	}
	dest++;
    }

    list[i] = NULL;

    return list;
}

void freeSplitString(char ** list)
{
    /*@-unqualifiedtrans@*/
    list[0] = _free(list[0]);
    /*@=unqualifiedtrans@*/
    list = _free(list);
}

int rpmfileexists(const char * urlfn)
{
    const char *fn;
    int urltype = urlPath(urlfn, &fn);
    struct stat buf;

    if (*fn == '\0') fn = "/";
    switch (urltype) {
    case URL_IS_FTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_HTTP:	/* XXX WRONG WRONG WRONG */
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	if (Stat(fn, &buf)) {
	    switch(errno) {
	    case ENOENT:
	    case EINVAL:
		return 0;
	    }
	}
	break;
    case URL_IS_DASH:
    default:
	return 0;
	/*@notreached@*/ break;
    }

    return 1;
}

int doputenv(const char *str)
{
    char * a;

    /* FIXME: this leaks memory! */
    a = xmalloc(strlen(str) + 1);
    strcpy(a, str);
    return putenv(a);
}

int dosetenv(const char * name, const char * value, int overwrite)
{
    char * a;

    if (!overwrite && getenv(name)) return 0;

    /* FIXME: this leaks memory! */
    a = xmalloc(strlen(name) + strlen(value) + sizeof("="));
    (void) stpcpy( stpcpy( stpcpy( a, name), "="), value);
    return putenv(a);
}

static int rpmMkpath(const char * path, mode_t mode, uid_t uid, gid_t gid)
{
    char * d, * de;
    int created = 0;
    int rc;

    if (path == NULL)
	return -1;
    d = alloca(strlen(path)+2);
    de = stpcpy(d, path);
    de[1] = '\0';
    for (de = d; *de != '\0'; de++) {
	struct stat st;
	char savec;

	while (*de && *de != '/') de++;
	savec = de[1];
	de[1] = '\0';

	rc = stat(d, &st);
	if (rc) {
	    switch(errno) {
	    default:
		return errno;
		/*@notreached@*/ break;
	    case ENOENT:
		break;
	    }
	    rc = mkdir(d, mode);
	    if (rc)
		return errno;
	    created = 1;
	    if (!(uid == (uid_t) -1 && gid == (gid_t) -1)) {
		rc = chown(d, uid, gid);
		if (rc)
		    return errno;
	    }
	} else if (!S_ISDIR(st.st_mode)) {
	    return ENOTDIR;
	}
	de[1] = savec;
    }
    rc = 0;
    if (created)
	rpmMessage(RPMMESS_WARNING, "created %%_tmppath directory %s\n", path);
    return rc;
}

int makeTempFile(const char * prefix, const char ** fnptr, FD_t * fdptr)
{
    const char * tpmacro = "%{?_tmppath:%{_tmppath}}%{!?_tmppath:/var/tmp}";
    const char * tempfn = NULL;
    const char * tfn = NULL;
    static int _initialized = 0;
    int temput;
    FD_t fd = NULL;
    int ran;

    if (!prefix) prefix = "";

    /* Create the temp directory if it doesn't already exist. */
    if (!_initialized) {
	_initialized = 1;
	tempfn = rpmGenPath(prefix, tpmacro, NULL);
	if (rpmMkpath(tempfn, 0755, (uid_t) -1, (gid_t) -1))
	    goto errxit;
    }

    /* XXX should probably use mkstemp here */
    srand(time(NULL));
    ran = rand() % 100000;

    /* maybe this should use link/stat? */

    do {
	char tfnbuf[64];
#ifndef	NOTYET
	sprintf(tfnbuf, "rpm-tmp.%d", ran++);
	tempfn = _free(tempfn);
	tempfn = rpmGenPath(prefix, tpmacro, tfnbuf);
#else
	strcpy(tfnbuf, "rpm-tmp.XXXXXX");
	tempfn = _free(tempfn);
	tempfn = rpmGenPath(prefix, tpmacro, mktemp(tfnbuf));
#endif

	temput = urlPath(tempfn, &tfn);
	if (*tfn == '\0') goto errxit;

	switch (temput) {
	case URL_IS_HTTP:
	case URL_IS_DASH:
	    goto errxit;
	    /*@notreached@*/ break;
	default:
	    break;
	}

	fd = Fopen(tempfn, "w+x.ufdio");
	/* XXX FIXME: errno may not be correct for ufdio */
    } while ((fd == NULL || Ferror(fd)) && errno == EEXIST);

    if (fd == NULL || Ferror(fd))
	goto errxit;

    switch(temput) {
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
      {	struct stat sb, sb2;
	if (!stat(tfn, &sb) && S_ISLNK(sb.st_mode)) {
	    rpmError(RPMERR_SCRIPT, _("error creating temporary file %s\n"), tfn);
	    goto errxit;
	}

	if (sb.st_nlink != 1) {
	    rpmError(RPMERR_SCRIPT, _("error creating temporary file %s\n"), tfn);
	    goto errxit;
	}

	if (fstat(Fileno(fd), &sb2) == 0) {
	    if (sb2.st_ino != sb.st_ino || sb2.st_dev != sb.st_dev) {
		rpmError(RPMERR_SCRIPT, _("error creating temporary file %s\n"), tfn);
		goto errxit;
	    }
	}
      }	break;
    default:
	break;
    }

    if (fnptr)
	*fnptr = tempfn;
    else 
	tempfn = _free(tempfn);
    *fdptr = fd;

    return 0;

errxit:
    tempfn = _free(tempfn);
    /*@-usereleased@*/
    if (fd) (void) Fclose(fd);
    /*@=usereleased@*/
    return 1;
}

char * currentDirectory(void)
{
    int currDirLen;
    char * currDir;

    currDirLen = 50;
    currDir = xmalloc(currDirLen);
    while (!getcwd(currDir, currDirLen) && errno == ERANGE) {
	currDirLen += 50;
	currDir = xrealloc(currDir, currDirLen);
    }

    return currDir;
}

int _noDirTokens = 0;

static int dncmp(const void * a, const void * b)
{
    const char *const * first = a;
    const char *const * second = b;
    return strcmp(*first, *second);
}

void compressFilelist(Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HAE_t hae = (HAE_t)headerAddEntry;
    HRE_t hre = (HRE_t)headerRemoveEntry;
    HFD_t hfd = headerFreeData;
    char ** fileNames;
    const char ** dirNames;
    const char ** baseNames;
    int_32 * dirIndexes;
    rpmTagType fnt;
    int count;
    int i;
    int dirIndex = -1;

    /*
     * This assumes the file list is already sorted, and begins with a
     * single '/'. That assumption isn't critical, but it makes things go
     * a bit faster.
     */

    if (headerIsEntry(h, RPMTAG_DIRNAMES)) {
	(void) hre(h, RPMTAG_OLDFILENAMES);
	return;		/* Already converted. */
    }

    if (!hge(h, RPMTAG_OLDFILENAMES, &fnt, (void **) &fileNames, &count))
	return;		/* no file list */
    if (fileNames == NULL || count <= 0)
	return;

    dirNames = alloca(sizeof(*dirNames) * count);	/* worst case */
    baseNames = alloca(sizeof(*dirNames) * count);
    dirIndexes = alloca(sizeof(*dirIndexes) * count);

    if (fileNames[0][0] != '/') {
	/* HACK. Source RPM, so just do things differently */
	dirIndex = 0;
	dirNames[dirIndex] = "";
	for (i = 0; i < count; i++) {
	    dirIndexes[i] = dirIndex;
	    baseNames[i] = fileNames[i];
	}
	goto exit;
    }

    for (i = 0; i < count; i++) {
	const char ** needle;
	char savechar;
	char * baseName;
	int len;

	if (fileNames[i] == NULL)	/* XXX can't happen */
	    continue;
	baseName = strrchr(fileNames[i], '/') + 1;
	len = baseName - fileNames[i];
	needle = dirNames;
	savechar = *baseName;
	*baseName = '\0';
	if (dirIndex < 0 ||
	    (needle = bsearch(&fileNames[i], dirNames, dirIndex + 1, sizeof(dirNames[0]), dncmp)) == NULL) {
	    char *s = alloca(len + 1);
	    memcpy(s, fileNames[i], len + 1);
	    s[len] = '\0';
	    dirIndexes[i] = ++dirIndex;
	    dirNames[dirIndex] = s;
	} else
	    dirIndexes[i] = needle - dirNames;

	*baseName = savechar;
	baseNames[i] = baseName;
    }

exit:
    if (count > 0) {
	(void) hae(h, RPMTAG_DIRINDEXES, RPM_INT32_TYPE, dirIndexes, count);
	(void) hae(h, RPMTAG_BASENAMES, RPM_STRING_ARRAY_TYPE,
			baseNames, count);
	(void) hae(h, RPMTAG_DIRNAMES, RPM_STRING_ARRAY_TYPE,
			dirNames, dirIndex + 1);
    }

    fileNames = hfd(fileNames, fnt);

    (void) hre(h, RPMTAG_OLDFILENAMES);
}

/*
 * This is pretty straight-forward. The only thing that even resembles a trick
 * is getting all of this into a single xmalloc'd block.
 */
static void doBuildFileList(Header h, /*@out@*/ const char *** fileListPtr,
			    /*@out@*/ int * fileCountPtr, rpmTag baseNameTag,
			    rpmTag dirNameTag, rpmTag dirIndexesTag)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    const char ** baseNames;
    const char ** dirNames;
    int * dirIndexes;
    int count;
    const char ** fileNames;
    int size;
    rpmTagType bnt, dnt;
    char * data;
    int i;

    if (!hge(h, baseNameTag, &bnt, (void **) &baseNames, &count)) {
	if (fileListPtr) *fileListPtr = NULL;
	if (fileCountPtr) *fileCountPtr = 0;
	return;		/* no file list */
    }

    (void) hge(h, dirNameTag, &dnt, (void **) &dirNames, NULL);
    (void) hge(h, dirIndexesTag, NULL, (void **) &dirIndexes, &count);

    size = sizeof(*fileNames) * count;
    for (i = 0; i < count; i++)
	size += strlen(baseNames[i]) + strlen(dirNames[dirIndexes[i]]) + 1;

    fileNames = xmalloc(size);
    data = ((char *) fileNames) + (sizeof(*fileNames) * count);
    for (i = 0; i < count; i++) {
	fileNames[i] = data;
	data = stpcpy( stpcpy(data, dirNames[dirIndexes[i]]), baseNames[i]);
	*data++ = '\0';
    }
    baseNames = hfd(baseNames, bnt);
    dirNames = hfd(dirNames, dnt);

    if (fileListPtr)
	*fileListPtr = fileNames;
    else
	fileNames = _free(fileNames);
    if (fileCountPtr) *fileCountPtr = count;
}

void expandFilelist(Header h)
{
    HAE_t hae = (HAE_t)headerAddEntry;
    HRE_t hre = (HRE_t)headerRemoveEntry;
    const char ** fileNames = NULL;
    int count = 0;

    if (!headerIsEntry(h, RPMTAG_OLDFILENAMES)) {
	doBuildFileList(h, &fileNames, &count, RPMTAG_BASENAMES,
			RPMTAG_DIRNAMES, RPMTAG_DIRINDEXES);
	if (fileNames == NULL || count <= 0)
	    return;
	(void) hae(h, RPMTAG_OLDFILENAMES, RPM_STRING_ARRAY_TYPE,
			fileNames, count);
	fileNames = _free(fileNames);
    }

    (void) hre(h, RPMTAG_DIRNAMES);
    (void) hre(h, RPMTAG_BASENAMES);
    (void) hre(h, RPMTAG_DIRINDEXES);
}


void rpmBuildFileList(Header h, const char *** fileListPtr, int * fileCountPtr)
{
    doBuildFileList(h, fileListPtr, fileCountPtr, RPMTAG_BASENAMES,
			RPMTAG_DIRNAMES, RPMTAG_DIRINDEXES);
}

void buildOrigFileList(Header h, const char *** fileListPtr, int * fileCountPtr)
{
    doBuildFileList(h, fileListPtr, fileCountPtr, RPMTAG_ORIGBASENAMES,
			RPMTAG_ORIGDIRNAMES, RPMTAG_ORIGDIRINDEXES);
}

/* glob_pattern_p() taken from bash
 * Copyright (C) 1985, 1988, 1989 Free Software Foundation, Inc.
 *
 * Return nonzero if PATTERN has any special globbing chars in it.
 */
int myGlobPatternP (const char *patternURL)
{
    const char *p;
    char c;
    int open = 0;
  
    (void) urlPath(patternURL, &p);
    while ((c = *p++) != '\0')
	switch (c) {
	case '?':
	case '*':
	    return (1);
	case '[':      /* Only accept an open brace if there is a close */
	    open++;    /* brace to match it.  Bracket expressions must be */
	    continue;  /* complete, according to Posix.2 */
	case ']':
	    if (open)
		return (1);
	    continue;      
	case '\\':
	    if (*p++ == '\0')
		return (0);
	}

    return (0);
}

static int glob_error(/*@unused@*/const char *foo, /*@unused@*/int bar)
{
    return 1;
}

int rpmGlob(const char * patterns, int * argcPtr, const char *** argvPtr)
{
    int ac = 0;
    const char ** av = NULL;
    int argc = 0;
    const char ** argv = NULL;
    const char * path;
    const char * globURL;
    char * globRoot = NULL;
    size_t maxb, nb;
    glob_t gl;
    int ut;
    int i, j;
    int rc;

    rc = poptParseArgvString(patterns, &ac, &av);
    if (rc)
	return rc;

    for (j = 0; j < ac; j++) {
	if (!myGlobPatternP(av[j])) {
	    if (argc == 0)
		argv = xmalloc((argc+2) * sizeof(*argv));
	    else
		argv = xrealloc(argv, (argc+2) * sizeof(*argv));
	    argv[argc] = xstrdup(av[j]);
if (_debug)
fprintf(stderr, "*** rpmGlob argv[%d] \"%s\"\n", argc, argv[argc]);
	    argc++;
	    continue;
	}
	
	gl.gl_pathc = 0;
	gl.gl_pathv = NULL;
	rc = Glob(av[j], 0, glob_error, &gl);
	if (rc)
	    goto exit;

	/* XXX Prepend the URL leader for globs that have stripped it off */
	maxb = 0;
	for (i = 0; i < gl.gl_pathc; i++) {
	    if ((nb = strlen(&(gl.gl_pathv[i][0]))) > maxb)
		maxb = nb;
	}
	
	ut = urlPath(av[j], &path);
	nb = ((ut > URL_IS_DASH) ? (path - av[j]) : 0);
	maxb += nb;
	maxb += 1;
	globURL = globRoot = xmalloc(maxb);

	switch (ut) {
	case URL_IS_HTTP:
	case URL_IS_FTP:
	case URL_IS_PATH:
	case URL_IS_DASH:
	    strncpy(globRoot, av[j], nb);
	    break;
	case URL_IS_UNKNOWN:
	    break;
	}
	globRoot += nb;
	*globRoot = '\0';
if (_debug)
fprintf(stderr, "*** GLOB maxb %d diskURL %d %*s globURL %p %s\n", (int)maxb, (int)nb, (int)nb, av[j], globURL, globURL);
	
	if (argc == 0)
	    argv = xmalloc((gl.gl_pathc+1) * sizeof(*argv));
	else if (gl.gl_pathc > 0)
	    argv = xrealloc(argv, (argc+gl.gl_pathc+1) * sizeof(*argv));
	for (i = 0; i < gl.gl_pathc; i++) {
	    const char * globFile = &(gl.gl_pathv[i][0]);
	    if (globRoot > globURL && globRoot[-1] == '/')
		while (*globFile == '/') globFile++;
	    strcpy(globRoot, globFile);
if (_debug)
fprintf(stderr, "*** rpmGlob argv[%d] \"%s\"\n", argc, globURL);
	    argv[argc++] = xstrdup(globURL);
	}
	/*@-immediatetrans@*/
	Globfree(&gl);
	/*@=immediatetrans@*/
	globURL = _free(globURL);
    }
    if (argv != NULL && argc > 0) {
	argv[argc] = NULL;
	if (argvPtr)
	    *argvPtr = argv;
	if (argcPtr)
	    *argcPtr = argc;
	rc = 0;
    } else
	rc = 1;


exit:
    av = _free(av);
    if (rc || argvPtr == NULL) {
	if (argv != NULL)
	for (i = 0; i < argc; i++)
	    argv[i] = _free(argv[i]);
	argv = _free(argv);
    }
    return rc;
}

/*
 * XXX This is a "dressed" entry to headerGetEntry to do:
 *      1) DIRNAME/BASENAME/DIRINDICES -> FILENAMES tag conversions.
 *      2) i18n lookaside (if enabled).
 */
int rpmHeaderGetEntry(Header h, int_32 tag, int_32 *type,
	void **p, int_32 *c)
{
    switch (tag) {
    case RPMTAG_OLDFILENAMES:
    {	const char ** fl = NULL;
	int count;
	rpmBuildFileList(h, &fl, &count);
	if (count > 0) {
	    *p = fl;
	    if (c)	*c = count;
	    if (type)	*type = RPM_STRING_ARRAY_TYPE;
	    return 1;
	}
	if (c)	*c = 0;
	return 0;
    }	/*@notreached@*/ break;

    case RPMTAG_GROUP:
    case RPMTAG_DESCRIPTION:
    case RPMTAG_SUMMARY:
    {	char fmt[128];
	const char * msgstr;
	const char * errstr;

	fmt[0] = '\0';
	(void) stpcpy( stpcpy( stpcpy( fmt, "%{"), tagName(tag)), "}\n");

	/* XXX FIXME: memory leak. */
        msgstr = headerSprintf(h, fmt, rpmTagTable, rpmHeaderFormats, &errstr);
	if (msgstr) {
	    *p = (void *) msgstr;
	    if (type)	*type = RPM_STRING_TYPE;
	    if (c)	*c = 1;
	    return 1;
	} else {
	    if (c)	*c = 0;
	    return 0;
	}
    }	/*@notreached@*/ break;

    default:
	return headerGetEntry(h, tag, type, p, c);
	/*@notreached@*/ break;
    }
    /*@notreached@*/
}

/*
 * XXX Yet Another dressed entry to unify signature/header tag retrieval.
 */
int rpmPackageGetEntry( /*@unused@*/ void *leadp, Header sigs, Header h,
	int_32 tag, int_32 *type, void **p, int_32 *c)
{
    int_32 sigtag;

    switch (tag) {
    case RPMTAG_SIGSIZE:	sigtag = RPMSIGTAG_SIZE;	break;
    case RPMTAG_SIGLEMD5_1:	sigtag = RPMSIGTAG_LEMD5_1;	break;
    case RPMTAG_SIGPGP:		sigtag = RPMSIGTAG_PGP;		break;
    case RPMTAG_SIGLEMD5_2:	sigtag = RPMSIGTAG_LEMD5_2;	break;
    case RPMTAG_SIGMD5:		sigtag = RPMSIGTAG_MD5;		break;
    case RPMTAG_SIGGPG:		sigtag = RPMSIGTAG_GPG;		break;
    case RPMTAG_SIGPGP5:	sigtag = RPMSIGTAG_GPG;		break;
	
    default:
	return rpmHeaderGetEntry(h, tag, type, p, c);
	/*@notreached@*/ break;
    }

    if (headerIsEntry(h, tag))
	return rpmHeaderGetEntry(h, tag, type, p, c);

    if (sigs == NULL) {
	if (c)	*c = 0;
	return 0;
    }

    return headerGetEntry(sigs, sigtag, type, p, c);
}

/*
 * Up to rpm 3.0.4, packages implicitly provided their own name-version-release.
 * Retrofit an explicit "Provides: name = epoch:version-release.
 */
void providePackageNVR(Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    const char *name, *version, *release;
    int_32 * epoch;
    const char *pEVR;
    char *p;
    int_32 pFlags = RPMSENSE_EQUAL;
    const char ** provides = NULL;
    const char ** providesEVR = NULL;
    rpmTagType pnt, pvt;
    int_32 * provideFlags = NULL;
    int providesCount;
    int i;
    int bingo = 1;

    /* Generate provides for this package name-version-release. */
    (void) headerNVR(h, &name, &version, &release);
    if (!(name && version && release))
	return;
    pEVR = p = alloca(21 + strlen(version) + 1 + strlen(release) + 1);
    *p = '\0';
    if (hge(h, RPMTAG_EPOCH, NULL, (void **) &epoch, NULL)) {
	sprintf(p, "%d:", *epoch);
	while (*p != '\0')
	    p++;
    }
    (void) stpcpy( stpcpy( stpcpy(p, version) , "-") , release);

    /*
     * Rpm prior to 3.0.3 does not have versioned provides.
     * If no provides at all are available, we can just add.
     */
    if (!hge(h, RPMTAG_PROVIDENAME, &pnt, (void **) &provides, &providesCount))
	goto exit;

    /*
     * Otherwise, fill in entries on legacy packages.
     */
    if (!hge(h, RPMTAG_PROVIDEVERSION, &pvt, (void **) &providesEVR, NULL)) {
	for (i = 0; i < providesCount; i++) {
	    char * vdummy = "";
	    int_32 fdummy = RPMSENSE_ANY;
	    (void) headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
			&vdummy, 1);
	    (void) headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
			&fdummy, 1);
	}
	goto exit;
    }

    (void) hge(h, RPMTAG_PROVIDEFLAGS, NULL, (void **) &provideFlags, NULL);

    if (provides && providesEVR && provideFlags)
    for (i = 0; i < providesCount; i++) {
        if (!(provides[i] && providesEVR[i]))
            continue;
	if (!(provideFlags[i] == RPMSENSE_EQUAL &&
	    !strcmp(name, provides[i]) && !strcmp(pEVR, providesEVR[i])))
	    continue;
	bingo = 0;
	break;
    }

exit:
    provides = hfd(provides, pnt);
    providesEVR = hfd(providesEVR, pvt);

    if (bingo) {
	(void) headerAddOrAppendEntry(h, RPMTAG_PROVIDENAME, RPM_STRING_ARRAY_TYPE,
		&name, 1);
	(void) headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
		&pFlags, 1);
	(void) headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
		&pEVR, 1);
    }
}
