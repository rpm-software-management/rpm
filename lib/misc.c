#include "system.h"

#include <rpmlib.h>
#include <rpmurl.h>
#include <rpmmacro.h>	/* XXX for rpmGetPath */

#include "misc.h"

char * RPMVERSION = VERSION;	/* just to put a marker in librpm.a */

char ** splitString(const char * str, int length, char sep) {
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

void freeSplitString(char ** list) {
    free(list[0]);
    free(list);
}

int rpmfileexists(const char * urlfn) {
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
	break;
    }

    return 1;
}

/* compare alpha and numeric segments of two versions */
/* return 1: a is newer than b */
/*        0: a and b are the same version */
/*       -1: b is newer than a */
int rpmvercmp(const char * a, const char * b) {
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

void stripTrailingSlashes(char * str) {
    char * chptr;

    chptr = str + strlen(str) - 1;
    while (*chptr == '/' && chptr >= str) {
	*chptr = '\0';
	chptr--;
    }
}

int doputenv(const char *str) {
    char * a;
    
    /* FIXME: this leaks memory! */

    a = xmalloc(strlen(str) + 1);
    strcpy(a, str);

    return putenv(a);
}

int dosetenv(const char *name, const char *value, int overwrite) {
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

int unameToUid(const char * thisUname, uid_t * uid) {
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

int gnameToGid(const char * thisGname, gid_t * gid) {
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

char * uidToUname(uid_t uid) {
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

char * gidToGname(gid_t gid) {
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

int makeTempFile(const char * prefix, const char ** fnptr, FD_t * fdptr) {
    const char * tempfn = NULL;
    const char * tfn = NULL;
    int temput;
    FD_t fd;
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
    else if (tempfn)
	xfree(tempfn);
    *fdptr = fd;

    return 0;

errxit:
    if (tempfn) xfree(tempfn);
    if (fd) Fclose(fd);
    return 1;
}

char * currentDirectory(void) {
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

void compressFilelist(Header h) {
    const char ** files;
    const char ** dirList;
    int * compDirList;
    const char ** baseNames;
    int fileCount;
    int i;
    int lastDir = -1;
    int lastLen = -1;

    /* This assumes thie file list is already sorted, and begins with a
       single '/'. That assumption isn't critical, but it makes things go
       a bit faster. */

    if (!headerGetEntry(h, RPMTAG_OLDFILENAMES, NULL, (void **) &files,
			&fileCount)) 
	/* no file list */
	return;

    if (files[0][0] != '/') {
	/* HACK. Source RPM, so just do things differently */
	free(files);
	return;
    }

    dirList = alloca(sizeof(*dirList) * fileCount);	/* worst case */
    baseNames = alloca(sizeof(*dirList) * fileCount); 
    compDirList = alloca(sizeof(*compDirList) * fileCount); 

    for (i = 0; i < fileCount; i++) {
	char *tail = strrchr(files[i], '/') + 1;
	
	if (lastDir < 0 || (lastLen != (tail - files[i])) ||
		strncmp(dirList[lastDir], files[i], tail - files[i])) {
	    char *s = alloca(tail - files[i] + 1);
	    memcpy(s, files[i], tail - files[i]);
	    s[tail - files[i]] = '\0';
	    dirList[++lastDir] = s;
	    lastLen = tail - files[i];
	} 

	compDirList[i] = lastDir;
	baseNames[i] = tail;
    }

    headerAddEntry(h, RPMTAG_COMPDIRLIST, RPM_STRING_ARRAY_TYPE, dirList, 
		   lastDir + 1);
    headerAddEntry(h, RPMTAG_COMPFILEDIRS, RPM_INT32_TYPE, compDirList, 
		   fileCount);
    headerAddEntry(h, RPMTAG_COMPFILELIST, RPM_STRING_ARRAY_TYPE, baseNames, 
		   fileCount);

    free(files);

    headerRemoveEntry(h, RPMTAG_OLDFILENAMES);
}

/* this is pretty straight-forward. The only thing that even resembles a trick
   is getting all of this into a single xmalloc'd block */
static void doBuildFileList(Header h, /*@out@*/ const char *** fileListPtr, 
			    /*@out@*/ int * fileCountPtr, int baseNameTag,
			    int dirListTag, int dirIndexesTag) {
    int * dirList;
    const char ** dirs;
    const char ** tails;
    int count;
    const char ** fileList;
    int size;
    char * data;
    int i;

    if (!headerGetEntry(h, baseNameTag, NULL, (void **) &tails,
			&count)) 
	/* no file list */
	return;

    headerGetEntry(h, dirListTag, NULL, (void **) &dirs,
			&count); 
    headerGetEntry(h, dirIndexesTag, NULL, (void **) &dirList,
			&count); 

    size = sizeof(*fileList) * count;
    for (i = 0; i < count; i++) {
	size += strlen(tails[i]) + strlen(dirs[dirList[i]]) + 1;
    }

    fileList = xmalloc(size);
    data = ((char *) fileList) + (sizeof(*fileList) * count);

    for (i = 0; i < count; i++) {
        fileList[i] = data;
	strcpy(data, dirs[dirList[i]]);
	strcat(data, tails[i]);
	data += strlen(tails[i]) + strlen(dirs[dirList[i]]) + 1;
    }

    *fileListPtr = fileList;
    *fileCountPtr = count;
    free(tails);
    free(dirs);
}

void buildFileList(Header h, const char *** fileListPtr, int * fileCountPtr)
{
    doBuildFileList(h, fileListPtr, fileCountPtr, RPMTAG_COMPFILELIST,
		    RPMTAG_COMPDIRLIST, RPMTAG_COMPFILEDIRS);
}

void buildOrigFileList(Header h, const char *** fileListPtr, int * fileCountPtr)
{
    doBuildFileList(h, fileListPtr, fileCountPtr, RPMTAG_ORIGCOMPFILELIST,
		    RPMTAG_ORIGCOMPDIRLIST, RPMTAG_ORIGCOMPFILEDIRS);
}
