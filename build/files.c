/* RPM - Copyright (C) 1995 Red Hat Software
 * 
 * prepack.c - routines for packaging
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "miscfn.h"
#include "spec.h"
#include "specP.h"
#include "stringbuf.h"
#include "build.h"
#include "messages.h"
#include "md5.h"
#include "myftw.h"
#include "header.h"
#include "files.h"
#include "names.h"
#include "rpmlead.h"
#include "rpmlib.h"
#include "misc.h"
#include "macro.h"

#define BINARY_HEADER 0
#define SOURCE_HEADER 1

struct file_entry {
    char file[1024];
    int isdoc;
    int conf;
    int isspecfile;
    int verify_flags;
    char *uname;  /* reference -- do not free */
    char *gname;  /* reference -- do not free */
    struct stat statbuf;
    struct file_entry *next;
};

static int add_file(struct file_entry **festack, const char *name,
		    int isdoc, int isconf, int isdir, int verify_flags,
		    char *Pmode, char *Uname, char *Gname, char *prefix);
static int compare_fe(const void *ap, const void *bp);
static int add_file_aux(const char *file, struct stat *sb, int flag);
static int glob_error(const char *foo, int bar);
static int glob_pattern_p (char *pattern);
static int parseForVerify(char *buf, int *verify_flags);
static int parseForConfig(char *buf, int *conf);
static int parseForAttr(char *origbuf, char **currPmode,
			char **currUname, char **currGname);

static void resetDocdir(void);
static void addDocdir(char *dirname);
static int isDoc(char *filename);

static int processFileListFailed;

static void parseForDocFiles(struct PackageRec *package, char *line)
{
    if (! (line = strstr(line, "%doc"))) {
	return;
    }

    line += 4;
    if ((*line != ' ') && (*line != '\t')) {
	return;
    }
    line += strspn(line, " \t\n");
    if ((! *line) || (*line == '/')) {
	return;
    }
    
    appendLineStringBuf(package->doc, "mkdir -p $DOCDIR");
    appendStringBuf(package->doc, "cp -pr ");
    appendStringBuf(package->doc, line);
    appendLineStringBuf(package->doc, " $DOCDIR");
}

int finish_filelists(Spec spec)
{
    char buf[1024];
    FILE *file;
    struct PackageRec *pr = spec->packages;
    char *s, **files, **line;
    char *version, *release, *packageVersion, *packageRelease, *docs, *name;

    headerGetEntry(spec->packages->header, RPMTAG_VERSION, NULL,
	     (void *) &version, NULL);
    headerGetEntry(spec->packages->header, RPMTAG_RELEASE, NULL,
	     (void *) &release, NULL);
    
    while (pr) {
	if (pr->fileFile) {
	    sprintf(buf, "%s/%s/%s", rpmGetVar(RPMVAR_BUILDDIR),
		    build_subdir, pr->fileFile);
	    rpmMessage(RPMMESS_DEBUG, "Reading file names from: %s\n", buf);
	    if ((file = fopen(buf, "r")) == NULL) {
		rpmError(RPMERR_BADSPEC, "unable to open filelist: %s\n", buf);
		return(RPMERR_BADSPEC);
	    }
	    while (fgets(buf, sizeof(buf), file)) {
		expandMacros(buf);
		appendStringBuf(pr->filelist, buf);
	    }
	    fclose(file);
	}

	/* parse for %doc wierdness */
	s = getStringBuf(pr->filelist);
	files = splitString(s, strlen(s), '\n');
	line = files;
	while (*line) {
	    parseForDocFiles(pr, *line);
	    line++;
	}
	freeSplitString(files);

	/* Handle subpackage version/release overrides */
        if (!headerGetEntry(pr->header, RPMTAG_VERSION, NULL,
		      (void *) &packageVersion, NULL)) {
            packageVersion = version;
	}
        if (!headerGetEntry(pr->header, RPMTAG_RELEASE, NULL,
		      (void *) &packageRelease, NULL)) {
            packageRelease = release;
	}

	/* Generate the doc script */
	appendStringBuf(spec->doc, "DOCDIR=$RPM_ROOT_DIR/$RPM_DOC_DIR/");
	headerGetEntry(pr->header, RPMTAG_NAME, NULL, (void *) &name, NULL);
	sprintf(buf, "%s-%s-%s", name, packageVersion, packageRelease);
	appendLineStringBuf(spec->doc, buf);
	docs = getStringBuf(pr->doc);
	if (*docs) {
	    appendLineStringBuf(spec->doc, "rm -rf $DOCDIR");
	    appendLineStringBuf(spec->doc, docs);
	}

	pr = pr->next;
    }

    return 0;
}

