#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "lib/package.h"
#include "rpmlib.h"
#include "query.h"

static void printHeader(Header h, int queryFlags);
static char * getString(Header h, int tag, char * def);    
static void showMatches(rpmdb db, dbIndexSet matches, int queryFlags);
static int findMatches(rpmdb db, char * name, char * version, char * release,
		       dbIndexSet * matches);

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
    int_32 * fileFlagsList;
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

		for (i = 0; i < count; i++) {
		    if (!((queryFlags & QUERY_FOR_DOCS) || 
			  (queryFlags & QUERY_FOR_CONFIG)) 
			|| ((queryFlags & QUERY_FOR_DOCS) && 
			    (fileFlagsList[i] & RPMFILE_DOC))
			|| ((queryFlags & QUERY_FOR_CONFIG) && 
			    (fileFlagsList[i] & RPMFILE_CONFIG))) {

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
		    }
		}
	    
		free(fileList);
	    }
	}
    }
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
	if (!rpmdbOpen(prefix, &db, O_RDONLY, 0644)) {
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
