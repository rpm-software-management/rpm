#include "system.h"

static int _debug = 0;

#include <rpmlib.h>
#include <rpmurl.h>
#include <rpmmacro.h>	/* XXX for rpmGetPath */

#include "misc.h"

char * RPMVERSION = VERSION;	/* just to put a marker in librpm.a */

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
    free(list[0]);
    free(list);
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

/* compare alpha and numeric segments of two versions */
/* return 1: a is newer than b */
/*        0: a and b are the same version */
/*       -1: b is newer than a */
int rpmvercmp(const char * a, const char * b)
{
    char oldch1, oldch2;
    char * str1, * str2;
    char * one, * two;
    int rc;
    int isnum;

    /* easy comparison to see if versions are identical */
    if (!strcmp(a, b)) return 0;

    str1 = alloca(strlen(a) + 1);
    str2 = alloca(strlen(b) + 1);

    strcpy(str1, a);
    strcpy(str2, b);

    one = str1;
    two = str2;

    /* loop through each version segment of str1 and str2 and compare them */
    while (*one && *two) {
	while (*one && !isalnum(*one)) one++;
	while (*two && !isalnum(*two)) two++;

	str1 = one;
	str2 = two;

	/* grab first completely alpha or completely numeric segment */
	/* leave one and two pointing to the start of the alpha or numeric */
	/* segment and walk str1 and str2 to end of segment */
	if (isdigit(*str1)) {
	    while (*str1 && isdigit(*str1)) str1++;
	    while (*str2 && isdigit(*str2)) str2++;
	    isnum = 1;
	} else {
	    while (*str1 && isalpha(*str1)) str1++;
	    while (*str2 && isalpha(*str2)) str2++;
	    isnum = 0;
	}

	/* save character at the end of the alpha or numeric segment */
	/* so that they can be restored after the comparison */
	oldch1 = *str1;
	*str1 = '\0';
	oldch2 = *str2;
	*str2 = '\0';

	/* take care of the case where the two version segments are */
	/* different types: one numeric and one alpha */
	if (one == str1) return -1;	/* arbitrary */
	if (two == str2) return -1;

	if (isnum) {
	    /* this used to be done by converting the digit segments */
	    /* to ints using atoi() - it's changed because long  */
	    /* digit segments can overflow an int - this should fix that. */

	    /* throw away any leading zeros - it's a number, right? */
	    while (*one == '0') one++;
	    while (*two == '0') two++;

	    /* whichever number has more digits wins */
	    if (strlen(one) > strlen(two)) return 1;
	    if (strlen(two) > strlen(one)) return -1;
	}

	/* strcmp will return which one is greater - even if the two */
	/* segments are alpha or if they are numeric.  don't return  */
	/* if they are equal because there might be more segments to */
	/* compare */
	rc = strcmp(one, two);
	if (rc) return rc;

	/* restore character that was replaced by null above */
	*str1 = oldch1;
	one = str1;
	*str2 = oldch2;
	two = str2;
    }

    /* this catches the case where all numeric and alpha segments have */
    /* compared identically but the segment sepparating characters were */
    /* different */
    if ((!*one) && (!*two)) return 0;

    /* whichever version still has characters left over wins */
    if (!*one) return -1; else return 1;
}

void stripTrailingSlashes(char * str)
{
    char * chptr;

    chptr = str + strlen(str) - 1;
    while (*chptr == '/' && chptr >= str) {
	*chptr = '\0';
	chptr--;
    }
}

int doputenv(const char *str)
{
    char * a;

    /* FIXME: this leaks memory! */

    a = xmalloc(strlen(str) + 1);
    strcpy(a, str);

    return putenv(a);
}

int dosetenv(const char *name, const char *value, int overwrite)
{
    int i;
    char * a;

    /* FIXME: this leaks memory! */

    if (!overwrite && getenv(name)) return 0;

    i = strlen(name) + strlen(value) + 2;
    a = xmalloc(i);
    if (!a) return 1;

    strcpy(a, name);
    strcat(a, "=");
    strcat(a, value);

    return putenv(a);
}

