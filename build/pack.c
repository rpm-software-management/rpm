/* RPM - Copyright (C) 1995 Red Hat Software
 * 
 * pack.c - routines for packaging
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <ftw.h>
#include <netinet/in.h>
#include <pwd.h>
#include <grp.h>
#include <netdb.h>
#include <glob.h>
#include <zlib.h>

#include "header.h"
#include "specP.h"
#include "rpmerr.h"
#include "rpmlead.h"
#include "rpmlib.h"
#include "signature.h"
#include "misc.h"
#include "pack.h"
#include "messages.h"
#include "md5.h"

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

static int cpio_gzip(Header header, int fd, char *tempdir, int *archiveSize);
static int writeMagic(int fd, char *name, unsigned short type,
		      unsigned short sigtype);
static int add_file(struct file_entry **festack, const char *name,
		    int isdoc, int isconf, int isdir, int verify_flags);
static int compare_fe(const void *ap, const void *bp);
static int process_filelist(Header header, StringBuf sb, int *size,
			    char *name, char *version, char *release,
			    int type);
static char *buildHost(void);
static int add_file_aux(const char *file, struct stat *sb, int flag);
static char *getUname(uid_t uid);
static char *getGname(gid_t gid);
static int glob_error(const char *foo, int bar);
static int glob_pattern_p (char *pattern);
static int parseForVerify(char *buf, int *verify_flags);

static
int generateRPM(char *name,       /* name-version-release         */
		int type,         /* source or binary             */
		Header header,    /* the header                   */
		char *stempdir);  /* directory containing sources */

static void resetDocdir(void);
static void addDocdir(char *dirname);
static int isDoc(char *filename);