int process_filelist(Header header, struct PackageRec *pr,
		     StringBuf sb, int *size, char *name,
		     char *version, char *release, int type,
		     char *prefix, char *specFile)
{
    char buf[1024];
    char **files, **fp;
    struct file_entry *fes, *fest;
    struct file_entry **file_entry_array;
    int isdoc, conf, isdir, verify_flags;
    char *currPmode=NULL;	/* hold info from %attr() */
    char *currUname=NULL;	/* hold info from %attr() */
    char *currGname=NULL;	/* hold info from %attr() */
    char *filename, *s;
    char *str;
    int count = 0;
    int c, x;
    glob_t glob_result;
    int special_doc;
    int passed_special_doc = 0;
    int tc;
    char *tcs;
    int currentTime;

    processFileListFailed = 0;
    fes = NULL;
    *size = 0;

    resetDocdir();

    str = getStringBuf(sb);
    files = splitString(str, strlen(str), '\n');
    fp = files;

    while (*fp) {
	strcpy(buf, *fp);  /* temp copy */
	isdoc = 0;
	special_doc = 0;
	conf = 0;
	isdir = 0;
	if (currPmode) {
	    free (currPmode);
	    currPmode = NULL;
	}
	if (currUname) {
	    free (currUname);
	    currUname = NULL;
	}
	if (currGname) {
	    free (currGname);
	    currGname = NULL;
	}
	verify_flags = RPMVERIFY_ALL;
	filename = NULL;

	/* First preparse buf for %verify() */
	if (parseForVerify(buf, &verify_flags)) {
	    processFileListFailed = 1;
	    fp++; continue;
	}
	
	/* Next parse for %attr() */
	if (parseForAttr(buf, &currPmode, &currUname, &currGname)) {
	    processFileListFailed = 1;
	    fp++; continue;
	}

	/* Then parse for %config or %config() */
	if (parseForConfig(buf, &conf)) {
	    processFileListFailed = 1;
	    fp++; continue;
	}

	s = strtok(buf, " \t\n");
	while (s) {
	    if (!strcmp(s, "%docdir")) {
	        s = strtok(NULL, " \t\n");
		addDocdir(s);
		break;
	    } else if (!strcmp(s, "%doc")) {
		isdoc = 1;
	    } else if (!strcmp(s, "%dir")) {
		isdir = 1;
	    } else {
		if (filename) {
		    /* We already got a file -- error */
		    rpmError(RPMERR_BADSPEC,
			  "Two files on one line: %s", filename);
		    processFileListFailed = 1;
		}
		if (*s != '/') {
		    if (isdoc) {
			special_doc = 1;
		    } else {
			/* not in %doc, does not begin with / -- error */
			rpmError(RPMERR_BADSPEC,
			      "File must begin with \"/\": %s", s);
			processFileListFailed = 1;
		    }
		} else {
		    filename = s;
		}
	    }
	    s = strtok(NULL, " \t\n");
	}
	if (special_doc) {
	    if (passed_special_doc) {
		fp++;
		continue;
	    } else {
		if (filename || conf || isdir) {
		    rpmError(RPMERR_BADSPEC,
			  "Can't mix special %%doc with other forms: %s", fp);
		    processFileListFailed = 1;
		    fp++; continue;
		}
		sprintf(buf, "%s/%s-%s-%s", rpmGetVar(RPMVAR_DEFAULTDOCDIR), 
			name, version, release);
		filename = buf;
		passed_special_doc = 1;
	    }
	}
	if (! filename) {
	    fp++;
	    continue;
	}

	if (type == RPMLEAD_BINARY) {
	    /* check that file starts with leading "/" */
	    if (*filename != '/') {
		rpmError(RPMERR_BADSPEC,
		      "File needs leading \"/\": %s", filename);
		processFileListFailed = 1;
		fp++; continue;
	    }

	    if (glob_pattern_p(filename)) {
		char fullname[1024];

		if (rpmGetVar(RPMVAR_ROOT)) {
		    sprintf(fullname, "%s%s", rpmGetVar(RPMVAR_ROOT), filename);
		} else {
		    strcpy(fullname, filename);
		}

	        if (glob(fullname, 0, glob_error, &glob_result) ||
		    (glob_result.gl_pathc < 1)) {
		    rpmError(RPMERR_BADSPEC, "No matches: %s", fullname);
		    processFileListFailed = 1;
		    globfree(&glob_result);
		    fp++; continue;
		}
		
		c = 0;
		x = 0;
		while (x < glob_result.gl_pathc) {
		    int offset = strlen(rpmGetVar(RPMVAR_ROOT) ? : "");
		    c += add_file(&fes, &(glob_result.gl_pathv[x][offset]),
				  isdoc, conf, isdir, verify_flags,
				  currPmode, currUname, currGname, prefix);
		    x++;
		}
		globfree(&glob_result);
	    } else {
	        c = add_file(&fes, filename, isdoc, conf, isdir,
			     verify_flags, currPmode, currUname,
			     currGname, prefix);
	    }
	} else {
	    /* Source package are the simple case */
	    fest = malloc(sizeof(struct file_entry));
	    fest->isdoc = 0;
	    fest->conf = 0;
	    if (!strcmp(filename, specFile)) {
		fest->isspecfile = 1;
	    }
	    fest->verify_flags = 0;  /* XXX - something else? */
	    stat(filename, &fest->statbuf);
	    fest->uname = getUname(fest->statbuf.st_uid);
	    fest->gname = getGname(fest->statbuf.st_gid);
	    if (! (fest->uname && fest->gname)) {
		rpmError(RPMERR_BADSPEC, "Bad owner/group: %s", filename);
		fest->uname = "";
		fest->gname = "";
		processFileListFailed = 1;
	    }
	    strcpy(fest->file, filename);
	    fest->next = fes;
	    fes = fest;
	    c = 1;
	}
	    
	if (! c) {
	    rpmError(RPMERR_BADSPEC, "File not found: %s", filename);
	    processFileListFailed = 1;
	}
	count += c;
	
	fp++;
    }

    /* If there are no files, don't add anything to the header */
    if (count) {
	char ** fileList;
	char ** fileMD5List;
	char ** fileLinktoList;
	int_32 * fileSizeList;
	int_32 * fileUIDList;
	int_32 * fileGIDList;
	char ** fileUnameList;
	char ** fileGnameList;
	int_32 * fileMtimesList;
	int_32 * fileFlagsList;
	int_16 * fileModesList;
	int_16 * fileRDevsList;
 	int_32 * fileVerifyFlagsList;

	fileList = malloc(sizeof(char *) * count);
	fileLinktoList = malloc(sizeof(char *) * count);
	fileMD5List = malloc(sizeof(char *) * count);
	fileSizeList = malloc(sizeof(int_32) * count);
	fileUIDList = malloc(sizeof(int_32) * count);
	fileGIDList = malloc(sizeof(int_32) * count);
	fileUnameList = malloc(sizeof(char *) * count);
	fileGnameList = malloc(sizeof(char *) * count);
	fileMtimesList = malloc(sizeof(int_32) * count);
	fileFlagsList = malloc(sizeof(int_32) * count);
	fileModesList = malloc(sizeof(int_16) * count);
	fileRDevsList = malloc(sizeof(int_16) * count);
	fileVerifyFlagsList = malloc(sizeof(int_32) * count);

	/* Build a reverse sorted file array.  */
	/* This makes uninstalls a lot easier. */
	file_entry_array = malloc(sizeof(struct file_entry *) * count);
	c = 0;
	fest = fes;
	while (fest) {
	    file_entry_array[c++] = fest;
	    fest = fest->next;
	}
	qsort(file_entry_array, count, sizeof(struct file_entry *),
	      compare_fe);
		
	/* Do timecheck */
	tc = 0;
	currentTime = time(NULL);
	if ((tcs = rpmGetVar(RPMVAR_TIMECHECK))) {
	    tc = strtoul(tcs, NULL, 10);
	}
    
	c = 0;
	while (c < count) {
	    fest = file_entry_array[c];
	    if (type == RPMLEAD_BINARY) {
		x = strlen(fest->file) - 1;
		if (x && fest->file[x] == '/') {
		    fest->file[x] = '\0';
		}
		fileList[c] = fest->file;
	    } else {
		fileList[c] = strrchr(fest->file, '/') + 1;
	    }
	    if ((c > 0) && !strcmp(fileList[c], fileList[c-1])) {
		rpmError(RPMERR_BADSPEC, "File listed twice: %s", fileList[c]);
		processFileListFailed = 1;
	    }
	    
	    fileUnameList[c] = fest->uname;
	    fileGnameList[c] = fest->gname;
	    *size += fest->statbuf.st_size;
	    if (S_ISREG(fest->statbuf.st_mode)) {
		if (rpmGetVar(RPMVAR_ROOT)) {
		    sprintf(buf, "%s%s", rpmGetVar(RPMVAR_ROOT), fest->file);
		} else {
		    strcpy(buf, fest->file);
		}
		mdfile(buf, buf);
		fileMD5List[c] = strdup(buf);
		rpmMessage(RPMMESS_DEBUG, "md5(%s) = %s\n", fest->file, buf);
	    } else {
		/* This is stupid */
		fileMD5List[c] = strdup("");
	    }
	    fileSizeList[c] = fest->statbuf.st_size;
	    fileUIDList[c] = fest->statbuf.st_uid;
	    fileGIDList[c] = fest->statbuf.st_gid;
	    fileMtimesList[c] = fest->statbuf.st_mtime;

	    /* Do timecheck */
	    if (tc && (type == RPMLEAD_BINARY)) {
		if (currentTime - fest->statbuf.st_mtime > tc) {
		    rpmMessage(RPMMESS_WARNING, "TIMECHECK failure: %s\n",
			    fest->file);
		}
	    }
    
	    fileFlagsList[c] = 0;
	    if (isDoc(fest->file))
	        fileFlagsList[c] |= RPMFILE_DOC;
	    if (fest->isdoc) 
		fileFlagsList[c] |= RPMFILE_DOC;
	    if (fest->conf && !(fest->statbuf.st_mode & S_IFDIR))
		fileFlagsList[c] |= fest->conf;
	    if (fest->isspecfile)
		fileFlagsList[c] |= RPMFILE_SPECFILE;

	    fileModesList[c] = fest->statbuf.st_mode;
	    fileRDevsList[c] = fest->statbuf.st_rdev;
	    fileVerifyFlagsList[c] = fest->verify_flags;

	    if (S_ISLNK(fest->statbuf.st_mode)) {
		if (rpmGetVar(RPMVAR_ROOT)) {
		    sprintf(buf, "%s%s", rpmGetVar(RPMVAR_ROOT), fest->file);
		} else {
		    strcpy(buf, fest->file);
		}
	        buf[readlink(buf, buf, 1024)] = '\0';
		fileLinktoList[c] = strdup(buf);
	    } else {
		/* This is stupid */
		fileLinktoList[c] = strdup("");
	    }
	    c++;
	}

	/* Add the header entries */
	c = count;
	headerAddEntry(header, RPMTAG_FILENAMES, RPM_STRING_ARRAY_TYPE, fileList, c);
	headerAddEntry(header, RPMTAG_FILELINKTOS, RPM_STRING_ARRAY_TYPE,
		 fileLinktoList, c);
	headerAddEntry(header, RPMTAG_FILEMD5S, RPM_STRING_ARRAY_TYPE, fileMD5List, c);
	headerAddEntry(header, RPMTAG_FILESIZES, RPM_INT32_TYPE, fileSizeList, c);
	headerAddEntry(header, RPMTAG_FILEUIDS, RPM_INT32_TYPE, fileUIDList, c);
	headerAddEntry(header, RPMTAG_FILEGIDS, RPM_INT32_TYPE, fileGIDList, c);
	headerAddEntry(header, RPMTAG_FILEUSERNAME, RPM_STRING_ARRAY_TYPE,
		 fileUnameList, c);
	headerAddEntry(header, RPMTAG_FILEGROUPNAME, RPM_STRING_ARRAY_TYPE,
		 fileGnameList, c);
	headerAddEntry(header, RPMTAG_FILEMTIMES, RPM_INT32_TYPE, fileMtimesList, c);
	headerAddEntry(header, RPMTAG_FILEFLAGS, RPM_INT32_TYPE, fileFlagsList, c);
	headerAddEntry(header, RPMTAG_FILEMODES, RPM_INT16_TYPE, fileModesList, c);
	headerAddEntry(header, RPMTAG_FILERDEVS, RPM_INT16_TYPE, fileRDevsList, c);
	headerAddEntry(header, RPMTAG_FILEVERIFYFLAGS, RPM_INT32_TYPE,
		 fileVerifyFlagsList, c);
	
	/* Free the allocated strings */
	c = count;
	while (c--) {
	    free(fileMD5List[c]);
	    free(fileLinktoList[c]);
	}

	/* Free all those lists */
	free(fileList);
	free(fileLinktoList);
	free(fileMD5List);
	free(fileSizeList);
	free(fileUIDList);
	free(fileGIDList);
	free(fileUnameList);
	free(fileGnameList);
	free(fileMtimesList);
	free(fileFlagsList);
	free(fileModesList);
	free(fileRDevsList);
	free(fileVerifyFlagsList);
	
	/* Free the file entry array */
	free(file_entry_array);
	
	/* Free the file entry stack */
	fest = fes;
	while (fest) {
	    fes = fest->next;
	    free(fest);
	    fest = fes;
	}
    }
    
    freeSplitString(files);
    return processFileListFailed;
}

