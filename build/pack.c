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
#include <string.h>
#include <fcntl.h>
#include <time.h>

#include "header.h"
#include "specP.h"
#include "rpmlib.h"
#include "rpmerr.h"
#include "misc.h"
#include "pack.h"
#include "messages.h"
#include "md5.h"

#define BINARY_HEADER 0
#define SOURCE_HEADER 1

static unsigned char magic[] = { 0xed, 0xab, 0xee, 0xdb };

static unsigned char arch_num;
static unsigned char os_num;
static char arch_token[40];

struct file_entry {
    char file[1024];
    int isdoc;
    int isconf;
    struct stat statbuf;
    struct file_entry *next;
};

static int cpio_gzip(Header header, int fd);
static int writeMagic(Spec s, struct PackageRec *pr,
		      int fd, char *name, unsigned char type);
static int add_file(struct file_entry **festack,
		    char *name, int isdoc, int isconf, int isdir);
static int process_filelist(Header header, StringBuf sb);

static int init_pack(void)
{
    static int pack_initialized = 0;
    struct utsname un;

    if (! pack_initialized) {
	uname(&un);
	if (!strcmp(un.sysname, "Linux")) {
	    os_num = 1;
	} else {
	    error(RPMERR_UNKNOWNOS, "Don't recogzine OS: %s", un.sysname);
	    return RPMERR_UNKNOWNOS;
	}

	if ((!strcmp(un.machine, "i386")) ||
	    (!strcmp(un.machine, "i486")) ||
	    (!strcmp(un.machine, "i586")) ||
	    (!strcmp(un.machine, "i686"))) {
	    arch_num = 1;
	    strcpy(arch_token, "i386");
	} else if (!strcmp(un.machine, "alpha")) {
	    arch_num = 2;
	    strcpy(arch_token, "axp");
	} else if (!strcmp(un.machine, "sparc")) {
	    arch_num = 3;
	    strcpy(arch_token, "sparc");
	} else if (!strcmp(un.machine, "IP2")) {
	    arch_num = 4;
	    strcpy(arch_token, "mips");
	} else {
	    error(RPMERR_UNKNOWNARCH, "Don't recogzine arch: %s", un.machine);
	    return RPMERR_UNKNOWNARCH;
	}

    }

    return 0;
}

static int writeMagic(Spec s, struct PackageRec *pr,
		      int fd, char *name, unsigned char type)
{
    char header[74];

    header[0] = 2; /* major */
    header[1] = 0; /* minor */
    header[2] = 0;
    header[3] = type;
    header[4] = 0;
    header[5] = arch_num;
    strncpy(&(header[6]), name, 66);
    header[72] = 0;
    header[73] = os_num;
    
    write(fd, &magic, 4);
    write(fd, &header, 74);

    return 0;
}

static int cpio_gzip(Header header, int fd)
{
    char **f;
    int count;
    FILE *inpipeF;
    int cpioPID, gzipPID;
    int inpipe[2];
    int outpipe[2];
    int status;

    pipe(inpipe);
    pipe(outpipe);
    
    if (!(cpioPID = fork())) {
	close(0);
	close(1);
	close(inpipe[1]);
	close(outpipe[0]);
	close(fd);
	
	dup2(inpipe[0], 0);  /* Make stdin the in pipe */
	dup2(outpipe[1], 1); /* Make stdout the out pipe */

	execlp("cpio", "cpio", "-ovLH", "crc", NULL);
	error(RPMERR_EXEC, "Couldn't exec cpio");
	exit(RPMERR_EXEC);
    }
    if (cpioPID < 0) {
	error(RPMERR_FORK, "Couldn't fork");
	return RPMERR_FORK;
    }

    if (!(gzipPID = fork())) {
	close(0);
	close(1);
	close(inpipe[1]);
	close(inpipe[0]);
	close(outpipe[1]);

	dup2(outpipe[0], 0); /* Make stdin the out pipe */
	dup2(fd, 1);         /* Make stdout the passed-in file descriptor */
	close(fd);

	execlp("gzip", "gzip", "-c9fn", NULL);
	error(RPMERR_EXEC, "Couldn't exec gzip");
	exit(RPMERR_EXEC);
    }
    if (gzipPID < 0) {
	error(RPMERR_FORK, "Couldn't fork");
	return RPMERR_FORK;
    }

    close(inpipe[0]);
    close(outpipe[1]);
    close(outpipe[0]);

    if (getEntry(header, RPMTAG_FILENAMES, NULL, (void **) &f, &count)) {
	inpipeF = fdopen(inpipe[1], "w");
	while (count--) {
	    fprintf(inpipeF, "%s\n", *f);
	    f++;
	}
	fclose(inpipeF);
    } else {
	close(inpipe[1]);
    }

    waitpid(cpioPID, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	error(RPMERR_CPIO, "cpio failed");
	return 1;
    }
    waitpid(gzipPID, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	error(RPMERR_GZIP, "gzip failed");
	return 1;
    }

    return 0;
}

static int add_file(struct file_entry **festack,
		    char *name, int isdoc, int isconf, int isdir)
{
    struct file_entry *p;

    /* XXX do globbing and %dir %doc %config expansion here */
    p = malloc(sizeof(struct file_entry));
    strcpy(p->file, name);
    p->isdoc = isdoc;
    p->isconf = isconf;
    stat(name, &p->statbuf);
    p->next = *festack;
    *festack = p;

    message(MESS_DEBUG, "ADDING: %s\n", name);
    /* return number of entries added */
    return 1;
}

