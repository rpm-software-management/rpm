#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lib/messages.h"
#include "install.h"
#include "intl.h"
#include "query.h"
#include "rpmlib.h"
#include "url.h"
#include "verify.h"

static void verifyHeader(char * prefix, Header h, int verifyFlags);
static void verifyMatches(char * prefix, rpmdb db, dbiIndexSet matches,
			  int verifyFlags);
static void verifyDependencies(rpmdb db, Header h);

static void verifyHeader(char * prefix, Header h, int verifyFlags) {
    char ** fileList;
    int count, type;
    int verifyResult;
    int i;
    char * size, * md5, * link, * mtime, * mode;
    char * group, * user, * rdev;
    int_32 * fileFlagsList;
    int omitMask = 0;

    if (!(verifyFlags & VERIFY_MD5)) omitMask = RPMVERIFY_MD5;

    headerGetEntry(h, RPMTAG_FILEFLAGS, NULL, (void **) &fileFlagsList, NULL);

    if (headerGetEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, 
			&count)) {
	for (i = 0; i < count; i++) {
	    if (rpmVerifyFile(prefix, h, i, &verifyResult, omitMask))
		printf("missing    %s\n", fileList[i]);
	    else {
		size = md5 = link = mtime = mode = ".";
		user = group = rdev = ".";

		if (!verifyResult) continue;
	    
		if (verifyResult & RPMVERIFY_MD5)
		    md5 = "5";
		if (verifyResult & RPMVERIFY_FILESIZE)
		    size = "S";
		if (verifyResult & RPMVERIFY_LINKTO)
		    link = "L";
		if (verifyResult & RPMVERIFY_MTIME)
		    mtime = "T";
		if (verifyResult & RPMVERIFY_RDEV)
		    rdev = "D";
		if (verifyResult & RPMVERIFY_USER)
		    user = "U";
		if (verifyResult & RPMVERIFY_GROUP)
		    group = "G";
		if (verifyResult & RPMVERIFY_MODE)
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

static void verifyDependencies(rpmdb db, Header h) {
    rpmDependencies rpmdep;
    struct rpmDependencyConflict * conflicts;
    int numConflicts;
    char * name, * version, * release;
    int type, count, i;

    rpmdep = rpmdepDependencies(db);
    rpmdepAddPackage(rpmdep, h);

    rpmdepCheck(rpmdep, &conflicts, &numConflicts);
    rpmdepDone(rpmdep);

    if (numConflicts) {
	headerGetEntry(h, RPMTAG_NAME, &type, (void **) &name, &count);
	headerGetEntry(h, RPMTAG_VERSION, &type, (void **) &version, &count);
	headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) &release, &count);
	printf(_("Unsatisfied dependencies for %s-%s-%s: "), name, version, 
		release);
	for (i = 0; i < numConflicts; i++) {
	    if (i) printf(", ");
	    printf("%s", conflicts[i].needsName);
	    if (conflicts[i].needsFlags) {
		printDepFlags(stdout, conflicts[i].needsVersion, 
			      conflicts[i].needsFlags);
	    }
	}
	printf("\n");
	rpmdepFreeConflicts(conflicts, numConflicts);
    }
}

static void verifyPackage(char * root, rpmdb db, Header h, int verifyFlags) {
    if (verifyFlags & VERIFY_DEPS)
	verifyDependencies(db, h);
    if (verifyFlags & VERIFY_FILES)
	verifyHeader(root, h, verifyFlags);
    if (verifyFlags & VERIFY_SCRIPT) {
	rpmVerifyScript(root, h, 1);
    }
}

static void verifyMatches(char * prefix, rpmdb db, dbiIndexSet matches,
			  int verifyFlags) {
    int i;
    Header h;

    for (i = 0; i < matches.count; i++) {
	if (matches.recs[i].recOffset) {
	    rpmMessage(RPMMESS_DEBUG, "verifying record number %d\n",
			matches.recs[i].recOffset);
	    
	    h = rpmdbGetRecord(db, matches.recs[i].recOffset);
	    if (!h) {
		fprintf(stderr, _("error: could not read database record\n"));
	    } else {
		verifyPackage(prefix, db, h, verifyFlags);
		headerFree(h);
	    }
	}
    }
}

void doVerify(char * prefix, enum verifysources source, char ** argv,
	      int verifyFlags) {
    Header h;
    int offset;
    int fd;
    int rc;
    int isSource;
    rpmdb db;
    dbiIndexSet matches;
    struct urlContext context;
    char * arg;
    int isUrl;
    char path[255];

    if (source == VERIFY_RPM && !(verifyFlags & VERIFY_DEPS)) {
	db = NULL;
    } else {
	if (rpmdbOpen(prefix, &db, O_RDONLY, 0644)) {
	    exit(1);
	}
    }

    if (source == VERIFY_EVERY) {
	offset = rpmdbFirstRecNum(db);
	while (offset) {
	    h = rpmdbGetRecord(db, offset);
	    if (!h) {
		fprintf(stderr, _("could not read database record!\n"));
		exit(1);
	    }
	    verifyPackage(prefix, db, h, verifyFlags);
	    headerFree(h);
	    offset = rpmdbNextRecNum(db, offset);
	}
    } else {
	while (*argv) {
	    arg = *argv++;

	    switch (source) {
	      case VERIFY_RPM:
		if (urlIsURL(arg)) {
		    isUrl = 1;
		    if ((fd = urlGetFd(arg, &context)) < 0) {
			fprintf(stderr, _("open of %s failed: %s\n"), arg, 
				ftpStrerror(fd));
		    }
		} else if (!strcmp(arg, "-")) {
		    fd = 0;
		} else {
		    if ((fd = open(arg, O_RDONLY)) < 0) {
			fprintf(stderr, _("open of %s failed: %s\n"), arg, 
				strerror(errno));
		    }
		}

		if (fd >= 0) {
		    rc = rpmReadPackageHeader(fd, &h, &isSource, NULL, NULL);
		    close(fd);
		    switch (rc) {
			case 0:
			    verifyPackage(prefix, db, h, verifyFlags);
			    headerFree(h);
			    break;
			case 1:
			    fprintf(stderr, "%s is not an RPM\n", arg);
		    }
		}
			
		break;

	      case VERIFY_GRP:
		if (rpmdbFindByGroup(db, arg, &matches)) {
		    fprintf(stderr, 
				_("group %s does not contain any packages\n"), 
				arg);
		} else {
		    verifyMatches(prefix, db, matches, verifyFlags);
		    dbiFreeIndexRecord(matches);
		}
		break;

	      case VERIFY_PATH:
		if (*arg != '/') {
		    if (realpath(arg, path) != NULL)
			arg = path;
		}
		if (rpmdbFindByFile(db, arg, &matches)) {
		    fprintf(stderr, _("file %s is not owned by any package\n"), 
				arg);
		} else {
		    verifyMatches(prefix, db, matches, verifyFlags);
		    dbiFreeIndexRecord(matches);
		}
		break;

	      case VERIFY_PACKAGE:
		rc = findPackageByLabel(db, arg, &matches);
		if (rc == 1) 
		    fprintf(stderr, _("package %s is not installed\n"), arg);
		else if (rc == 2) {
		    fprintf(stderr, _("error looking for package %s\n"), arg);
		} else {
		    verifyMatches(prefix, db, matches, verifyFlags);
		    dbiFreeIndexRecord(matches);
		}
		break;

		case VERIFY_EVERY:
		    ; /* nop */
	    }
	}
    }
   
    if (source != VERIFY_RPM) {
	rpmdbClose(db);
    }
}