/* unameToUid(), uidTouname() and the group variants are really poorly
   implemented. They really ought to use hash tables. I just made the
   guess that most files would be owned by root or the same person/group
   who owned the last file. Those two values are cached, everything else
   is looked up via getpw() and getgr() functions.  If this performs
   too poorly I'll have to implement it properly :-( */

int unameToUid(const char * thisUname, uid_t * uid)
{
    /*@only@*/ static char * lastUname = NULL;
    static int lastUnameLen = 0;
    static int lastUnameAlloced;
    static uid_t lastUid;
    struct passwd * pwent;
    int thisUnameLen;

    if (!thisUname) {
	lastUnameLen = 0;
	return -1;
    } else if (!strcmp(thisUname, "root")) {
	*uid = 0;
	return 0;
    }

    thisUnameLen = strlen(thisUname);
    if (!lastUname || thisUnameLen != lastUnameLen ||
	strcmp(thisUname, lastUname)) {
	if (lastUnameAlloced < thisUnameLen + 1) {
	    lastUnameAlloced = thisUnameLen + 10;
	    lastUname = xrealloc(lastUname, lastUnameAlloced);	/* XXX memory leak */
	}
	strcpy(lastUname, thisUname);

	pwent = getpwnam(thisUname);
	if (!pwent) {
	    endpwent();
	    pwent = getpwnam(thisUname);
	    if (!pwent) return -1;
	}

	lastUid = pwent->pw_uid;
    }

    *uid = lastUid;

    return 0;
}

int gnameToGid(const char * thisGname, gid_t * gid)
{
    /*@only@*/ static char * lastGname = NULL;
    static int lastGnameLen = 0;
    static int lastGnameAlloced;
    static uid_t lastGid;
    int thisGnameLen;
    struct group * grent;

    if (!thisGname) {
	lastGnameLen = 0;
	return -1;
    } else if (!strcmp(thisGname, "root")) {
	*gid = 0;
	return 0;
    }

    thisGnameLen = strlen(thisGname);
    if (!lastGname || thisGnameLen != lastGnameLen ||
	strcmp(thisGname, lastGname)) {
	if (lastGnameAlloced < thisGnameLen + 1) {
	    lastGnameAlloced = thisGnameLen + 10;
	    lastGname = xrealloc(lastGname, lastGnameAlloced);	/* XXX memory leak */
	}
	strcpy(lastGname, thisGname);

	grent = getgrnam(thisGname);
	if (!grent) {
	    endgrent();
	    grent = getgrnam(thisGname);
	    if (!grent) return -1;
	}
	lastGid = grent->gr_gid;
    }

    *gid = lastGid;

    return 0;
}

char * uidToUname(uid_t uid)
{
    static int lastUid = -1;
    /*@only@*/ static char * lastUname = NULL;
    static int lastUnameLen = 0;
    struct passwd * pwent;
    int len;

    if (uid == (uid_t) -1) {
	lastUid = -1;
	return NULL;
    } else if (!uid) {
	return "root";
    } else if (uid == lastUid) {
	return lastUname;
    } else {
	pwent = getpwuid(uid);
	if (!pwent) return NULL;

	lastUid = uid;
	len = strlen(pwent->pw_name);
	if (lastUnameLen < len + 1) {
	    lastUnameLen = len + 20;
	    lastUname = xrealloc(lastUname, lastUnameLen);
	}
	strcpy(lastUname, pwent->pw_name);

	return lastUname;
    }
}

char * gidToGname(gid_t gid)
{
    static int lastGid = -1;
    /*@only@*/ static char * lastGname = NULL;
    static int lastGnameLen = 0;
    struct group * grent;
    int len;

    if (gid == (gid_t) -1) {
	lastGid = -1;
	return NULL;
    } else if (!gid) {
	return "root";
    } else if (gid == lastGid) {
	return lastGname;
    } else {
	grent = getgrgid(gid);
	if (!grent) return NULL;

	lastGid = gid;
	len = strlen(grent->gr_name);
	if (lastGnameLen < len + 1) {
	    lastGnameLen = len + 20;
	    lastGname = xrealloc(lastGname, lastGnameLen);
	}
	strcpy(lastGname, grent->gr_name);

	return lastGname;
    }
}