/*************************************************************/
/*                                                           */
/* misc                                                      */
/*                                                           */
/*************************************************************/

static int compare_fe(const void *ap, const void *bp)
{
    char *a, *b;

    a = (*(struct file_entry **)ap)->file;
    b = (*(struct file_entry **)bp)->file;

    return strcmp(a, b);
}

/*************************************************************/
/*                                                           */
/* Doc dir stuff                                             */
/*                                                           */
/*************************************************************/

/* XXX hard coded limit -- only 1024 %docdir allowed */
static char *docdirs[1024];
static int docdir_count;

static void resetDocdir(void)
{
    while (docdir_count--) {
        free(docdirs[docdir_count]);
    }
    docdir_count = 0;
    docdirs[docdir_count++] = strdup("/usr/doc");
    docdirs[docdir_count++] = strdup("/usr/man");
    docdirs[docdir_count++] = strdup("/usr/info");
}

static void addDocdir(char *dirname)
{
    if (docdir_count == 1024) {
	fprintf(stderr, "RPMERR_INTERNAL: Hit limit in addDocdir()\n");
	exit(RPMERR_INTERNAL);
    }
    docdirs[docdir_count++] = strdup(dirname);
}

static int isDoc(char *filename)
{
    int x = 0;

    while (x < docdir_count) {
        if (strstr(filename, docdirs[x]) == filename) {
	    return 1;
        }
	x++;
    }
    return 0;
}

