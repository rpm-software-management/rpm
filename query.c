#include <alloca.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/param.h>
#include <unistd.h>

#include "lib/messages.h"
#include "rpmlib.h"
#include "query.h"

static char * permsString(int mode);
static void printHeader(Header h, int queryFlags, char * queryFormat);
static int queryPartial(Header h, char ** chptrptr, int * cntptr, 
			int arrayNum);
static int queryHeader(Header h, char * chptr);
static int queryArray(Header h, char ** chptrptr);
static void escapedChar(char ch);
static char * handleFormat(Header h, char * chptr, int * count, int arrayNum);
static void showMatches(rpmdb db, dbIndexSet matches, int queryFlags, 
			char * queryFormat);
static int findMatches(rpmdb db, char * name, char * version, char * release,
		       dbIndexSet * matches);
static void printFileInfo(char * name, unsigned int size, unsigned short mode,
			  unsigned int mtime, unsigned short rdev,
			  char * owner, char * group, int uid, int gid,
			  char * linkto);

static char * defaultQueryFormat = 
	    "Name        : %-27{NAME} Distribution: %{DISTRIBUTION}\n"
	    "Version     : %-27{VERSION}       Vendor: %{VENDOR}\n"
	    "Release     : %-27{RELEASE}   Build Date: %{BUILDTIME:date}\n"
	    "Install date: %-27{INSTALLTIME:date}   Build Host: %{BUILDHOST}\n"
	    "Group       : %-27{GROUP}   Source RPM: %{SOURCERPM}\n"
	    "Size        : %{SIZE}\n"
	    "Summary     : %{SUMMARY}\n"
	    "Description :\n%{DESCRIPTION}\n";
static char * requiresQueryFormat = 
	    "[%{REQUIRENAME} %{REQUIREFLAGS:depflags} %{REQUIREVERSION}\n]";
static char * providesQueryFormat = "[%{PROVIDES}\n]";
static char * scriptQueryFormat = 
	    "printinstall script:\n%{RPMTAG_PREIN}\n"
	    "postinstall script:\n%{RPMTAG_POSTIN}\n"
	    "preuninstall script:\n%{RPMTAG_PREUN}\n"
	    "postuninstall script:\n%{RPMTAG_POSTIN}\n";

static int queryHeader(Header h, char * chptr) {
    int count = 0;

    return queryPartial(h, &chptr, &count, -1);
}

static int queryArray(Header h, char ** chptrptr) {
    int count = 0;
    int arrayNum;
    char * start = NULL;

    /* first one */
    start = *chptrptr;
    if (queryPartial(h, chptrptr, &count, 0)) return 1;

    arrayNum = 1;
    while (arrayNum < count) {
	*chptrptr = start;
	if (queryPartial(h, chptrptr, &count, arrayNum++)) return 1;
    }

    return 0;
}

static int queryPartial(Header h, char ** chptrptr, int * cntptr, 
			int arrayNum) {
    char * chptr = *chptrptr;
    int count;
    int emptyItem = 0;

    while (chptr && *chptr) {
	switch (*chptr) {
	  case '\\':
	    chptr++;
	    if (!*chptr) return 1;
	    escapedChar(*chptr++);
	    break;

	  case '%':
	    chptr++;
	    if (!*chptr) return 1;
	    if (*chptr == '%') {
		putchar('%');
		chptr++;
	    }
	    count = *cntptr;
	    chptr = handleFormat(h, chptr, &count, arrayNum);
	    if (!count) {
		count = 0;	
		emptyItem = 1;
	    } else if (count != -1 && !*cntptr && !emptyItem){ 
		*cntptr = count;
	    } else if (count != -1 && *cntptr && count != *cntptr) {
		printf("(parallel array size mismatch)");
		return 1;
	    }
	    break;

	  case ']':
	    if (arrayNum == -1) {
		printf("(unexpected ']')");
		return 1;
	    }
	    *chptrptr = chptr + 1;
	    return 0;

	  case '[':
	    if (arrayNum != -1) {
		printf("(unexpected ']')");
		return 1;
	    }
	    *chptrptr = chptr + 1;
	    if (queryArray(h, chptrptr)) {
		return 1;
	    }
	    chptr = *chptrptr;
	    break;

	  default:
	    putchar(*chptr++);
	}
    }

    *chptrptr = chptr;

    return 0;
}