int makeTempFile(const char * prefix, const char ** fnptr, FD_t * fdptr)
{
    const char * tempfn = NULL;
    const char * tfn = NULL;
    int temput;
    FD_t fd = NULL;
    int ran;

    if (!prefix) prefix = "";

    /* XXX should probably use mktemp here */
    srand(time(NULL));
    ran = rand() % 100000;

    /* maybe this should use link/stat? */

    do {
	char tfnbuf[64];
#ifndef	NOTYET
	sprintf(tfnbuf, "rpm-tmp.%d", ran++);
	if (tempfn)	xfree(tempfn);
	tempfn = rpmGenPath(prefix, "%{_tmppath}/", tfnbuf);
#else
	strcpy(tfnbuf, "rpm-tmp.XXXXXX");
	if (tempfn)	xfree(tempfn);
	tempfn = rpmGenPath(prefix, "%{_tmppath}/", mktemp(tfnbuf));
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
	struct stat sb, sb2;
    case URL_IS_PATH:
    case URL_IS_UNKNOWN:
	if (!stat(tfn, &sb) && S_ISLNK(sb.st_mode)) {
	    rpmError(RPMERR_SCRIPT, _("error creating temporary file %s"), tfn);
	    goto errxit;
	}

	if (sb.st_nlink != 1) {
	    rpmError(RPMERR_SCRIPT, _("error creating temporary file %s"), tfn);
	    goto errxit;
	}

	if (fstat(Fileno(fd), &sb2) == 0) {
	    if (sb2.st_ino != sb.st_ino || sb2.st_dev != sb.st_dev) {
		rpmError(RPMERR_SCRIPT, _("error creating temporary file %s"), tfn);
		goto errxit;
	    }
	}
	break;
    default:
	break;
    }

    if (fnptr)
	*fnptr = tempfn;
    else if (tempfn) {
	xfree(tempfn);
	tempfn = NULL;
    }
    *fdptr = fd;

    return 0;

errxit:
    if (tempfn) xfree(tempfn);
    if (fd) Fclose(fd);
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
    char ** fileNames;
    const char ** dirNames;
    const char ** baseNames;
    int_32 * dirIndexes;
    int count;
    int i;
    int dirIndex = -1;

    /*
     * This assumes the file list is already sorted, and begins with a
     * single '/'. That assumption isn't critical, but it makes things go
     * a bit faster.
     */

    if (!headerGetEntry(h, RPMTAG_OLDFILENAMES, NULL,
			(void **) &fileNames, &count))
	return;		/* no file list */

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
	char *baseName = strrchr(fileNames[i], '/') + 1;
	char savechar;
	int len = baseName - fileNames[i];

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
    headerAddEntry(h, RPMTAG_DIRNAMES, RPM_STRING_ARRAY_TYPE,
			dirNames, dirIndex + 1);
    headerAddEntry(h, RPMTAG_DIRINDEXES, RPM_INT32_TYPE,
			dirIndexes, count);
    headerAddEntry(h, RPMTAG_BASENAMES, RPM_STRING_ARRAY_TYPE,
			baseNames, count);

    xfree(fileNames);

    headerRemoveEntry(h, RPMTAG_OLDFILENAMES);
}

/*
 * This is pretty straight-forward. The only thing that even resembles a trick
 * is getting all of this into a single xmalloc'd block.
 */
static void doBuildFileList(Header h, /*@out@*/ const char *** fileListPtr,
			    /*@out@*/ int * fileCountPtr, int baseNameTag,
			    int dirNameTag, int dirIndexesTag)
{
    const char ** baseNames;
    const char ** dirNames;
    int * dirIndexes;
    int count;
    const char ** fileNames;
    int size;
    char * data;
    int i;

    if (!headerGetEntry(h, baseNameTag, NULL, (void **) &baseNames, &count)) {
	*fileListPtr = NULL;
	*fileCountPtr = 0;
	return;		/* no file list */
    }

    headerGetEntry(h, dirNameTag, NULL, (void **) &dirNames, NULL);
    headerGetEntry(h, dirIndexesTag, NULL, (void **) &dirIndexes, &count);

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
    xfree(baseNames);
    xfree(dirNames);

    *fileListPtr = fileNames;
    *fileCountPtr = count;
}