/*************************************************************/
/*                                                           */
/* File stating / tree walk                                  */
/*                                                           */
/*************************************************************/

/* Need three globals to keep track of things in ftw() */
static int Gisdoc;
static int Gconf;
static int Gverify_flags;
static int Gcount;
static char *GPmode;
static char *GUname;
static char *GGname;
static struct file_entry **Gfestack;
static char *Gprefix;

static int add_file(struct file_entry **festack, const char *name,
		    int isdoc, int conf, int isdir, int verify_flags,
		    char *Pmode, char *Uname, char *Gname, char *prefix)
{
    struct file_entry *p;
    char *copyTo, copied;
    const char *prefixTest, *prefixPtr;
    const char *copyFrom;
    char fullname[1024];
    int mode;

    /* Set these up for ftw() */
    Gfestack = festack;
    Gisdoc = isdoc;
    Gconf = conf;
    Gverify_flags = verify_flags;
    GPmode = Pmode;
    GUname = Uname;
    GGname = Gname;
    Gprefix = prefix;

    p = malloc(sizeof(struct file_entry));

    copyTo = p->file;
    copied = '\0';
    copyFrom = name;
    while (*copyFrom) {
	if (*copyFrom != '/' || copied != '/') {
	    *copyTo++ = copied = *copyFrom;
	}
	copyFrom++;
    }
    *copyTo = '\0';

    p->isdoc = isdoc;
    p->conf = conf;
    p->isspecfile = 0;  /* source packages are done by hand */
    p->verify_flags = verify_flags;
    if (rpmGetVar(RPMVAR_ROOT)) {
	sprintf(fullname, "%s%s", rpmGetVar(RPMVAR_ROOT), name);
    } else {
	strcpy(fullname, name);
    }

    /* If we are using a prefix, validate the file */
    if (prefix) {
	prefixTest = name;
	prefixPtr = prefix;
	while (*prefixTest == '/') {
	    prefixTest++;  /* Skip leading "/" */
	}
	while (*prefixPtr && *prefixTest && (*prefixTest == *prefixPtr)) {
	    prefixPtr++;
	    prefixTest++;
	}
	if (*prefixPtr) {
	    rpmError(RPMERR_BADSPEC, "File doesn't match prefix (%s): %s",
		  prefix, name);
	    return 0;
	}
    }

    /* OK, finally stat() the file */
    if (lstat(fullname, &p->statbuf)) {
	return 0;
    }

    /*
     * If %attr() was specified, then use those values instead of
     * what lstat() returned.
     */
    if (Pmode && strcmp(Pmode, "-")) {
 	sscanf(Pmode, "%o", &mode);
 	mode |= p->statbuf.st_mode & S_IFMT;
 	p->statbuf.st_mode = (unsigned short)mode;
    }

    if (Uname && strcmp(Uname, "-")) {
 	p->uname = getUnameS(Uname);
    } else {
 	p->uname = getUname(p->statbuf.st_uid);
    }
    
    if (Gname && strcmp(Gname, "-")) {
 	p->gname = getGnameS(Gname);
    } else {
 	p->gname = getGname(p->statbuf.st_gid);
    }
    
    if (! (p->uname && p->gname)) {
	rpmError(RPMERR_BADSPEC, "Bad owner/group: %s\n", fullname);
	p->uname = "";
	p->gname = "";
	return 0;
    }
    
    if ((! isdir) && S_ISDIR(p->statbuf.st_mode)) {
	/* This means we need to descend with ftw() */
	Gcount = 0;
	
	/* We use our own ftw() call, because ftw() uses stat()    */
	/* instead of lstat(), which causes it to follow symlinks! */
	myftw(fullname, add_file_aux, 16);
	
	free(p);

	return Gcount;
    } else {
	/* Link it in */
	p->next = *festack;
	*festack = p;

	rpmMessage(RPMMESS_DEBUG, "ADDING: %s\n", name);

	/* return number of entries added */
	return 1;
    }
}