static char * handleFormat(Header h, char * chptr, int * cntptr,
				 int arrayNum) {
    const char * f = chptr;
    const char * tagptr;
    char * end;
    char how[20], format[20];
    int i, tagLength;
    char tag[100];
    const struct rpmTagTableEntry * t;
    void * p;
    int type;
    int notArray = 0;
    time_t dateint;
    struct tm * tstruct;
    char buf[100];
    int count;
    int anint;

    strcpy(format, "%");
    while (*chptr && *chptr != '{') chptr++;
    if (!*chptr || (chptr - f > (sizeof(format) - 3))) {
	fprintf(stderr, "bad query format - %s\n", f);
	return NULL;
    }

    strncat(format, f, chptr - f);

    tagptr = ++chptr;
    while (*chptr && *chptr != '}' && *chptr != ':' ) chptr++;
    if (tagptr == chptr || !*chptr) {
	fprintf(stderr, "bad query format - %s\n", f);
	return NULL;
    }

    if (*chptr == ':') {
	end = chptr + 1;
	while (*end && *end != '}') end++;

	if (*end != '}') {
	    fprintf(stderr, "bad query format - %s\n", f);
	    return NULL;
	}
	if ((end - chptr + 1) > sizeof(how)) {
	    fprintf(stderr, "bad query format - %s\n", f);
	    return NULL;
	}
	strncpy(how, chptr + 1, end - chptr - 1);
	how[end - chptr - 1] = '\0';
    } else {
	strcpy(how, "");
	end = chptr;
    }

    switch (*tagptr) {
	case '=':	notArray = 1, tagptr++;	break;
    }

    tagLength = chptr - tagptr;
    chptr = end + 1;

    if (tagLength > (sizeof(tag) - 20)) {
	fprintf(stderr, "query tag too long\n");
	return NULL;
    }
    memset(tag, 0, sizeof(tag));
    if (strncmp(tagptr, "RPMTAG_", 7)) {
	strcpy(tag, "RPMTAG_");
    }
    strncat(tag, tagptr, tagLength);

    for (i = 0, t = rpmTagTable; i < rpmTagTableSize; i++, t++) {
	if (!strcasecmp(tag, t->name)) break;
    }

    if (i == rpmTagTableSize) {
	fprintf(stderr, "unknown tag %s\n", tag);
	return NULL;
    }
 
    if (!getEntry(h, t->val, &type, &p, &count) || !p) {
	p = "(none)";
	count = 1;
	type = STRING_TYPE;
    } else if (notArray) {
	*cntptr = -1;
    } else if (count > 1 && (arrayNum == -1)) {
	p = "(array)";
	count = 1;
	type = STRING_TYPE;
    } else if ((count - 1) < arrayNum && arrayNum != -1) {
	p = "(past array end)";
	count = 1;
	type = STRING_TYPE;
    } else if (arrayNum != -1)
	*cntptr = count;

    if (arrayNum == -1) arrayNum = 0;

    switch (type) {
      case STRING_ARRAY_TYPE:
	strcat(format, "s");
	printf(format, ((char **) p)[arrayNum]);
	free(p);
	break;

      case STRING_TYPE:
	strcat(format, "s");
	printf(format, p);
	break;

      case INT8_TYPE:
	strcat(format, "d");
	printf(format, *(((int_8 *) p) + arrayNum));
	break;

      case INT16_TYPE:
	if (!strcmp(how, "perms") || !strcmp(how, "permissions")) {
	    strcat(format, "s");
	    printf(format, permsString(*(((int_16 *) p) + arrayNum)));
	} else {
	    strcat(format, "d");
	    printf(format, *(((int_16 *) p) + arrayNum));
	}
	break;

      case INT32_TYPE:
	if (!strcmp(how, "date")) {
	    strcat(format, "s");
	    /* this is important if sizeof(int_32) ! sizeof(time_t) */
	    dateint = *(((int_32 *) p) + arrayNum);
	    tstruct = localtime(&dateint);
	    strftime(buf, sizeof(buf) - 1, "%c", tstruct);
	    printf(format, buf);
	} else if (!strcmp(how, "perms") || !strcmp(how, "permissions")) {
	    strcat(format, "s");
	    printf(format, permsString(*(((int_32 *) p) + arrayNum)));
	} else if (!strcmp(how, "depflags")) {
	    buf[0] = '\0';
	    anint = *(((int_32 *) p) + arrayNum);
	    if (anint & REQUIRE_LESS) 
		strcat(buf, "<");
	    if (anint & REQUIRE_GREATER)
		strcat(buf, ">");
	    if (anint & REQUIRE_EQUAL)
		strcat(buf, "=");
	    if (anint & REQUIRE_SERIAL)
		strcat(buf, "S");
	
	    strcat(format, "s");
	    printf(format, buf);
	} else {
	    strcat(format, "d");
	    printf(format, *(((int_32 *) p) + arrayNum));
	}
	break;

      default:
	printf("(can't handle type %d)", type);
	break;
    }

    return chptr;
}