void expandFilelist(Header h)
{
    const char ** fileNames = NULL;
    int count = 0;

    doBuildFileList(h, &fileNames, &count, RPMTAG_BASENAMES,
			RPMTAG_DIRNAMES, RPMTAG_DIRINDEXES);

    if (fileNames == NULL || count <= 0)
	return;

    headerAddEntry(h, RPMTAG_OLDFILENAMES, RPM_STRING_ARRAY_TYPE,
			fileNames, count);

    xfree(fileNames);

    headerRemoveEntry(h, RPMTAG_BASENAMES);
    headerRemoveEntry(h, RPMTAG_DIRNAMES);
    headerRemoveEntry(h, RPMTAG_DIRINDEXES);
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
if (_debug)
fprintf(stderr, "*** rpmGlob argv[%d] \"%s\"\n", argc, av[j]);
	    argv[argc++] = xstrdup(av[j]);
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
	Globfree(&gl);
	xfree(globURL);
    }
    argv[argc] = NULL;
    if (argvPtr)
	*argvPtr = argv;
    if (argcPtr)
	*argcPtr = argc;
    rc = 0;

exit:
    if (av)
	xfree(av);
    if ((rc || argvPtr == NULL) && argv) {
	for (i = 0; i < argc; i++)
	    xfree(argv[i]);
	xfree(argv);
	argv = NULL;
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
 * Up to rpm 3.0.4, packages implicitly provided their own name-version-release.
 * Retrofit an explicit "Provides: name = epoch:version-release.
 */
void providePackageNVR(Header h)
{
    const char *name, *version, *release;
    int_32 * epoch;
    const char *pEVR;
    char *p;
    int pFlags = RPMSENSE_EQUAL;
    const char ** provides = NULL;
    const char ** providesEVR = NULL;
    int_32 * provideFlags = NULL;
    int providesCount;
    int type;
    int i;
    int bingo = 1;

    /* Generate provides for this package name-version-release. */
    headerNVR(h, &name, &version, &release);
    pEVR = p = alloca(21 + strlen(version) + 1 + strlen(release) + 1);
    *p = '\0';
    if (headerGetEntry(h, RPMTAG_EPOCH, NULL, (void **) &epoch, NULL)) {
	sprintf(p, "%d:", *epoch);
	while (*p++)
	    ;
    }
    (void) stpcpy( stpcpy( stpcpy(p, version) , "-") , release);

    /*
     * Rpm prior to 3.0.3 does not have versioned provides.
     * If no provides version info is available, then just add.
     */
    if (!headerGetEntry(h, RPMTAG_PROVIDEVERSION, &type,
		(void **) &providesEVR, &providesCount))
	goto exit;

    headerGetEntry(h, RPMTAG_PROVIDEFLAGS, &type,
	(void **) &provideFlags, &providesCount);

    if (!headerGetEntry(h, RPMTAG_PROVIDENAME, &type,
		(void **) &provides, &providesCount)) {
	goto exit;
    }

    for (i = 0; i < providesCount; i++) {
	if (!(provideFlags[i] == RPMSENSE_EQUAL &&
	    !strcmp(name, provides[i]) && !strcmp(pEVR, providesEVR[i])))
	    continue;
	bingo = 0;
	break;
    }

exit:
    if (provides) xfree(provides);
    if (providesEVR) xfree(providesEVR);

    if (bingo) {
	headerAddOrAppendEntry(h, RPMTAG_PROVIDENAME, RPM_STRING_ARRAY_TYPE,
		&name, 1);
	headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
		&pEVR, 1);
	headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
		&pFlags, 1);
    }
}