static int add_file_aux(const char *file, struct stat *sb, int flag)
{
    const char *name = file;

    if (rpmGetVar(RPMVAR_ROOT)) {
	name += strlen(rpmGetVar(RPMVAR_ROOT));
    }

    /* The 1 will cause add_file() to *not* descend */
    /* directories -- ftw() is already doing it!    */
    Gcount += add_file(Gfestack, name, Gisdoc, Gconf, 1, Gverify_flags,
			GPmode, GUname, GGname, Gprefix);

    return 0; /* for ftw() */
}

/*************************************************************/
/*                                                           */
/* globbing                                                  */
/*                                                           */
/*************************************************************/

/* glob_pattern_p() taken from bash
 * Copyright (C) 1985, 1988, 1989 Free Software Foundation, Inc.
 */

/* Return nonzero if PATTERN has any special globbing chars in it.  */
static int glob_pattern_p (char *pattern)
{
    register char *p = pattern;
    register char c;
    int open = 0;
  
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

static int glob_error(const char *foo, int bar)
{
    return 1;
}

/*************************************************************/
/*                                                           */
/* %attr parsing                                             */
/*                                                           */
/*************************************************************/

static int parseForAttr(char *buf, char **currPmode,
			char **currUname, char **currGname)
{
    char *p, *start, *end;
    char ourbuf[1024];
    int mode, x;

    if (!(p = start = strstr(buf, "%attr"))) {
	return 0;
    }

    *currPmode = *currUname = *currGname = NULL;

    p += 5;
    while (*p && (*p == ' ' || *p == '\t')) {
	p++;
    }

    if (*p != '(') {
	rpmError(RPMERR_BADSPEC, "Bad %%attr() syntax: %s", buf);
	return RPMERR_BADSPEC;
    }
    p++;

    end = p;
    while (*end && *end != ')') {
	end++;
    }

    if (! *end) {
	rpmError(RPMERR_BADSPEC, "Bad %%attr() syntax: %s", buf);
	return RPMERR_BADSPEC;
    }

    strncpy(ourbuf, p, end-p);
    ourbuf[end-p] = '\0';

    *currPmode = strtok(ourbuf, ", \n\t");
    *currUname = strtok(NULL, ", \n\t");
    *currGname = strtok(NULL, ", \n\t");

    if (! (*currPmode && *currUname && *currGname)) {
	rpmError(RPMERR_BADSPEC, "Bad %%attr() syntax: %s", buf);
	*currPmode = *currUname = *currGname = NULL;
	return RPMERR_BADSPEC;
    }

    /* Do a quick test on the mode argument */
    if (strcmp(*currPmode, "-")) {
	x = sscanf(*currPmode, "%o", &mode);
	if ((x == 0) || (mode >> 12)) {
	    rpmError(RPMERR_BADSPEC, "Bad %%attr() mode spec: %s", buf);
	    *currPmode = *currUname = *currGname = NULL;
	    return RPMERR_BADSPEC;
	}
    }
    
    *currPmode = strdup(*currPmode);
    *currUname = strdup(*currUname);
    *currGname = strdup(*currGname);
    
    /* Set everything we just parsed to blank spaces */
    while (start <= end) {
	*start++ = ' ';
    }

    return 0;
}

/*************************************************************/
/*                                                           */
/* %verify parsing                                           */
/*                                                           */
/*************************************************************/

static int parseForVerify(char *buf, int *verify_flags)
{
    char *p, *start, *end;
    char ourbuf[1024];
    int not;

    if (!(p = start = strstr(buf, "%verify"))) {
	return 0;
    }

    p += 7;
    while (*p && (*p == ' ' || *p == '\t')) {
	p++;
    }

    if (*p != '(') {
	rpmError(RPMERR_BADSPEC, "Bad %%verify() syntax: %s", buf);
	return RPMERR_BADSPEC;
    }
    p++;

    end = p;
    while (*end && *end != ')') {
	end++;
    }

    if (! *end) {
	rpmError(RPMERR_BADSPEC, "Bad %%verify() syntax: %s", buf);
	return RPMERR_BADSPEC;
    }

    strncpy(ourbuf, p, end-p);
    ourbuf[end-p] = '\0';
    while (start <= end) {
	*start++ = ' ';
    }

    p = strtok(ourbuf, ", \n\t");
    not = 0;
    *verify_flags = RPMVERIFY_NONE;
    while (p) {
	if (!strcmp(p, "not")) {
	    not = 1;
	} else if (!strcmp(p, "md5")) {
	    *verify_flags |= RPMVERIFY_MD5;
	} else if (!strcmp(p, "size")) {
	    *verify_flags |= RPMVERIFY_FILESIZE;
	} else if (!strcmp(p, "link")) {
	    *verify_flags |= RPMVERIFY_LINKTO;
	} else if (!strcmp(p, "user")) {
	    *verify_flags |= RPMVERIFY_USER;
	} else if (!strcmp(p, "group")) {
	    *verify_flags |= RPMVERIFY_GROUP;
	} else if (!strcmp(p, "mtime")) {
	    *verify_flags |= RPMVERIFY_MTIME;
	} else if (!strcmp(p, "mode")) {
	    *verify_flags |= RPMVERIFY_MODE;
	} else if (!strcmp(p, "rdev")) {
	    *verify_flags |= RPMVERIFY_RDEV;
	} else {
	    rpmError(RPMERR_BADSPEC, "Invalid %%verify token: %s", p);
	    return RPMERR_BADSPEC;
	}
	p = strtok(NULL, ", \n\t");
    }

    if (not) {
	*verify_flags = ~(*verify_flags);
    }

    return 0;
}

static int parseForConfig(char *buf, int *conf)
{
    char *p, *start, *end;
    char ourbuf[1024];

    if (!(p = start = strstr(buf, "%config"))) {
	return 0;
    }
    *conf = RPMFILE_CONFIG;

    p += 7;
    while (*p && (*p == ' ' || *p == '\t')) {
	p++;
    }

    if (*p != '(') {
	while (start < p) {
	    *start++ = ' ';
	}
	return 0;
    }
    p++;

    end = p;
    while (*end && *end != ')') {
	end++;
    }

    if (! *end) {
	rpmError(RPMERR_BADSPEC, "Bad %%config() syntax: %s", buf);
	return RPMERR_BADSPEC;
    }

    strncpy(ourbuf, p, end-p);
    ourbuf[end-p] = '\0';
    while (start <= end) {
	*start++ = ' ';
    }

    p = strtok(ourbuf, ", \n\t");
    while (p) {
	if (!strcmp(p, "missingok")) {
	    *conf |= RPMFILE_MISSINGOK;
	} else if (!strcmp(p, "noreplace")) {
	    *conf |= RPMFILE_NOREPLACE;
	} else {
	    rpmError(RPMERR_BADSPEC, "Invalid %%config token: %s", p);
	    return RPMERR_BADSPEC;
	}
	p = strtok(NULL, ", \n\t");
    }

    return 0;
}