static int process_filelist(Header header, StringBuf sb)
{
    char buf[1024];
    char **files, **fp;
    struct file_entry *fes, *fest;
    int isdoc, isconf, isdir;
    char *filename, *s;
    char *str;
    int count = 0;
    int c;

    fes = NULL;

    str = getStringBuf(sb);
    files = splitString(str, strlen(str), '\n');
    fp = files;

    while (*fp) {
	strcpy(buf, *fp);  /* temp copy */
	isdoc = 0;
	isconf = 0;
	isdir = 0;
	filename = NULL;
	s = strtok(buf, " \t\n");
	while (s) {
	    if (!strcmp(s, "%doc")) {
		isdoc = 1;
	    } else if (!strcmp(s, "%config")) {
		isconf = 1;
	    } else if (!strcmp(s, "%dir")) {
		isdir = 1;
	    } else {
		filename = s;
	    }
	    s = strtok(NULL, " \t\n");
	}
	if (! filename) {
	    fp++;
	    continue;
	}
	count += add_file(&fes, filename, isdoc, isconf, isdir);
	
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
	int_32 * fileMtimesList;
	int_32 * fileFlagsList;
	int_16 * fileModesList;
	int_16 * fileRDevsList;

	fileList = malloc(sizeof(char *) * count);
	fileLinktoList = malloc(sizeof(char *) * count);
	fileMD5List = malloc(sizeof(char *) * count);
	fileSizeList = malloc(sizeof(int_32) * count);
	fileUIDList = malloc(sizeof(int_32) * count);
	fileGIDList = malloc(sizeof(int_32) * count);
	fileMtimesList = malloc(sizeof(int_32) * count);
	fileFlagsList = malloc(sizeof(int_32) * count);
	fileModesList = malloc(sizeof(int_16) * count);
	fileRDevsList = malloc(sizeof(int_16) * count);

	fest = fes;
	c = count;
	while (c--) {
	    fileList[c] = strdup(fes->file);
	    mdfile(fes->file, buf);
	    fileMD5List[c] = strdup(buf);
	    message(MESS_DEBUG, "md5(%s) = %s\n", fes->file, buf);
	    fileSizeList[c] = fes->statbuf.st_size;
	    fileUIDList[c] = fes->statbuf.st_uid;
	    fileGIDList[c] = fes->statbuf.st_gid;
	    fileMtimesList[c] = fes->statbuf.st_mtime;
	    fileFlagsList[c] = 0;
	    if (fes->isdoc) 
		fileFlagsList[c] |= RPMFILE_DOC;
	    if (fes->isconf)
		fileFlagsList[c] |= RPMFILE_CONFIG;

	    fileModesList[c] = fes->statbuf.st_mode;
	    fileRDevsList[c] = fes->statbuf.st_rdev;

	    if (S_ISLNK(fes->statbuf.st_mode)) {
		readlink(fes->file, buf, 1024);
		fileLinktoList[c] = strdup(buf);
	    } else {
		/* This is stupid */
		fileLinktoList[c] = strdup("");
	    }

	    fes = fes->next;
	}

	/* Add the header entries */
	c = count;
	addEntry(header, RPMTAG_FILENAMES, STRING_TYPE, fileList, c);
	addEntry(header, RPMTAG_FILELINKTOS, STRING_TYPE, fileLinktoList, c);
	addEntry(header, RPMTAG_FILEMD5S, STRING_TYPE, fileMD5List, c);
	addEntry(header, RPMTAG_FILESIZES, INT32_TYPE, fileSizeList, c);
	addEntry(header, RPMTAG_FILEUIDS, INT32_TYPE, fileUIDList, c);
	addEntry(header, RPMTAG_FILEGIDS, INT32_TYPE, fileGIDList, c);
	addEntry(header, RPMTAG_FILEMTIMES, INT32_TYPE, fileMtimesList, c);
	addEntry(header, RPMTAG_FILEFLAGS, INT32_TYPE, fileFlagsList, c);
	addEntry(header, RPMTAG_FILEMODES, INT16_TYPE, fileModesList, c);
	addEntry(header, RPMTAG_FILERDEVS, INT16_TYPE, fileRDevsList, c);
	
	/* Free the allocated strings */
	c = count;
	while (c--) {
	    free(fileList[c]);
	    free(fileMD5List[c]);
	    free(fileLinktoList[c]);
	}
	
	/* Free the file entry stack */
	while (fest) {
	    fes = fest->next;
	    free(fest);
	    fest = fes;
	}
    }
    
    freeSplitString(files);
    return 0;
}

int packageBinaries(Spec s)
{
    char name[1024];
    char filename[1024];
    struct PackageRec *pr;
    Header outHeader;
    HeaderIterator headerIter;
    int_32 tag, type, c;
    void *ptr;
    int fd;
    time_t buildtime;
    char *version;
    char *release;

    if (init_pack()) {
	return 1;
    }

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

    buildtime = time(NULL);
    
    /* Look through for each package */
    pr = s->packages;
    while (pr) {
	if (pr->files == -1) {
	    pr = pr->next;
	    continue;
	}
	
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
	
	/* First build the header structure.            */
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
	
	process_filelist(outHeader, pr->filelist);
	
	sprintf(filename, "%s.%s.rpm", name, arch_token);
	fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (writeMagic(s, pr, fd, name, BINARY_HEADER)) {
	    return 1;
	}

	/* Add some final entries to the header */
	addEntry(outHeader, RPMTAG_BUILDTIME, INT32_TYPE, &buildtime, 1);
	writeHeader(fd, outHeader);
	
	/* Now do the cpio | gzip thing */
	if (cpio_gzip(outHeader, fd)) {
	    return 1;
	}
    
	close(fd);

	freeHeader(outHeader);
	pr = pr->next;
    }
    
    return 0;
}

int packageSource(Spec s)
{
    if (init_pack()) {
	return 1;
    }
    
    return 0;
}
