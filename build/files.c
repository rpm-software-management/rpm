/* RPM - Copyright (C) 1995 Red Hat Software
 * 
 * prepack.c - routines for packaging
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <ftw.h>
#include <glob.h>

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
#include "rpmerr.h"
#include "rpmlead.h"
#include "rpmlib.h"
#include "misc.h"

#define BINARY_HEADER 0
#define SOURCE_HEADER 1

struct file_entry {
    char file[1024];
    int isdoc;
    int isconf;
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
static int parseForAttr(char *origbuf, char **currPmode,
			char **currUname, char **currGname);

static void resetDocdir(void);
static void addDocdir(char *dirname);
static int isDoc(char *filename);

int process_filelist(Header header, struct PackageRec *pr,
		     StringBuf sb, int *size, char *name,
		     char *version, char *release, int type, char *prefix)
{
    char buf[1024];
    char **files, **fp;
    struct file_entry *fes, *fest;
    struct file_entry **file_entry_array;
    int isdoc, isconf, isdir, verify_flags;
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
    FILE *file;

    fes = NULL;
    *size = 0;

    resetDocdir();

    if (type == RPMLEAD_BINARY && pr->fileFile) {
	sprintf(buf, "%s/%s/%s", getVar(RPMVAR_BUILDDIR),
		build_subdir, pr->fileFile);
	message(MESS_DEBUG, "Reading file names from: %sXX\n", buf);
	if ((file = fopen(buf, "r")) == NULL) {
	    perror("open fileFile");
	    exit(1);
	}
	while (fgets(buf, sizeof(buf), file)) {
	    appendStringBuf(sb, buf);
	}
	fclose(file);
    }
    
    str = getStringBuf(sb);
    files = splitString(str, strlen(str), '\n');
    fp = files;

    while (*fp) {
	strcpy(buf, *fp);  /* temp copy */
	isdoc = 0;
	special_doc = 0;
	isconf = 0;
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
	verify_flags = VERIFY_ALL;
	filename = NULL;

	/* First preparse buf for %verify() */
	if (!parseForVerify(buf, &verify_flags)) {
	    return(RPMERR_BADSPEC);
	}
	
	/* Next parse for for %attr() */
	if (!parseForAttr(buf, &currPmode, &currUname, &currGname)) {
	    return(RPMERR_BADSPEC);
	}

	s = strtok(buf, " \t\n");
	while (s) {
	    if (!strcmp(s, "%docdir")) {
	        s = strtok(NULL, " \t\n");
		addDocdir(s);
		break;
	    } else if (!strcmp(s, "%doc")) {
		isdoc = 1;
	    } else if (!strcmp(s, "%config")) {
		isconf = 1;
	    } else if (!strcmp(s, "%dir")) {
		isdir = 1;
	    } else {
		if (isdoc && (*s != '/')) {
		    /* This is a special %doc macro */
		    special_doc = 1;
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
		if (filename || isconf || isdir) {
		    error(RPMERR_BADSPEC,
			  "Can't mix special %%doc with other forms: %s", fp);
		    return(RPMERR_BADSPEC);
		}
		sprintf(buf, "/usr/doc/%s-%s-%s", name, version, release);
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
		error(RPMERR_BADSPEC,
		      "File needs leading \"/\": %s", filename);
		return(RPMERR_BADSPEC);
	    }

	    if (glob_pattern_p(filename)) {
		char fullname[1024];

		if (getVar(RPMVAR_ROOT)) {
		    sprintf(fullname, "%s%s", getVar(RPMVAR_ROOT), filename);
		} else {
		    strcpy(fullname, filename);
		}

	        if (glob(fullname, 0, glob_error, &glob_result)) {
		    error(RPMERR_BADSPEC, "No matches: %s", fullname);
		    return(RPMERR_BADSPEC);
		}
		if (glob_result.gl_pathc < 1) {
		    error(RPMERR_BADSPEC, "No matches: %s", fullname);
		    return(RPMERR_BADSPEC);
		}
		x = 0;
		c = 0;
		while (x < glob_result.gl_pathc) {
		    int offset = strlen(getVar(RPMVAR_ROOT) ? : "");
		    c += add_file(&fes, &(glob_result.gl_pathv[x][offset]),
				  isdoc, isconf, isdir, verify_flags,
				  currPmode, currUname, currGname, prefix);
		    x++;
		}
		globfree(&glob_result);
	    } else {
	        c = add_file(&fes, filename, isdoc, isconf, isdir,
			     verify_flags, currPmode, currUname,
			     currGname, prefix);
	    }
	} else {
	    /* Source package are the simple case */
	    fest = malloc(sizeof(struct file_entry));
	    fest->isdoc = 0;
	    fest->isconf = 0;
	    fest->verify_flags = 0;  /* XXX - something else? */
	    stat(filename, &fest->statbuf);
	    fest->uname = getUname(fest->statbuf.st_uid);
	    fest->gname = getGname(fest->statbuf.st_gid);
	    strcpy(fest->file, filename);
	    fest->next = fes;
	    fes = fest;
	    c = 1;
	}
	    
	if (! c) {
	    error(RPMERR_BADSPEC, "File not found: %s", filename);
	    return(RPMERR_BADSPEC);
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
	
	c = 0;
	while (c < count) {
	    fest = file_entry_array[c];
	    if (type == RPMLEAD_BINARY) {
		fileList[c] = fest->file;
	    } else {
		fileList[c] = strrchr(fest->file, '/') + 1;
	    }
	    fileUnameList[c] = fest->uname;
	    fileGnameList[c] = fest->gname;
	    *size += fest->statbuf.st_size;
	    if (S_ISREG(fest->statbuf.st_mode)) {
		if (getVar(RPMVAR_ROOT)) {
		    sprintf(buf, "%s%s", getVar(RPMVAR_ROOT), fest->file);
		} else {
		    strcpy(buf, fest->file);
		}
		mdfile(buf, buf);
		fileMD5List[c] = strdup(buf);
		message(MESS_DEBUG, "md5(%s) = %s\n", fest->file, buf);
	    } else {
		/* This is stupid */
		fileMD5List[c] = strdup("");
	    }
	    fileSizeList[c] = fest->statbuf.st_size;
	    fileUIDList[c] = fest->statbuf.st_uid;
	    fileGIDList[c] = fest->statbuf.st_gid;
	    fileMtimesList[c] = fest->statbuf.st_mtime;
	    fileFlagsList[c] = 0;
	    if (isDoc(fest->file))
	        fileFlagsList[c] |= RPMFILE_DOC;
	    if (fest->isdoc) 
		fileFlagsList[c] |= RPMFILE_DOC;
	    if (fest->isconf)
		fileFlagsList[c] |= RPMFILE_CONFIG;

	    fileModesList[c] = fest->statbuf.st_mode;
	    fileRDevsList[c] = fest->statbuf.st_rdev;
	    fileVerifyFlagsList[c] = fest->verify_flags;

	    if (S_ISLNK(fest->statbuf.st_mode)) {
		if (getVar(RPMVAR_ROOT)) {
		    sprintf(buf, "%s%s", getVar(RPMVAR_ROOT), fest->file);
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
	addEntry(header, RPMTAG_FILENAMES, STRING_ARRAY_TYPE, fileList, c);
	addEntry(header, RPMTAG_FILELINKTOS, STRING_ARRAY_TYPE,
		 fileLinktoList, c);
	addEntry(header, RPMTAG_FILEMD5S, STRING_ARRAY_TYPE, fileMD5List, c);
	addEntry(header, RPMTAG_FILESIZES, INT32_TYPE, fileSizeList, c);
	addEntry(header, RPMTAG_FILEUIDS, INT32_TYPE, fileUIDList, c);
	addEntry(header, RPMTAG_FILEGIDS, INT32_TYPE, fileGIDList, c);
	addEntry(header, RPMTAG_FILEUSERNAME, STRING_ARRAY_TYPE,
		 fileUnameList, c);
	addEntry(header, RPMTAG_FILEGROUPNAME, STRING_ARRAY_TYPE,
		 fileGnameList, c);
	addEntry(header, RPMTAG_FILEMTIMES, INT32_TYPE, fileMtimesList, c);
	addEntry(header, RPMTAG_FILEFLAGS, INT32_TYPE, fileFlagsList, c);
	addEntry(header, RPMTAG_FILEMODES, INT16_TYPE, fileModesList, c);
	addEntry(header, RPMTAG_FILERDEVS, INT16_TYPE, fileRDevsList, c);
	addEntry(header, RPMTAG_FILEVERIFYFLAGS, INT32_TYPE,
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
    return 0;
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
static int Gisconf;
static int Gverify_flags;
static int Gcount;
static char *GPmode;
static char *GUname;
static char *GGname;
static struct file_entry **Gfestack;
static char *Gprefix;

static int add_file(struct file_entry **festack, const char *name,
		    int isdoc, int isconf, int isdir, int verify_flags,
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
    Gisconf = isconf;
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
    p->isconf = isconf;
    p->verify_flags = verify_flags;
    if (getVar(RPMVAR_ROOT)) {
	sprintf(fullname, "%s%s", getVar(RPMVAR_ROOT), name);
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
	    error(RPMERR_BADSPEC, "File doesn't match prefix (%s): %s",
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

	message(MESS_DEBUG, "ADDING: %s\n", name);

	/* return number of entries added */
	return 1;
    }
}

static int add_file_aux(const char *file, struct stat *sb, int flag)
{
    const char *name = file;

    if (getVar(RPMVAR_ROOT)) {
	name += strlen(getVar(RPMVAR_ROOT));
    }

    /* The 1 will cause add_file() to *not* descend */
    /* directories -- ftw() is already doing it!    */
    Gcount += add_file(Gfestack, name, Gisdoc, Gisconf, 1, Gverify_flags,
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
	return 1;
    }

    *currPmode = *currUname = *currGname = NULL;

    p += 5;
    while (*p && (*p == ' ' || *p == '\t')) {
	p++;
    }

    if (*p != '(') {
	error(RPMERR_BADSPEC, "Bad %%attr() syntax: %s", buf);
	return 0;
    }
    p++;

    end = p;
    while (*end && *end != ')') {
	end++;
    }

    if (! *end) {
	error(RPMERR_BADSPEC, "Bad %%attr() syntax: %s", buf);
	return 0;
    }

    strncpy(ourbuf, p, end-p);
    ourbuf[end-p] = '\0';

    *currPmode = strtok(ourbuf, ", \n\t");
    *currUname = strtok(NULL, ", \n\t");
    *currGname = strtok(NULL, ", \n\t");

    if (! (*currPmode && *currUname && *currGname)) {
	error(RPMERR_BADSPEC, "Bad %%attr() syntax: %s", buf);
	*currPmode = *currUname = *currGname = NULL;
	return 0;
    }

    /* Do a quick test on the mode argument */
    if (strcmp(*currPmode, "-")) {
	x = sscanf(*currPmode, "%o", &mode);
	if ((x == 0) || (mode >> 12)) {
	    error(RPMERR_BADSPEC, "Bad %%attr() mode spec: %s", buf);
	    *currPmode = *currUname = *currGname = NULL;
	    return 0;
	}
    }
    
    *currPmode = strdup(*currPmode);
    *currUname = strdup(*currUname);
    *currGname = strdup(*currGname);
    
    /* Set everything we just parsed to blank spaces */
    while (start <= end) {
	*start++ = ' ';
    }

    return 1;
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
	return 1;
    }

    p += 7;
    while (*p && (*p == ' ' || *p == '\t')) {
	p++;
    }

    if (*p != '(') {
	error(RPMERR_BADSPEC, "Bad %%verify() syntax: %s", buf);
	return 0;
    }
    p++;

    end = p;
    while (*end && *end != ')') {
	end++;
    }

    if (! *end) {
	error(RPMERR_BADSPEC, "Bad %%verify() syntax: %s", buf);
	return 0;
    }

    strncpy(ourbuf, p, end-p);
    ourbuf[end-p] = '\0';
    while (start <= end) {
	*start++ = ' ';
    }

    p = strtok(ourbuf, ", \n\t");
    not = 0;
    *verify_flags = VERIFY_NONE;
    while (p) {
	if (!strcmp(p, "not")) {
	    not = 1;
	} else if (!strcmp(p, "md5")) {
	    *verify_flags |= VERIFY_MD5;
	} else if (!strcmp(p, "size")) {
	    *verify_flags |= VERIFY_FILESIZE;
	} else if (!strcmp(p, "link")) {
	    *verify_flags |= VERIFY_LINKTO;
	} else if (!strcmp(p, "user")) {
	    *verify_flags |= VERIFY_USER;
	} else if (!strcmp(p, "group")) {
	    *verify_flags |= VERIFY_GROUP;
	} else if (!strcmp(p, "mtime")) {
	    *verify_flags |= VERIFY_MTIME;
	} else if (!strcmp(p, "mode")) {
	    *verify_flags |= VERIFY_MODE;
	} else if (!strcmp(p, "rdev")) {
	    *verify_flags |= VERIFY_RDEV;
	} else {
	    error(RPMERR_BADSPEC, "Invalid %%verify token: %s", p);
	    return 0;
	}
	p = strtok(NULL, ", \n\t");
    }

    if (not) {
	*verify_flags = ~(*verify_flags);
    }

    return 1;
}
