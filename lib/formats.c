#include "config.h"
#include "miscfn.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "header.h"
#include "rpmlib.h"

static char * permsFormat(int_32 type, const void * data, 
		          char * formatPrefix, int padding, int element);
static char * depflagsFormat(int_32 type, const void * data, 
		             char * formatPrefix, int padding, int element);
static char * fflagsFormat(int_32 type, const void * data, 
		           char * formatPrefix, int padding, int element);
static int fsnamesTag(Header h, int_32 * type, void ** data, int_32 * count,
		      int * freeData);
static int fssizesTag(Header h, int_32 * type, void ** data, int_32 * count,
		      int * freeData);
static char * permsString(int mode);

const struct headerSprintfExtension rpmHeaderFormats[] = {
    { HEADER_EXT_TAG, "RPMTAG_FSSIZES", { fssizesTag } },
    { HEADER_EXT_TAG, "RPMTAG_FSNAMES", { fsnamesTag } },
    { HEADER_EXT_FORMAT, "depflags", { depflagsFormat } },
    { HEADER_EXT_FORMAT, "fflags", { fflagsFormat } },
    { HEADER_EXT_FORMAT, "perms", { permsFormat } },
    { HEADER_EXT_FORMAT, "permissions", { permsFormat } },
    { HEADER_EXT_MORE, NULL, { (void *) headerDefaultFormats } }
} ;

static char * permsString(int mode) {
    char * perms = malloc(11);

    strcpy(perms, "-----------");
   
    if (mode & S_ISVTX) perms[10] = 't';

    if (mode & S_IRUSR) perms[1] = 'r';
    if (mode & S_IWUSR) perms[2] = 'w';
    if (mode & S_IXUSR) perms[3] = 'x';
 
    if (mode & S_IRGRP) perms[4] = 'r';
    if (mode & S_IWGRP) perms[5] = 'w';
    if (mode & S_IXGRP) perms[6] = 'x';

    if (mode & S_IROTH) perms[7] = 'r';
    if (mode & S_IWOTH) perms[8] = 'w';
    if (mode & S_IXOTH) perms[9] = 'x';

    if (mode & S_ISUID) {
	if (mode & S_IXUSR) 
	    perms[3] = 's'; 
	else
	    perms[3] = 'S'; 
    }

    if (mode & S_ISGID) {
	if (mode & S_IXGRP) 
	    perms[6] = 's'; 
	else
	    perms[6] = 'S'; 
    }

    if (S_ISDIR(mode)) 
	perms[0] = 'd';
    else if (S_ISLNK(mode)) {
	perms[0] = 'l';
    }
    else if (S_ISFIFO(mode)) 
	perms[0] = 'p';
    else if (S_ISSOCK(mode)) 
	perms[0] = 'l';
    else if (S_ISCHR(mode)) {
	perms[0] = 'c';
    } else if (S_ISBLK(mode)) {
	perms[0] = 'b';
    }

    return perms;
}

static char * permsFormat(int_32 type, const void * data, 
		         char * formatPrefix, int padding, int element) {
    char * val;
    char * buf;

    if (type != RPM_INT32_TYPE) {
	val = malloc(20);
	strcpy(val, "(not a number)");
    } else {
	val = malloc(15 + padding);
	strcat(formatPrefix, "s");
	buf = permsString(*((int_32 *) data));
	sprintf(val, formatPrefix, buf);
	free(buf);
    }

    return val;
}

static char * fflagsFormat(int_32 type, const void * data, 
		         char * formatPrefix, int padding, int element) {
    char * val;
    char buf[15];
    int anint = *((int_32 *) data);

    if (type != RPM_INT32_TYPE) {
	val = malloc(20);
	strcpy(val, "(not a number)");
    } else {
	buf[0] = '\0';
	if (anint & RPMFILE_DOC)
	    strcat(buf, "d");
	if (anint & RPMFILE_CONFIG)
	    strcat(buf, "c");
	if (anint & RPMFILE_SPECFILE)
	    strcat(buf, "s");
	if (anint & RPMFILE_MISSINGOK)
	    strcat(buf, "m");
	if (anint & RPMFILE_NOREPLACE)
	    strcat(buf, "n");

	val = malloc(5 + padding);
	strcat(formatPrefix, "s");
	sprintf(val, formatPrefix, buf);
    }

    return val;
}

static char * depflagsFormat(int_32 type, const void * data, 
		         char * formatPrefix, int padding, int element) {
    char * val;
    char buf[10];
    int anint = *((int_32 *) data);

    if (type != RPM_INT32_TYPE) {
	val = malloc(20);
	strcpy(val, "(not a number)");
    } else {
	*buf = '\0';

	if (anint & RPMSENSE_LESS) 
	    strcat(buf, "<");
	if (anint & RPMSENSE_GREATER)
	    strcat(buf, ">");
	if (anint & RPMSENSE_EQUAL)
	    strcat(buf, "=");
	if (anint & RPMSENSE_SERIAL)
	    strcat(buf, "S");

	val = malloc(5 + padding);
	strcat(formatPrefix, "s");
	sprintf(val, formatPrefix, buf);
    }

    return val;
}

static int fsnamesTag(Header h, int_32 * type, void ** data, int_32 * count,
		      int * freeData) {
    char ** list;
    int i;

    if (rpmGetFilesystemList(&list)) {
	return 1;
    }

    *type = RPM_STRING_ARRAY_TYPE;
    *((char ***) data) = list;

    for (i = 0; list[i]; i++) ;
    *count = i;

    *freeData = 0;

    return 0; 
}

static int fssizesTag(Header h, int_32 * type, void ** data, int_32 * count,
		      int * freeData) {
    char ** filenames;
    int_32 * filesizes;
    uint_32 * usages;
    int numFiles;

    headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &filenames, NULL);
    headerGetEntry(h, RPMTAG_FILESIZES, NULL, (void **) &filesizes, &numFiles);

    if (rpmGetFilesystemUsage(filenames, filesizes, numFiles, &usages, 0))	
	return 1;

    *type = RPM_INT32_TYPE;
    *count = 20;
    *freeData = 1;
    *data = usages;

    return 0;
}
