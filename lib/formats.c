#include "system.h"

#include <rpmlib.h>

static char * permsString(int mode)
{
    char * perms = xmalloc(11);

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

static char * triggertypeFormat(int_32 type, const void * data, 
	/*@unused@*/char * formatPrefix, /*@unused@*/int padding,
	/*@unused@*/int element)
{
    const int_32 * item = data;
    char * val;

    if (type != RPM_INT32_TYPE) {
	val = xmalloc(20);
	strcpy(val, _("(not a number)"));
    } else if (*item & RPMSENSE_TRIGGERIN) {
	val = xstrdup("in");
    } else {
	val = xstrdup("un");
    }

    return val;
}

static char * permsFormat(int_32 type, const void * data, 
		char * formatPrefix, int padding, /*@unused@*/int element)
{
    char * val;
    char * buf;

    if (type != RPM_INT32_TYPE) {
	val = xmalloc(20);
	strcpy(val, _("(not a number)"));
    } else {
	val = xmalloc(15 + padding);
	strcat(formatPrefix, "s");
	buf = permsString(*((int_32 *) data));
	sprintf(val, formatPrefix, buf);
	free(buf);
    }

    return val;
}

static char * fflagsFormat(int_32 type, const void * data, 
		char * formatPrefix, int padding, /*@unused@*/int element)
{
    char * val;
    char buf[15];
    int anint = *((int_32 *) data);

    if (type != RPM_INT32_TYPE) {
	val = xmalloc(20);
	strcpy(val, _("(not a number)"));
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
	if (anint & RPMFILE_GHOST)
	    strcat(buf, "g");

	val = xmalloc(5 + padding);
	strcat(formatPrefix, "s");
	sprintf(val, formatPrefix, buf);
    }

    return val;
}

static char * depflagsFormat(int_32 type, const void * data, 
		char * formatPrefix, int padding, /*@unused@*/int element)
{
    char * val;
    char buf[10];
    int anint = *((int_32 *) data);

    if (type != RPM_INT32_TYPE) {
	val = xmalloc(20);
	strcpy(val, _("(not a number)"));
    } else {
	buf[0] = '\0';

	if (anint & RPMSENSE_LESS) 
	    strcat(buf, "<");
	if (anint & RPMSENSE_GREATER)
	    strcat(buf, ">");
	if (anint & RPMSENSE_EQUAL)
	    strcat(buf, "=");

	val = xmalloc(5 + padding);
	strcat(formatPrefix, "s");
	sprintf(val, formatPrefix, buf);
    }

    return val;
}

static int fsnamesTag( /*@unused@*/ Header h, int_32 * type, void ** data,
	int_32 * count, int * freeData)
{
    const char ** list;

    if (rpmGetFilesystemList(&list, count)) {
	return 1;
    }

    *type = RPM_STRING_ARRAY_TYPE;
    *((const char ***) data) = list;

    *freeData = 0;

    return 0; 
}

static int instprefixTag(Header h, int_32 * type, void ** data, int_32 * count,
		      int * freeData)
{
    char ** array;

    if (headerGetEntry(h, RPMTAG_INSTALLPREFIX, type, data, count)) {
	*freeData = 0;
	return 0;
    } else if (headerGetEntry(h, RPMTAG_INSTPREFIXES, NULL, (void **) &array, 
			      count)) {
	*((char **) data) = xstrdup(array[0]);
	*freeData = 1;
	*type = RPM_STRING_TYPE;
	free(array);
	return 0;
    } 

    return 1;
}

static int fssizesTag(Header h, int_32 * type, void ** data, int_32 * count,
		      int * freeData)
{
    const char ** filenames;
    int_32 * filesizes;
    uint_32 * usages;
    int numFiles;

    if (!headerGetEntry(h, RPMTAG_FILENAMES, NULL, (void **) &filenames, NULL)) 
	filenames = NULL;

    if (!headerGetEntry(h, RPMTAG_FILESIZES, NULL, (void **) &filesizes, 
		       &numFiles)) {
	filesizes = NULL;
	numFiles = 0;
    }

    if (rpmGetFilesystemList(NULL, count)) {
	return 1;
    }

    *type = RPM_INT32_TYPE;
    *freeData = 1;

    if (filenames == NULL) {
	usages = xcalloc((*count), sizeof(usages));
	*data = usages;

	return 0;
    }

    if (rpmGetFilesystemUsage(filenames, filesizes, numFiles, &usages, 0))	
	return 1;

    *data = usages;

    return 0;
}

static int triggercondsTag(Header h, /*@out@*/int_32 * type, /*@out@*/void ** data, 
			   /*@out@*/int_32 * count, /*@out@*/int * freeData)
{
    int_32 * indices, * flags;
    char ** names, ** versions;
    int numNames, numScripts;
    char ** conds, ** s;
    char * item, * flagsStr;
    char * chptr;
    int i, j;
    char buf[5];

    if (!headerGetEntry(h, RPMTAG_TRIGGERNAME, NULL, (void **) &names, 
			&numNames)) {
	*freeData = 0;
	return 0;
    }

    headerGetEntry(h, RPMTAG_TRIGGERINDEX, NULL, (void **) &indices, NULL);
    headerGetEntry(h, RPMTAG_TRIGGERFLAGS, NULL, (void **) &flags, NULL);
    headerGetEntry(h, RPMTAG_TRIGGERVERSION, NULL, (void **) &versions, NULL);
    headerGetEntry(h, RPMTAG_TRIGGERSCRIPTS, NULL, (void **) &s, &numScripts);
    free(s);

    *freeData = 1;
    *data = conds = xmalloc(sizeof(char * ) * numScripts);
    *count = numScripts;
    *type = RPM_STRING_ARRAY_TYPE;
    for (i = 0; i < numScripts; i++) {
	chptr = xstrdup("");

	for (j = 0; j < numNames; j++) {
	    if (indices[j] != i) continue;

	    item = xmalloc(strlen(names[j]) + strlen(versions[j]) + 20);
	    if (flags[j] & RPMSENSE_SENSEMASK) {
		buf[0] = '%', buf[1] = '\0';
		flagsStr = depflagsFormat(RPM_INT32_TYPE, flags, buf,
					  0, j);
		sprintf(item, "%s %s %s", names[j], flagsStr, versions[j]);
		free(flagsStr);
	    } else {
		strcpy(item, names[j]);
	    }

	    chptr = xrealloc(chptr, strlen(chptr) + strlen(item) + 5);
	    if (*chptr) strcat(chptr, ", ");
	    strcat(chptr, item);
	    free(item);
	}

	conds[i] = chptr;
    }

    free(names);
    free(versions);

    return 0;
}

static int triggertypeTag(Header h, int_32 * type, /*@out@*/void ** data, 
			   int_32 * count, int * freeData)
{
    int_32 * indices, * flags;
    char ** conds, ** s;
    int i, j;
    int numScripts, numNames;

    if (!headerGetEntry(h, RPMTAG_TRIGGERINDEX, NULL, (void **) &indices, 
			&numNames)) {
	*freeData = 0;
	return 1;
    }

    headerGetEntry(h, RPMTAG_TRIGGERFLAGS, NULL, (void **) &flags, NULL);

    headerGetEntry(h, RPMTAG_TRIGGERSCRIPTS, NULL, (void **) &s, &numScripts);
    free(s);

    *freeData = 1;
    *data = conds = xmalloc(sizeof(char * ) * numScripts);
    *count = numScripts;
    *type = RPM_STRING_ARRAY_TYPE;
    for (i = 0; i < numScripts; i++) {
	for (j = 0; j < numNames; j++) {
	    if (indices[j] != i) continue;

	    if (flags[j] & RPMSENSE_TRIGGERIN)
		conds[i] = xstrdup("in");
	    else if (flags[j] & RPMSENSE_TRIGGERUN)
		conds[i] = xstrdup("un");
	    else
		conds[i] = xstrdup("postun");
	    break;
	}
    }

    return 0;
}

const struct headerSprintfExtension rpmHeaderFormats[] = {
    { HEADER_EXT_TAG, "RPMTAG_FSSIZES", { fssizesTag } },
    { HEADER_EXT_TAG, "RPMTAG_FSNAMES", { fsnamesTag } },
    { HEADER_EXT_TAG, "RPMTAG_INSTALLPREFIX", { instprefixTag } },
    { HEADER_EXT_TAG, "RPMTAG_TRIGGERCONDS", { triggercondsTag } },
    { HEADER_EXT_TAG, "RPMTAG_TRIGGERTYPE", { triggertypeTag } },
    { HEADER_EXT_FORMAT, "depflags", { depflagsFormat } },
    { HEADER_EXT_FORMAT, "fflags", { fflagsFormat } },
    { HEADER_EXT_FORMAT, "perms", { permsFormat } },
    { HEADER_EXT_FORMAT, "permissions", { permsFormat } },
    { HEADER_EXT_FORMAT, "triggertype", { triggertypeFormat } },
    { HEADER_EXT_MORE, NULL, { (void *) headerDefaultFormats } }
} ;
