#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "rpmlib.h"
#include "query.h"

void printHeader(Header h, int queryFlags);

void printHeader(Header h, int queryFlags) {
    char * name, * version, * release;
    char * distribution, * vendor, * group, *description, * buildHost;
    uint_32 * size;
    int_32 count, type;
    int_32 * pBuildDate;
    int_32 buildDate;
    char * prefix = NULL;
    char buildDateStr[100];
    struct tm * tstruct;
    char ** fileList;
    char * fileStatesList;
    int_32 * fileFlagsList;
    int i;

    getEntry(h, RPMTAG_NAME, &type, (void **) &name, &count);
    getEntry(h, RPMTAG_VERSION, &type, (void **) &version, &count);
    getEntry(h, RPMTAG_RELEASE, &type, (void **) &release, &count);

    if (!queryFlags) {
	printf("%s-%s-%s\n", name, version, release);
    } else {
	if (queryFlags & QUERY_FOR_INFO) {
	    getEntry(h, RPMTAG_DISTRIBUTION, &type, (void **) &distribution, 
		     &count);
	    getEntry(h, RPMTAG_DESCRIPTION, &type, (void **) &description, 
		     &count);
	    getEntry(h, RPMTAG_BUILDHOST, &type, (void **) &buildHost, &count);
	    getEntry(h, RPMTAG_VENDOR, &type, (void **) &vendor, &count);
	    getEntry(h, RPMTAG_GROUP, &type, (void **) &group, &count);
	    getEntry(h, RPMTAG_SIZE, &type, (void **) &size, &count);
	    getEntry(h, RPMTAG_BUILDTIME, &type, (void **) &pBuildDate, &count);

	    buildDate = ntohl(*pBuildDate);
	    tstruct = localtime((time_t *) &buildDate);
	    strftime(buildDateStr, sizeof(buildDateStr) - 1, "%c", tstruct);
	   
	    printf("Name        : %-27s Distribution: %s\n", 
		    name, distribution);
	    printf("Version     : %-27s       Vendor: %s\n", version, vendor);
	    printf("Release     : %-27s   Build Date: %s\n", release,				    buildDateStr); 
	    printf("Install date: %-27s   Build Host: %s\n", "", buildHost);
	    printf("Group       : %s\n", group);
	    printf("Size        : %d\n", ntohl(*size));
	    printf("Description : %s\n", description);
	    prefix = "    ";
	}

	if (queryFlags & QUERY_FOR_LIST) {
	    getEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, &count);
	    getEntry(h, RPMTAG_FILESTATES, &type, 
		     (void **) &fileStatesList, &count);
	    getEntry(h, RPMTAG_FILEFLAGS, &type, 
		     (void **) &fileFlagsList, &count);

	    for (i = 0; i < count; i++) {
		fileFlagsList[i] = ntohl(fileFlagsList[i]);

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
	    
	    /*free(fileList);
	    free(fileStatesList);
	    free(fileFlagsList);*/
	}

/*
		for (i = 0; i < package.fileCount; i++) {
		    if (queryFlags & QUERY_FOR_STATE) 
			printf("%s%-9s%s\n", prefix, 
				rpmfileStateStrs[package.files[i].state],
				package.files[i].path);
		    else
			printf("%s%s\n", prefix, package.files[i].path);
		}
	    }
*/
    }
}

void doQuery(char * prefix, enum querysources source, int queryFlags, 
	     char * arg) {
    Header h;
    int offset;
    int i;
    rpmdb db;
    dbIndexSet matches;

    if (!rpmdbOpen(prefix, &db, O_RDWR, 0644)) {
	fprintf(stderr, "cannot open %s/var/lib/rpm/packages.rpm\n", prefix);
	exit(1);
    }

    switch (source) {
      case QUERY_SRPM:
      case QUERY_RPM:
	printf("not yet\n");
	exit(1);
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

      case QUERY_SPATH:
      case QUERY_PATH:
	if (rpmdbFindByFile(db, arg, &matches)) {
	    fprintf(stderr, "file %s is not owned by and package\n", arg);
	} else {
	    for (i = 0; i < matches.count; i++) {
		h = rpmdbGetRecord(db, matches.recs[i].recOffset);
		if (!h) {
		    fprintf(stderr, "error: could not read database record\n");
		} else {
		    printHeader(h, queryFlags);
		    freeHeader(h);
		}
	    }
	}
	break;

      case QUERY_SPACKAGE:
      case QUERY_PACKAGE:
	printf("not yet\n");
	exit(1);
	break;
    }
   
    rpmdbClose(db);
}