int generateRPM(char *name,       /* name-version-release         */
		int type,         /* source or binary             */
		Header header,    /* the header                   */
		char *stempdir)   /* directory containing sources */
{
    unsigned short sigtype;
    char *archName;
    char filename[1024];
    char *sigtarget, *archiveTemp;
    int fd, ifd, count, archiveSize;
    unsigned char buffer[8192];

    /* Figure out the signature type */
    if ((sigtype = sigLookupType()) == RPMSIG_BAD) {
	error(RPMERR_BADSIGTYPE, "Bad signature type in rpmrc");
	return RPMERR_BADSIGTYPE;
    }

    /* Make the output RPM filename */
    if (type == RPMLEAD_SOURCE) {
	sprintf(filename, "%s/%s.src.rpm", getVar(RPMVAR_SRPMDIR), name);
    } else {
	archName = getArchName();
	sprintf(filename, "%s/%s/%s.%s.rpm", getVar(RPMVAR_RPMDIR),
		archName, name, archName);
    }

    /* Write the archive to a temp file so we can get the size */
    archiveTemp = tempnam("/usr/tmp", "rpmbuild");
    fd = open(archiveTemp, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (cpio_gzip(header, fd, stempdir, &archiveSize)) {
	return 1;
    }
    close(fd);

    /* Add the archive size to the Header */
    addEntry(header, RPMTAG_ARCHIVESIZE, INT32_TYPE, &archiveSize, 1);
    
    /* Now write the header and append the archive */
    sigtarget = tempnam("/usr/tmp", "rpmbuild");
    fd = open(sigtarget, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    writeHeader(fd, header);
    ifd = open(archiveTemp, O_RDONLY, 0644);
    while ((count = read(ifd, buffer, sizeof(buffer))) > 0) {
	write(fd, buffer, count);
    }
    close(ifd);
    close(fd);
    unlink(archiveTemp);

    /* Now write the lead */
    fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (writeMagic(fd, name, type, sigtype)) {
	return 1;
    }

    /* Generate the signature */
    message(MESS_VERBOSE, "Generating signature: %d\n", sigtype);
    fflush(stdout);
    makeSignature(sigtarget, sigtype, fd);

    /* Append the header and archive */
    ifd = open(sigtarget, O_RDONLY);
    while ((count = read(ifd, buffer, sizeof(buffer))) > 0) {
	write(fd, buffer, count);
    }
    close(ifd);
    close(fd);
    unlink(sigtarget);
    
    return 0;
}

static int writeMagic(int fd, char *name,
		      unsigned short type,
		      unsigned short sigtype)
{
    struct rpmlead lead;

    lead.major = 2;
    lead.minor = 0;
    lead.type = type;
    lead.archnum = getArchNum();
    lead.osnum = getOsNum();
    lead.signature_type = sigtype;
    strncpy(lead.name, name, sizeof(lead.name));

    writeLead(fd, &lead);

    return 0;
}

static int cpio_gzip(Header header, int fd, char *tempdir, int *archiveSize)
{
    char **f, *s;
    int count;
    FILE *inpipeF;
    int cpioPID;
    int inpipe[2];
    int outpipe[2];
    int status;
    gzFile zFile;
    void *oldhandler;
    int cpioDead;
    int bytes;
    unsigned char buf[8192];

    *archiveSize = 0;
    
    pipe(inpipe);
    pipe(outpipe);
    
    oldhandler = signal(SIGPIPE, SIG_IGN);

    if (!(cpioPID = fork())) {
	close(0);
	close(1);
	close(inpipe[1]);
	close(outpipe[0]);
	close(fd);
	
	dup2(inpipe[0], 0);  /* Make stdin the in pipe */
	dup2(outpipe[1], 1); /* Make stdout the out pipe */

	if (tempdir) {
	    chdir(tempdir);
	} else if (getVar(RPMVAR_ROOT)) {
	    if (chdir(getVar(RPMVAR_ROOT))) {
		error(RPMERR_EXEC, "Couldn't chdir to %s",
		      getVar(RPMVAR_ROOT));
		exit(RPMERR_EXEC);
	    }
	} else {
	    chdir("/");
	}

	execlp("cpio", "cpio",
	       (isVerbose()) ? "-ov" : "-o",
	       (tempdir) ? "-LH" : "-H",
	       "crc", NULL);
	error(RPMERR_EXEC, "Couldn't exec cpio");
	exit(RPMERR_EXEC);
    }
    if (cpioPID < 0) {
	error(RPMERR_FORK, "Couldn't fork");
	return RPMERR_FORK;
    }

    close(inpipe[0]);
    close(outpipe[1]);
    fcntl(outpipe[0], F_SETFL, O_NONBLOCK);

    /* XXX - Unfortunately, this only does default (level 6) comrpession */
    zFile = gzdopen(fd, "w");

    inpipeF = fdopen(inpipe[1], "w");
    if (!getEntry(header, RPMTAG_FILENAMES, NULL, (void **) &f, &count)) {
	/* count may already be 0, but this is safer */
	count = 0;
    }
    
    cpioDead = 0;
    do {
	if (waitpid(cpioPID, &status, WNOHANG)) {
	    cpioDead = 1;
	}

	/* Write a file to the cpio process */
	if (count) {
	    s = *f++;
	    fprintf(inpipeF, "%s\n", (tempdir) ? s : (s+1));
	    count--;
	} else {
	    fclose(inpipeF);
	}
	
	/* Read any data from cpio, write it to the output fd */
	bytes = read(outpipe[0], buf, sizeof(buf));
	while (bytes > 0) {
	    *archiveSize += bytes;
	    gzwrite(zFile, buf, bytes);
	    bytes = read(outpipe[0], buf, sizeof(buf));
	}

    } while (!cpioDead);

    close(inpipe[1]);
    close(outpipe[0]);
    gzclose(zFile);
    
    signal(SIGPIPE, oldhandler);
    waitpid(cpioPID, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	error(RPMERR_CPIO, "cpio failed");
	return 1;
    }

    return 0;
}

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

static char *getUname(uid_t uid)
{
    static uid_t uids[1024];
    static char *unames[1024];
    static int used = 0;
    
    struct passwd *pw;
    int x;

    x = 0;
    while (x < used) {
	if (uids[x] == uid) {
	    return unames[x];
	}
	x++;
    }

    /* XXX - This is the other hard coded limit */
    if (x == 1024) {
	fprintf(stderr, "RPMERR_INTERNAL: Hit limit in getUname()\n");
	exit(RPMERR_INTERNAL);
    }
    
    pw = getpwuid(uid);
    uids[x] = uid;
    used++;
    if (pw) {
	unames[x] = strdup(pw->pw_name);
    } else {
	unames[x] = "";
    }
    return unames[x];
}

static char *getGname(gid_t gid)
{
    static gid_t gids[1024];
    static char *gnames[1024];
    static int used = 0;
    
    struct group *gr;
    int x;

    x = 0;
    while (x < used) {
	if (gids[x] == gid) {
	    return gnames[x];
	}
	x++;
    }

    /* XXX - This is the other hard coded limit */
    if (x == 1024) {
	fprintf(stderr, "RPMERR_INTERNAL: Hit limit in getGname()\n");
	exit(RPMERR_INTERNAL);
    }
    
    gr = getgrgid(gid);
    gids[x] = gid;
    used++;
    if (gr) {
	gnames[x] = strdup(gr->gr_name);
    } else {
	gnames[x] = "";
    }
    return gnames[x];
}

/* Need three globals to keep track of things in ftw() */
static int Gisdoc;
static int Gisconf;
static int Gverify_flags;
static int Gcount;
static struct file_entry **Gfestack;

static int add_file(struct file_entry **festack, const char *name,
		    int isdoc, int isconf, int isdir, int verify_flags)
{
    struct file_entry *p;
    char fullname[1024];

    /* Set these up for ftw() */
    Gfestack = festack;
    Gisdoc = isdoc;
    Gisconf = isconf;
    Gverify_flags = verify_flags;

    p = malloc(sizeof(struct file_entry));
    strcpy(p->file, name);
    p->isdoc = isdoc;
    p->isconf = isconf;
    p->verify_flags = verify_flags;
    if (getVar(RPMVAR_ROOT)) {
	sprintf(fullname, "%s%s", getVar(RPMVAR_ROOT), name);
    } else {
	strcpy(fullname, name);
    }
    if (lstat(fullname, &p->statbuf)) {
	return 0;
    }
    p->uname = getUname(p->statbuf.st_uid);
    p->gname = getGname(p->statbuf.st_gid);

    if ((! isdir) && S_ISDIR(p->statbuf.st_mode)) {
	/* This means we need to decend with ftw() */
	Gcount = 0;

	ftw(fullname, add_file_aux, 16);
	
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
    Gcount += add_file(Gfestack, name, Gisdoc, Gisconf, 1, Gverify_flags);

    return 0; /* for ftw() */
}

static int compare_fe(const void *ap, const void *bp)
{
    char *a, *b;

    a = (*(struct file_entry **)ap)->file;
    b = (*(struct file_entry **)bp)->file;

    return strcmp(a, b);
}

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

static int parseForVerify(char *buf, int *verify_flags)
{
    char *p, *start, *end;
    char ourbuf[1024];
    int not;

    if (!(p = start = strstr(buf, "%verify"))) {
	return 1;
    }

    p += 7;
    while (*p && *p == ' ' && *p == '\t') {
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

static int process_filelist(Header header, StringBuf sb, int *size,
			    char *name, char *version, char *release, int type)
{
    char buf[1024];
    char **files, **fp;
    struct file_entry *fes, *fest;
    struct file_entry **file_entry_array;
    int isdoc, isconf, isdir, verify_flags;
    char *filename, *s;
    char *str;
    int count = 0;
    int c, x;
    glob_t glob_result;
    int special_doc;
    int passed_special_doc = 0;

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
	isconf = 0;
	isdir = 0;
	verify_flags = VERIFY_ALL;
	filename = NULL;

	/* First preparse buf for %verify() */
	if (!parseForVerify(buf, &verify_flags)) {
	    return(RPMERR_BADSPEC);
	}
	
	s = strtok(buf, " \t\n");
	while (s) {
	    if (!strcmp(s, "%doc")) {
		isdoc = 1;
	    } else if (!strcmp(s, "%config")) {
		isconf = 1;
	    } else if (!strcmp(s, "%dir")) {
		isdir = 1;
	    } else if (!strcmp(s, "%docdir")) {
	        s = strtok(NULL, " \t\n");
		addDocdir(s);
		break;
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
		sprintf(buf, "%s/%s-%s-%s", getVar(RPMVAR_DOCDIR),
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
		error(RPMERR_BADSPEC,
		      "File needs leading \"/\": %s", filename);
		return(RPMERR_BADSPEC);
	    }

	    if (glob_pattern_p(filename)) {
	        if (glob(filename, 0, glob_error, &glob_result)) {
		    error(RPMERR_BADSPEC,
			  "No matches: %s", filename);
		    return(RPMERR_BADSPEC);
		}
		if (glob_result.gl_pathc < 1) {
		    error(RPMERR_BADSPEC,
			  "No matches: %s", filename);
		    return(RPMERR_BADSPEC);
		}
		x = 0;
		c = 0;
		while (x < glob_result.gl_pathc) {
		    c += add_file(&fes, glob_result.gl_pathv[x],
				  isdoc, isconf, isdir, verify_flags);
		    x++;
		}
		globfree(&glob_result);
	    } else {
	        c = add_file(&fes, filename, isdoc, isconf,
			     isdir, verify_flags);
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
		mdfile(fest->file, buf);
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
		readlink(buf, buf, 1024);
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

static time_t buildtime;
void markBuildTime(void)
{
    buildtime = time(NULL);
}

static char *buildHost(void)
{
    static char hostname[1024];
    static int gotit = 0;
    struct hostent *hbn;

    if (! gotit) {
        gethostname(hostname, sizeof(hostname));
	hbn = gethostbyname(hostname);
	strcpy(hostname, hbn->h_name);
	gotit = 1;
    }
    return(hostname);
}

int packageBinaries(Spec s)
{
    char name[1024];
    char filename[1024];
    char sourcerpm[1024];
    char *icon;
    int iconFD;
    struct stat statbuf;
    struct PackageRec *pr;
    Header outHeader;
    HeaderIterator headerIter;
    int_32 tag, type, c;
    void *ptr;
    char *version;
    char *release;
    char *vendor;
    char *dist;
    int size;
    int_8 os, arch;
    
    if (!getEntry(s->packages->header, RPMTAG_VERSION, NULL,
		  (void *) &version, NULL)) {
	error(RPMERR_BADSPEC, "No version field");
	return RPMERR_BADSPEC;
    }
    if (!getEntry(s->packages->header, RPMTAG_RELEASE, NULL,
		  (void *) &release, NULL)) {
	error(RPMERR_BADSPEC, "No release field");
	return RPMERR_BADSPEC;
    }

    sprintf(sourcerpm, "%s-%s-%s.src.rpm", s->name, version, release);

    vendor = NULL;
    if (!isEntry(s->packages->header, RPMTAG_VENDOR)) {
	vendor = getVar(RPMVAR_VENDOR);
    }
    dist = NULL;
    if (!isEntry(s->packages->header, RPMTAG_DISTRIBUTION)) {
	dist = getVar(RPMVAR_DISTRIBUTION);
    }
    
    /* Look through for each package */
    pr = s->packages;
    while (pr) {
	/* A file count of -1 means no package */
	if (pr->files == -1) {
	    pr = pr->next;
	    continue;
	}
	
	/* Figure out the name of this package */
	if (pr->subname) {
	    strcpy(name, s->name);
	    strcat(name, "-");
	    strcat(name, pr->subname);
	} else if (pr->newname) {
	    strcpy(name, pr->newname);
	} else {
	    strcpy(name, s->name);
	}
	strcat(name, "-");
	strcat(name, version);
	strcat(name, "-");
	strcat(name, release);

	/**** Generate the Header ****/
	
	/* Here's the plan: copy the package's header,  */
	/* then add entries from the primary header     */
	/* that don't already exist.                    */
	outHeader = copyHeader(pr->header);
	headerIter = initIterator(s->packages->header);
	while (nextIterator(headerIter, &tag, &type, &ptr, &c)) {
	    /* Some tags we don't copy */
	    switch (tag) {
	      case RPMTAG_PREIN:
	      case RPMTAG_POSTIN:
	      case RPMTAG_PREUN:
	      case RPMTAG_POSTUN:
		  continue;
		  break;  /* Shouldn't need this */
	      default:
		  if (! isEntry(outHeader, tag)) {
		      addEntry(outHeader, tag, type, ptr, c);
		  }
	    }
	}
	freeIterator(headerIter);
	
	/* Add some final entries to the header */
	os = getArchNum();
	arch = getArchNum();
	addEntry(outHeader, RPMTAG_OS, INT8_TYPE, &os, 1);
	addEntry(outHeader, RPMTAG_ARCH, INT8_TYPE, &arch, 1);
	addEntry(outHeader, RPMTAG_BUILDTIME, INT32_TYPE, &buildtime, 1);
	addEntry(outHeader, RPMTAG_BUILDHOST, STRING_TYPE, buildHost(), 1);
	addEntry(outHeader, RPMTAG_SOURCERPM, STRING_TYPE, sourcerpm, 1);
	if (pr->icon) {
	    sprintf(filename, "%s/%s", getVar(RPMVAR_SOURCEDIR), pr->icon);
	    stat(filename, &statbuf);
	    icon = malloc(statbuf.st_size);
	    iconFD = open(filename, O_RDONLY, 0644);
	    read(iconFD, icon, statbuf.st_size);
	    close(iconFD);
	    if (! strncmp(icon, "GIF", 3)) {
		addEntry(outHeader, RPMTAG_GIF, BIN_TYPE,
			 icon, statbuf.st_size);
	    } else if (! strncmp(icon, "/* XPM", 6)) {
		addEntry(outHeader, RPMTAG_XPM, BIN_TYPE,
			 icon, statbuf.st_size);
	    } else {
	       error(RPMERR_BADSPEC, "Unknown icon type");
	       return 1;
	    }
	    free(icon);
	}
	if (vendor) {
	    addEntry(outHeader, RPMTAG_VENDOR, STRING_TYPE, vendor, 1);
	}
	if (dist) {
	    addEntry(outHeader, RPMTAG_DISTRIBUTION, STRING_TYPE, dist, 1);
	}
	
	/**** Process the file list ****/
	
	if (process_filelist(outHeader, pr->filelist, &size,
			     s->name, version, release, RPMLEAD_BINARY)) {
	    return 1;
	}
	/* And add the final Header entry */
	addEntry(outHeader, RPMTAG_SIZE, INT32_TYPE, &size, 1);

	/**** Make the RPM ****/

	generateRPM(name, RPMLEAD_BINARY, outHeader, NULL);

	freeHeader(outHeader);
	pr = pr->next;
    }
	
    return 0;
}

/**************** SOURCE PACKAGING ************************/

int packageSource(Spec s)
{
    struct sources *source;
    struct PackageRec *package;
    char *tempdir;
    char src[1024], dest[1024], fullname[1024];
    char *version;
    char *release;
    char *vendor;
    char *dist;
    Header outHeader;
    StringBuf filelist;
    int size;
    int_8 os, arch;
    char **sources;
    char **patches;
    int scount, pcount;

    /**** Create links for all the sources ****/
    
    tempdir = tempnam("/usr/tmp", "rpmbuild");
    mkdir(tempdir, 0700);

    filelist = newStringBuf();

    sources = malloc((s->numSources+1) * sizeof(char *));
    patches = malloc((s->numPatches+1) * sizeof(char *));
    
    /* Link in the spec file and all the sources */
    sprintf(dest, "%s%s", tempdir, strrchr(s->specfile, '/'));
    symlink(s->specfile, dest);
    appendLineStringBuf(filelist, dest);
    source = s->sources;
    scount = 0;
    pcount = 0;
    while (source) {
	sprintf(src, "%s/%s", getVar(RPMVAR_SOURCEDIR), source->source);
	sprintf(dest, "%s/%s", tempdir, source->source);
	symlink(src, dest);
	appendLineStringBuf(filelist, dest);
	if (source->ispatch) {
	    patches[pcount++] = source->fullSource;
	} else {
	    sources[scount++] = source->fullSource;
	}
	source = source->next;
    }
    /* ... and icons */
    package = s->packages;
    while (package) {
	if (package->icon) {
	    sprintf(src, "%s/%s", getVar(RPMVAR_SOURCEDIR), package->icon);
	    sprintf(dest, "%s/%s", tempdir, package->icon);
	    appendLineStringBuf(filelist, dest);
	    symlink(src, dest);
	}
	package = package->next;
    }

    /**** Generate the Header ****/
    
    if (!getEntry(s->packages->header, RPMTAG_VERSION, NULL,
		  (void *) &version, NULL)) {
	error(RPMERR_BADSPEC, "No version field");
	return RPMERR_BADSPEC;
    }
    if (!getEntry(s->packages->header, RPMTAG_RELEASE, NULL,
		  (void *) &release, NULL)) {
	error(RPMERR_BADSPEC, "No release field");
	return RPMERR_BADSPEC;
    }

    outHeader = copyHeader(s->packages->header);
    os = getArchNum();
    arch = getArchNum();
    addEntry(outHeader, RPMTAG_OS, INT8_TYPE, &os, 1);
    addEntry(outHeader, RPMTAG_ARCH, INT8_TYPE, &arch, 1);
    addEntry(outHeader, RPMTAG_BUILDTIME, INT32_TYPE, &buildtime, 1);
    addEntry(outHeader, RPMTAG_BUILDHOST, STRING_TYPE, buildHost(), 1);
    if (scount) 
        addEntry(outHeader, RPMTAG_SOURCE, STRING_ARRAY_TYPE, sources, scount);
    if (pcount)
        addEntry(outHeader, RPMTAG_PATCH, STRING_ARRAY_TYPE, patches, pcount);
    if (!isEntry(s->packages->header, RPMTAG_VENDOR)) {
	if ((vendor = getVar(RPMVAR_VENDOR))) {
	    addEntry(outHeader, RPMTAG_VENDOR, STRING_TYPE, vendor, 1);
	}
    }
    if (!isEntry(s->packages->header, RPMTAG_DISTRIBUTION)) {
	if ((dist = getVar(RPMVAR_DISTRIBUTION))) {
	    addEntry(outHeader, RPMTAG_DISTRIBUTION, STRING_TYPE, dist, 1);
	}
    }

    /* Process the file list */
    if (process_filelist(outHeader, filelist, &size,
			 s->name, version, release, RPMLEAD_SOURCE)) {
	return 1;
    }
    /* And add the final Header entry */
    addEntry(outHeader, RPMTAG_SIZE, INT32_TYPE, &size, 1);

    /**** Make the RPM ****/

    sprintf(fullname, "%s-%s-%s", s->name, version, release);
    generateRPM(fullname, RPMLEAD_SOURCE, outHeader, tempdir);
    
    /**** Now clean up ****/

    freeStringBuf(filelist);
    
    source = s->sources;
    while (source) {
	sprintf(dest, "%s/%s", tempdir, source->source);
	unlink(dest);
	source = source->next;
    }
    package = s->packages;
    while (package) {
	if (package->icon) {
	    sprintf(dest, "%s/%s", tempdir, package->icon);
	    unlink(dest);
	}
	package = package->next;
    }
    sprintf(dest, "%s%s", tempdir, strrchr(s->specfile, '/'));
    unlink(dest);
    rmdir(tempdir);
    
    return 0;
}

/****************** Source Removal ********************/

int doRmSource(Spec s)
{
    char filename[1024];
    struct sources *source;
    struct PackageRec *package;

    /* spec file */
    sprintf(filename, "%s%s", getVar(RPMVAR_SPECDIR),
	    strrchr(s->specfile, '/'));
    unlink(filename);

    /* sources and patches */
    source = s->sources;
    while (source) {
	sprintf(filename, "%s/%s", getVar(RPMVAR_SOURCEDIR), source->source);
	unlink(filename);
	source = source->next;
    }

    /* icons */
    package = s->packages;
    while (package) {
	if (package->icon) {
	    sprintf(filename, "%s/%s", getVar(RPMVAR_SOURCEDIR),
		    package->icon);
	    unlink(filename);
	}
	package = package->next;
    }
    
    return 0;
}