static void escapedChar(const char ch) {
    switch (ch) {
      case 'a': 	putchar('\a'); break;
      case 'b': 	putchar('\b'); break;
      case 'f': 	putchar('\f'); break;
      case 'n': 	putchar('\n'); break;
      case 'r': 	putchar('\r'); break;
      case 't': 	putchar('\t'); break;
      case 'v': 	putchar('\v'); break;

      default:		putchar(ch); break;
    }
}

static void printHeader(Header h, int queryFlags, char * queryFormat) {
    char * name, * version, * release;
    int_32 count, type;
    char * prefix = NULL;
    char ** fileList, ** fileMD5List;
    char * fileStatesList;
    char ** fileOwnerList, ** fileGroupList;
    char ** fileLinktoList;
    int_32 * fileFlagsList, * fileMTimeList, * fileSizeList;
    int_32 * fileUIDList, * fileGIDList;
    uint_16 * fileModeList;
    uint_16 * fileRdevList;
    int i;

    getEntry(h, RPMTAG_NAME, &type, (void **) &name, &count);
    getEntry(h, RPMTAG_VERSION, &type, (void **) &version, &count);
    getEntry(h, RPMTAG_RELEASE, &type, (void **) &release, &count);

    if (!queryFlags) {
	printf("%s-%s-%s\n", name, version, release);
    } else {
	if (queryFlags & QUERY_FOR_INFO) {
	    if (!queryFormat) {
		queryFormat = defaultQueryFormat;
	    } 

	    queryHeader(h, queryFormat);
	}

	if (queryFlags & QUERY_FOR_PROVIDES) {
	    if (isEntry(h, RPMTAG_PROVIDES))
		queryHeader(h, providesQueryFormat);
	}

	if (queryFlags & QUERY_FOR_REQUIRES) {
	    if (isEntry(h, RPMTAG_REQUIREFLAGS))
		queryHeader(h, requiresQueryFormat);
	}

	if (queryFlags & QUERY_FOR_LIST) {
	    if (!getEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, 
		 &count)) {
		puts("(contains no files)");
	    } else {
		if (!getEntry(h, RPMTAG_FILESTATES, &type, 
			 (void **) &fileStatesList, &count)) {
		    fileStatesList = NULL;
		}
		getEntry(h, RPMTAG_FILEFLAGS, &type, 
			 (void **) &fileFlagsList, &count);
		getEntry(h, RPMTAG_FILESIZES, &type, 
			 (void **) &fileSizeList, &count);
		getEntry(h, RPMTAG_FILEMODES, &type, 
			 (void **) &fileModeList, &count);
		getEntry(h, RPMTAG_FILEMTIMES, &type, 
			 (void **) &fileMTimeList, &count);
		getEntry(h, RPMTAG_FILERDEVS, &type, 
			 (void **) &fileRdevList, &count);
		getEntry(h, RPMTAG_FILEUIDS, &type, 
			 (void **) &fileUIDList, &count);
		getEntry(h, RPMTAG_FILEGIDS, &type, 
			 (void **) &fileGIDList, &count);
		getEntry(h, RPMTAG_FILEUSERNAME, &type, 
			 (void **) &fileOwnerList, &count);
		getEntry(h, RPMTAG_FILEGROUPNAME, &type, 
			 (void **) &fileGroupList, &count);
		getEntry(h, RPMTAG_FILELINKTOS, &type, 
			 (void **) &fileLinktoList, &count);
		getEntry(h, RPMTAG_FILEMD5S, &type, 
			 (void **) &fileMD5List, &count);

		for (i = 0; i < count; i++) {
		    if (!((queryFlags & QUERY_FOR_DOCS) || 
			  (queryFlags & QUERY_FOR_CONFIG)) 
			|| ((queryFlags & QUERY_FOR_DOCS) && 
			    (fileFlagsList[i] & RPMFILE_DOC))
			|| ((queryFlags & QUERY_FOR_CONFIG) && 
			    (fileFlagsList[i] & RPMFILE_CONFIG))) {

			if (!isVerbose())
			    prefix ? fputs(prefix, stdout) : 0;

			if (queryFlags & QUERY_FOR_STATE) {
			    if (fileStatesList) {
				switch (fileStatesList[i]) {
				  case RPMFILE_STATE_NORMAL:
				    fputs("normal        ", stdout); break;
				  case RPMFILE_STATE_REPLACED:
				    fputs("replaced      ", stdout); break;
				  case RPMFILE_STATE_NOTINSTALLED:
				    fputs("not installed ", stdout); break;
				  default:
				    fputs("(unknown)     ", stdout);
				}
			    } else {
				fputs(    "(no state)    ", stdout);
			    }
			}
			    
			if (queryFlags & QUERY_FOR_DUMPFILES) {
			    printf("%s %d %d %s 0%o ", fileList[i],
				   fileSizeList[i], fileMTimeList[i],
				   fileMD5List[i], fileModeList[i]);

			    if (fileOwnerList)
				printf("%s %s", fileOwnerList[i], 
						fileGroupList[i]);
			    else
				printf("%d %d", fileUIDList[i], 
						fileGIDList[i]);

			    printf(" %s %s %d ", 
				 fileFlagsList[i] & RPMFILE_CONFIG ? "1" : "0",
				 fileFlagsList[i] & RPMFILE_DOC ? "1" : "0",
				 fileRdevList[i]);

			    if (strlen(fileLinktoList[i]))
				printf("%s\n", fileLinktoList[i]);
			    else
				printf("X\n");

			} else if (!isVerbose()) {
			    puts(fileList[i]);
			} else if (fileOwnerList) 
			    printFileInfo(fileList[i], fileSizeList[i],
					  fileModeList[i], fileMTimeList[i],
					  fileRdevList[i], fileOwnerList[i], 
					  fileGroupList[i], fileUIDList[i], 
					  fileGIDList[i], fileLinktoList[i]);
			else
			    printFileInfo(fileList[i], fileSizeList[i],
					  fileModeList[i], fileMTimeList[i],
					  fileRdevList[i], NULL, 
					  NULL, fileUIDList[i], 
					  fileGIDList[i], fileLinktoList[i]);
		    }
		}
	    
		free(fileList);
		free(fileLinktoList);
		free(fileMD5List);
		if (fileOwnerList) free(fileOwnerList);
		if (fileGroupList) free(fileGroupList);
	    }
	}

	if (queryFlags & QUERY_FOR_SCRIPTS) {
	    queryHeader(h, scriptQueryFormat);
	}
    }
}

