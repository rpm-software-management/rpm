#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lib/messages.h"
#include "lib/package.h"
#include "query.h"
#include "rpmlib.h"
#include "verify.h"

static void verifyHeader(char * prefix, Header h);
static void verifyMatches(char * prefix, rpmdb db, dbIndexSet matches);

static void verifyHeader(char * prefix, Header h) {
    char ** fileList;
    int count, type;
    int verifyResult;
    int i;
    char * size, * md5, * link, * mtime, * mode;
    char * group, * user, * rdev;
    int_32 * fileFlagsList;

    getEntry(h, RPMTAG_FILEFLAGS, &type, (void **) &fileFlagsList, &count);

    if (getEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, &count)) {
	for (i = 0; i < count; i++) {
	    if (rpmVerifyFile(prefix, h, i, &verifyResult))
		printf("missing    %s\n", fileList[i]);
	    else {
		size = md5 = link = mtime = mode = ".";
		user = group = rdev = ".";

		if (!verifyResult) continue;
	    
		if (verifyResult & VERIFY_MD5)
		    md5 = "5";
		if (verifyResult & VERIFY_FILESIZE)
		    size = "S";
		if (verifyResult & VERIFY_LINKTO)
		    link = "L";
		if (verifyResult & VERIFY_MTIME)
		    mtime = "T";
		if (verifyResult & VERIFY_RDEV)
		    rdev = "D";
		if (verifyResult & VERIFY_USER)
		    user = "U";
		if (verifyResult & VERIFY_GROUP)
		    group = "G";
		if (verifyResult & VERIFY_MODE)
		    mode = "M";

		printf("%s%s%s%s%s%s%s%s %c %s\n",
		       size, mode, md5, rdev, link, user, group, mtime, 
		       fileFlagsList[i] & RPMFILE_CONFIG ? 'c' : ' ', 
		       fileList[i]);
	    }
	}
	
	free(fileList);
    }
}

static void verifyMatches(char * prefix, rpmdb db, dbIndexSet matches) {
    int i;
    Header h;

    for (i = 0; i < matches.count; i++) {
	if (matches.recs[i].recOffset) {
	    message(MESS_DEBUG, "verifying record number %d\n",
			matches.recs[i].recOffset);
	    
	    h = rpmdbGetRecord(db, matches.recs[i].recOffset);
	    if (!h) {
		fprintf(stderr, "error: could not read database record\n");
	    } else {
		verifyHeader(prefix, h);
		freeHeader(h);
	    }
	}
    }
}

void doVerify(char * prefix, enum verifysources source, char ** argv) {
    Header h;
    int offset;
    int fd;
    int rc;
    int isSource;
    rpmdb db;
    dbIndexSet matches;
    char * arg;

    if (source != VERIFY_SRPM && source != VERIFY_RPM) {
	if (rpmdbOpen(prefix, &db, O_RDONLY, 0644)) {
	    exit(1);
	}
    }

    if (source != VERIFY_SRPM && source != VERIFY_RPM) {
	if (rpmdbOpen(prefix, &db, O_RDONLY, 0644)) {
	    exit(1);
	}
    }

    if (source == VERIFY_EVERY) {
	offset = rpmdbFirstRecNum(db);
	while (offset) {
	    h = rpmdbGetRecord(db, offset);
	    if (!h) {
		fprintf(stderr, "could not read database record!\n");
		exit(1);
	    }
	    verifyHeader(prefix, h);
	    freeHeader(h);
	    offset = rpmdbNextRecNum(db, offset);
	}
    } else {
	while (*argv) {
	    arg = *argv++;

	    switch (source) {
	      case VERIFY_SRPM:
	      case VERIFY_RPM:
		fd = open(arg, O_RDONLY);
		if (fd < 0) {
		    fprintf(stderr, "open of %s failed: %s\n", arg, 
			    strerror(errno));
		} else {
		    rc = pkgReadHeader(fd, &h, &isSource);
		    close(fd);
		    switch (rc) {
			case 0:
			    verifyHeader(prefix, h);
			    freeHeader(h);
			    break;
			case 1:
			    fprintf(stderr, "%s is not an RPM\n", arg);
		    }
		}
			
		break;

	      case VERIFY_SGROUP:
	      case VERIFY_GRP:
		if (rpmdbFindByGroup(db, arg, &matches)) {
		    fprintf(stderr, "group %s does not contain any pacakges\n", 
				     arg);
		} else {
		    verifyMatches(prefix, db, matches);
		    freeDBIndexRecord(matches);
		}
		break;

	      case VERIFY_SPATH:
	      case VERIFY_PATH:
		if (rpmdbFindByFile(db, arg, &matches)) {
		    fprintf(stderr, "file %s is not owned by any package\n", 
				arg);
		} else {
		    verifyMatches(prefix, db, matches);
		    freeDBIndexRecord(matches);
		}
		break;

	      case VERIFY_SPACKAGE:
	      case VERIFY_PACKAGE:
		rc = findPackageByLabel(db, arg, &matches);
		if (rc == 1) 
		    fprintf(stderr, "package %s is not installed\n", arg);
		else if (rc == 2) {
		    fprintf(stderr, "error looking for package %s\n", arg);
		} else {
		    verifyMatches(prefix, db, matches);
		    freeDBIndexRecord(matches);
		}
		break;

		case VERIFY_EVERY:
		    ; /* nop */
	    }
	}
    }
   
    if (source != VERIFY_SRPM && source != VERIFY_RPM) {
	rpmdbClose(db);
    }
}
