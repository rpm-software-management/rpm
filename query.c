#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "lib/messages.h"
#include "lib/package.h"
#include "rpmlib.h"
#include "query.h"

static void printHeader(Header h, int queryFlags);
static char * getString(Header h, int tag, char * def);    
static void showMatches(rpmdb db, dbIndexSet matches, int queryFlags);
static int findMatches(rpmdb db, char * name, char * version, char * release,
		       dbIndexSet * matches);
static void printFileInfo(char * name, unsigned int size, unsigned short mode,
			  unsigned int mtime, unsigned short rdev,
			  char * owner, char * group, int uid, int gid,
			  char * linkto);

static char * getString(Header h, int tag, char * def) {
    char * str;
    int count, type;
   
    if (!getEntry(h, tag, &type, (void **) &str, &count)) {
	return def;
    } 
 
    return str;
}

static void printHeader(Header h, int queryFlags) {
    char * name, * version, * release;
    char * distribution, * vendor, * group, *description, * buildHost;
    uint_32 * size;
    int_32 count, type;
    int_32 * pBuildDate, * pInstallDate;
    time_t buildDate, installDate;
    char * prefix = NULL;
    char buildDateStr[100];
    char installDateStr[100];
    struct tm * tstruct;
    char ** fileList;
    char * fileStatesList;
    char * sourcePackage;
    char ** fileOwnerList, ** fileGroupList;
    char ** fileLinktoList;
    int_32 * fileFlagsList, * fileMTimeList, * fileSizeList;
    int_32 * fileUIDList, * fileGIDList;
    int_16 * fileModeList;
    int_16 * fileRdevList;
    int i;

    getEntry(h, RPMTAG_NAME, &type, (void **) &name, &count);
    getEntry(h, RPMTAG_VERSION, &type, (void **) &version, &count);
    getEntry(h, RPMTAG_RELEASE, &type, (void **) &release, &count);

    if (!queryFlags) {
	printf("%s-%s-%s\n", name, version, release);
    } else {
	if (queryFlags & QUERY_FOR_INFO) {
	    distribution = getString(h, RPMTAG_DISTRIBUTION, "");
	    description = getString(h, RPMTAG_DESCRIPTION, "");
	    buildHost = getString(h, RPMTAG_BUILDHOST, "");
	    vendor = getString(h, RPMTAG_VENDOR, "");
	    group = getString(h, RPMTAG_GROUP, "Unknown");
	    sourcePackage = getString(h, RPMTAG_SOURCERPM, "Unknown");
	    if (!getEntry(h, RPMTAG_SIZE, &type, (void **) &size, &count)) 
		size = NULL;

	    if (getEntry(h, RPMTAG_BUILDTIME, &type, (void **) &pBuildDate, 
			 &count)) {
		/* this is important if sizeof(int_32) ! sizeof(time_t) */
		buildDate = *pBuildDate; 
		tstruct = localtime(&buildDate);
		strftime(buildDateStr, sizeof(buildDateStr) - 1, "%c", tstruct);
	    } else
		strcpy(buildDateStr, "(unknown)");

	    if (getEntry(h, RPMTAG_INSTALLTIME, &type, (void **) &pInstallDate, 
		&count)) {
		/* this is important if sizeof(int_32) ! sizeof(time_t) */
		installDate = *pInstallDate; 
		tstruct = localtime(&installDate);
		strftime(installDateStr, sizeof(installDateStr) - 1, "%c", 
			 tstruct);
	    } else 
		strcpy(installDateStr, "(unknown)");
	   
	    printf("Name        : %-27s Distribution: %s\n", 
		   name, distribution);
	    printf("Version     : %-27s       Vendor: %s\n", version, vendor);
	    printf("Release     : %-27s   Build Date: %s\n", release,
		   buildDateStr); 
	    printf("Install date: %-27s   Build Host: %s\n", installDateStr, 
		   buildHost);
	    printf("Group       : %-27s   Source RPM: %s\n", group, 
		   sourcePackage);
	    if (size) 
		printf("Size        : %d\n", *size);
	    else 
		printf("Size        : (unknown)\n");
	    printf("Description : %s\n", description);
	    prefix = "    ";
	}

	if (queryFlags & QUERY_FOR_LIST) {
	    if (!getEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, 
		 &count)) {
		puts("(contains no files)");
	    } else {
		getEntry(h, RPMTAG_FILESTATES, &type, 
			 (void **) &fileStatesList, &count);
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

		for (i = 0; i < count; i++) {
		    if (!((queryFlags & QUERY_FOR_DOCS) || 
			  (queryFlags & QUERY_FOR_CONFIG)) 
			|| ((queryFlags & QUERY_FOR_DOCS) && 
			    (fileFlagsList[i] & RPMFILE_DOC))
			|| ((queryFlags & QUERY_FOR_CONFIG) && 
			    (fileFlagsList[i] & RPMFILE_CONFIG))) {

			if (!isVerbose()) {
			    prefix ? fputs(prefix, stdout) : 0;
			    if (queryFlags & QUERY_FOR_STATE) {
				switch (fileStatesList[i]) {
				  case RPMFILE_STATE_NORMAL:
				    fputs("normal   ", stdout); break;
				  case RPMFILE_STATE_REPLACED:
				    fputs("replaced ", stdout); break;
				  default:
				    fputs("unknown  ", stdout);
				}
			    }
			    
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
		if (fileOwnerList) free(fileOwnerList);
		if (fileGroupList) free(fileGroupList);
	    }
	}
    }
}

static void printFileInfo(char * name, unsigned int size, unsigned short mode,
			  unsigned int mtime, unsigned short rdev,
			  char * owner, char * group, int uid, int gid,
			  char * linkto) {
    char perms[11];
    char sizefield[15];
    char ownerfield[9], groupfield[9];
    char timefield[100] = "";
    time_t themtime;
    time_t currenttime;
    static int thisYear = 0;
    static int thisMonth = 0;
    struct tm * tstruct;
    char * namefield = name;

    strcpy(perms, "----------");
   
    if (!thisYear) {
	currenttime = time(NULL);
	tstruct = localtime(&currenttime);
	thisYear = tstruct->tm_year;
	thisMonth = tstruct->tm_mon;
    }

    if (mode & S_IRUSR) perms[1]= 'r';
    if (mode & S_IWUSR) perms[2]= 'w';
    if (mode & S_IXUSR) perms[3]= 'x';

    if (mode & S_IRGRP) perms[4]= 'r';
    if (mode & S_IWGRP) perms[5]= 'w';
    if (mode & S_IXGRP) perms[6]= 'x';

    if (mode & S_IROTH) perms[7]= 'r';
    if (mode & S_IWOTH) perms[8]= 'w';
    if (mode & S_IXOTH) perms[9]= 'x';

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

    if (S_ISDIR(mode)) 
	perms[0] = 'd';
    else if (S_ISLNK(mode)) {
	perms[0] = 'l';
	namefield = alloca(strlen(name) + strlen(linkto) + 10);
	sprintf(namefield, "%s -> %s", name, linkto);
    }
    else if (S_ISFIFO(mode)) 
	perms[0] = 'p';
    else if (S_ISSOCK(mode)) 
	perms[0] = 'l';
    else if (S_ISCHR(mode)) {
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

static void showMatches(rpmdb db, dbIndexSet matches, int queryFlags) {
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
		printHeader(h, queryFlags);
		freeHeader(h);
	    }
	}
    }
}

void doQuery(char * prefix, enum querysources source, int queryFlags, 
	     char * arg) {
    Header h;
    int offset;
    int fd;
    int rc;
    int isSource;
    rpmdb db;
    dbIndexSet matches;

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
	    rc = pkgReadHeader(fd, &h, &isSource);
	    close(fd);
	    switch (rc) {
		case 0:
		    printHeader(h, queryFlags);
		    freeHeader(h);
		    break;
		case 1:
		    fprintf(stderr, "%s is not an RPM\n", arg);
	    }
	}
		
	break;

      case QUERY_ALL:
	offset = rpmdbFirstRecNum(db);
	while (offset) {
	    h = rpmdbGetRecord(db, offset);
	    if (!h) {
		fprintf(stderr, "could not read database record!\n");
		exit(1);
	    }
	    printHeader(h, queryFlags);
	    freeHeader(h);
	    offset = rpmdbNextRecNum(db, offset);
	}
	break;

      case QUERY_SGROUP:
      case QUERY_GROUP:
	if (rpmdbFindByGroup(db, arg, &matches)) {
	    fprintf(stderr, "group %s does not contain any pacakges\n", arg);
	} else {
	    showMatches(db, matches, queryFlags);
	    freeDBIndexRecord(matches);
	}
	break;

      case QUERY_SPATH:
      case QUERY_PATH:
	if (rpmdbFindByFile(db, arg, &matches)) {
	    fprintf(stderr, "file %s is not owned by any package\n", arg);
	} else {
	    showMatches(db, matches, queryFlags);
	    freeDBIndexRecord(matches);
	}
	break;

      case QUERY_SPACKAGE:
      case QUERY_PACKAGE:
	rc = findPackageByLabel(db, arg, &matches);
	if (rc == 1) 
	    fprintf(stderr, "package %s is not installed\n", arg);
	else if (rc == 2) {
	    fprintf(stderr, "error looking for package %s\n", arg);
	} else {
	    showMatches(db, matches, queryFlags);
	    freeDBIndexRecord(matches);
	}
	break;
    }
   
    if (source != QUERY_SRPM && source != QUERY_RPM) {
	rpmdbClose(db);
    }
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