static char * permsString(int mode) {
    static char perms[11];

    strcpy(perms, "-----------");
   
    if (mode & S_ISVTX) perms[9] = 't';

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

static void printFileInfo(char * name, unsigned int size, unsigned short mode,
			  unsigned int mtime, unsigned short rdev,
			  char * owner, char * group, int uid, int gid,
			  char * linkto) {
    char sizefield[15];
    char ownerfield[9], groupfield[9];
    char timefield[100] = "";
    time_t themtime;
    time_t currenttime;
    static int thisYear = 0;
    static int thisMonth = 0;
    struct tm * tstruct;
    char * namefield = name;
    char * perms;

    perms = permsString(mode);

    if (!thisYear) {
	currenttime = time(NULL);
	tstruct = localtime(&currenttime);
	thisYear = tstruct->tm_year;
	thisMonth = tstruct->tm_mon;
    }

    if (owner) 
	strncpy(ownerfield, owner, 8);
    else
	sprintf(ownerfield, "%-8d", uid);

    if (group) 
	strncpy(groupfield, group, 8);
    else
	sprintf(groupfield, "%-8d", gid);

    /* this is normally right */
    sprintf(sizefield, "%10d", size);

    /* this knows too much about dev_t */

    if (S_ISLNK(mode)) {
	namefield = alloca(strlen(name) + strlen(linkto) + 10);
	sprintf(namefield, "%s -> %s", name, linkto);
    } else if (S_ISCHR(mode)) {
	perms[0] = 'c';
	sprintf(sizefield, "%3d, %3d", rdev >> 8, rdev & 0xFF);
    } else if (S_ISBLK(mode)) {
	perms[0] = 'b';
	sprintf(sizefield, "%3d, %3d", rdev >> 8, rdev & 0xFF);
    }

    /* this is important if sizeof(int_32) ! sizeof(time_t) */
    themtime = mtime;
    tstruct = localtime(&themtime);

    if (tstruct->tm_year == thisYear || 
	((tstruct->tm_year + 1) == thisYear && tstruct->tm_mon > thisMonth)) 
	strftime(timefield, sizeof(timefield) - 1, "%b %d %H:%M", tstruct);
    else
	strftime(timefield, sizeof(timefield) - 1, "%b %d  %Y", tstruct);

    printf("%s %8s %8s %10s %s %s\n", perms, ownerfield, groupfield, 
		sizefield, timefield, namefield);
}

static void showMatches(rpmdb db, dbIndexSet matches, int queryFlags, 
			char * queryFormat) {
    int i;
    Header h;

    for (i = 0; i < matches.count; i++) {
	if (matches.recs[i].recOffset) {
	    message(MESS_DEBUG, "querying record number %d\n",
			matches.recs[i].recOffset);
	    
	    h = rpmdbGetRecord(db, matches.recs[i].recOffset);
	    if (!h) {
		fprintf(stderr, "error: could not read database record\n");
	    } else {
		printHeader(h, queryFlags, queryFormat);
		freeHeader(h);
	    }
	}
    }
}

int doQuery(char * prefix, enum querysources source, int queryFlags, 
	     char * arg, char * queryFormat) {
    Header h;
    int offset;
    int fd;
    int rc;
    int isSource;
    rpmdb db;
    dbIndexSet matches;
    int recNumber;
    int retcode = 0;
    char *end = NULL;

    if (source != QUERY_SRPM && source != QUERY_RPM) {
	if (rpmdbOpen(prefix, &db, O_RDONLY, 0644)) {
	    exit(1);
	}
    }

    switch (source) {
      case QUERY_SRPM:
      case QUERY_RPM:
	fd = open(arg, O_RDONLY);
	if (fd < 0) {
	    fprintf(stderr, "open of %s failed: %s\n", arg, strerror(errno));
	} else {
	    rc = pkgReadHeader(fd, &h, &isSource, NULL, NULL);
	    close(fd);
	    switch (rc) {
		case 0:
		    if (!h) {
			fprintf(stderr, "old format source packages cannot be "
						"queried\n");
		    } else {
			printHeader(h, queryFlags, queryFormat);
			freeHeader(h);
		    }
		    break;
		case 1:
		    fprintf(stderr, "%s does not appear to be a RPM package\n", 
				arg);
		    /* fallthrough */
		case 2:
		    fprintf(stderr, "query of %s failed\n", arg);
		    retcode = 1;
	    }

	}
		
	break;

      case QUERY_ALL:
	offset = rpmdbFirstRecNum(db);
	while (offset) {
	    h = rpmdbGetRecord(db, offset);
	    if (!h) {
		fprintf(stderr, "could not read database record!\n");
		return 1;
	    }
	    printHeader(h, queryFlags, queryFormat);
	    freeHeader(h);
	    offset = rpmdbNextRecNum(db, offset);
	}
	break;

      case QUERY_SGROUP:
      case QUERY_GROUP:
	if (rpmdbFindByGroup(db, arg, &matches)) {
	    fprintf(stderr, "group %s does not contain any packages\n", arg);
	    retcode = 1;
	} else {
	    showMatches(db, matches, queryFlags, queryFormat);
	    freeDBIndexRecord(matches);
	}
	break;

      case QUERY_WHATPROVIDES:
	if (rpmdbFindByProvides(db, arg, &matches)) {
	    fprintf(stderr, "no package provides %s\n", arg);
	    retcode = 1;
	} else {
	    showMatches(db, matches, queryFlags, queryFormat);
	    freeDBIndexRecord(matches);
	}
	break;

      case QUERY_WHATREQUIRES:
	if (rpmdbFindByRequiredBy(db, arg, &matches)) {
	    fprintf(stderr, "no package requires %s\n", arg);
	    retcode = 1;
	} else {
	    showMatches(db, matches, queryFlags, queryFormat);
	    freeDBIndexRecord(matches);
	}
	break;

      case QUERY_SPATH:
      case QUERY_PATH:
	if (*arg != '/') {
	    char path[255];
	    if (realpath(arg, path) != NULL)
		arg = path;
	}
	if (rpmdbFindByFile(db, arg, &matches)) {
	    fprintf(stderr, "file %s is not owned by any package\n", arg);
	    retcode = 1;
	} else {
	    showMatches(db, matches, queryFlags, queryFormat);
	    freeDBIndexRecord(matches);
	}
	break;

      case QUERY_DBOFFSET:

      case QUERY_SPACKAGE:
	recNumber = strtoul(arg, &end, 10);
	if ((*end) || (end == arg) || (recNumber == ULONG_MAX)) {
	    fprintf(stderr, "invalid package number: %s\n", arg);
	    return 1;
	}
	message(MESS_DEBUG, "showing package: %d\n", recNumber);
	h = rpmdbGetRecord(db, recNumber);

	if (!h)  {
	    fprintf(stderr, "record %d could not be read\n", recNumber);
	    retcode = 1;
	} else {
	    printHeader(h, queryFlags, queryFormat);
	    freeHeader(h);
	}
	break;

      case QUERY_PACKAGE:
	rc = findPackageByLabel(db, arg, &matches);
	if (rc == 1) {
	    retcode = 1;
	    fprintf(stderr, "package %s is not installed\n", arg);
	} else if (rc == 2) {
	    retcode = 1;
	    fprintf(stderr, "error looking for package %s\n", arg);
	} else {
	    showMatches(db, matches, queryFlags, queryFormat);
	    freeDBIndexRecord(matches);
	}
	break;
    }
   
    if (source != QUERY_SRPM && source != QUERY_RPM) {
	rpmdbClose(db);
    }

    return retcode;
}

/* 0 found matches */
/* 1 no matches */
/* 2 error */
int findPackageByLabel(rpmdb db, char * arg, dbIndexSet * matches) {
    char * localarg, * chptr;
    char * release;
    int rc;
 
    if (!strlen(arg)) return 1;

    /* did they give us just a name? */
    rc = findMatches(db, arg, NULL, NULL, matches);
    if (rc != 1) return rc;

    /* maybe a name and a release */
    localarg = alloca(strlen(arg) + 1);
    strcpy(localarg, arg);

    chptr = (localarg + strlen(localarg)) - 1;
    while (chptr > localarg && *chptr != '-') chptr--;
    if (chptr == localarg) return 1;

    *chptr = '\0';
    rc = findMatches(db, localarg, chptr + 1, NULL, matches);
    if (rc != 1) return rc;
    
    /* how about name-version-release? */

    release = chptr + 1;
    while (chptr > localarg && *chptr != '-') chptr--;
    if (chptr == localarg) return 1;

    *chptr = '\0';
    return findMatches(db, localarg, chptr + 1, release, matches);
}

/* 0 found matches */
/* 1 no matches */
/* 2 error */
int findMatches(rpmdb db, char * name, char * version, char * release,
		dbIndexSet * matches) {
    int gotMatches;
    int rc;
    int i;
    char * pkgRelease, * pkgVersion;
    int count, type;
    int goodRelease, goodVersion;
    Header h;

    if ((rc = rpmdbFindPackage(db, name, matches))) {
	if (rc == -1) return 2; else return 1;
    }

    if (!version && !release) return 0;

    gotMatches = 0;

    /* make sure the version and releases match */
    for (i = 0; i < matches->count; i++) {
	if (matches->recs[i].recOffset) {
	    h = rpmdbGetRecord(db, matches->recs[i].recOffset);
	    if (!h) {
		fprintf(stderr, "error: could not read database record\n");
		freeDBIndexRecord(*matches);
		return 2;
	    }

	    getEntry(h, RPMTAG_VERSION, &type, (void **) &pkgVersion, &count);
	    getEntry(h, RPMTAG_RELEASE, &type, (void **) &pkgRelease, &count);
	    
	    goodRelease = goodVersion = 1;

	    if (release && strcmp(release, pkgRelease)) goodRelease = 0;
	    if (version && strcmp(version, pkgVersion)) goodVersion = 0;

	    if (goodRelease && goodVersion) 
		gotMatches = 1;
	    else 
		matches->recs[i].recOffset = 0;
	}
    }

    if (!gotMatches) {
	freeDBIndexRecord(*matches);
	return 1;
    }
    
    return 0;
}

void queryPrintTags(void) {
    const struct rpmTagTableEntry * t;
    int i;

    for (i = 0, t = rpmTagTable; i < rpmTagTableSize; i++, t++) {
	printf("%s\n", t->name);
    }
}
