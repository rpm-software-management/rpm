#include "system.h"

#include "build/rpmbuild.h"

#include "install.h"
#include "url.h"
#include "verify.h"

static int verifyHeader(const char * prefix, Header h, int verifyFlags);
static int verifyMatches(const char * prefix, rpmdb db, dbiIndexSet matches,
			  int verifyFlags);
static int verifyDependencies(rpmdb db, Header h);

static int verifyHeader(const char * prefix, Header h, int verifyFlags) {
    const char ** fileList;
    int count, type;
    int verifyResult;
    int i, ec, rc;
    int_32 * fileFlagsList;
    int omitMask = 0;

    ec = 0;
    if (!(verifyFlags & VERIFY_MD5)) omitMask = RPMVERIFY_MD5;

    if (headerGetEntry(h, RPMTAG_FILEFLAGS, NULL, (void **) &fileFlagsList, NULL) &&
       headerGetEntry(h, RPMTAG_FILENAMES, &type, (void **) &fileList, &count)) {
	for (i = 0; i < count; i++) {
	    if ((rc = rpmVerifyFile(prefix, h, i, &verifyResult, omitMask)) != 0) {
		fprintf(stdout, _("missing    %s\n"), fileList[i]);
	    } else {
		const char * size, * md5, * link, * mtime, * mode;
		const char * group, * user, * rdev;
		static const char * aok = ".";
		static const char * unknown = "?";

		if (!verifyResult) continue;
	    
		rc = 1;

#define	_verify(_RPMVERIFY_F, _C)	\
	((verifyResult & _RPMVERIFY_F) ? _C : aok)
#define	_verifylink(_RPMVERIFY_F, _C)	\
	((verifyResult & RPMVERIFY_READLINKFAIL) ? unknown : \
	 (verifyResult & _RPMVERIFY_F) ? _C : aok)
#define	_verifyfile(_RPMVERIFY_F, _C)	\
	((verifyResult & RPMVERIFY_READFAIL) ? unknown : \
	 (verifyResult & _RPMVERIFY_F) ? _C : aok)
	
		md5 = _verifyfile(RPMVERIFY_MD5, "5");
		size = _verify(RPMVERIFY_FILESIZE, "S");
		link = _verifylink(RPMVERIFY_LINKTO, "L");
		mtime = _verify(RPMVERIFY_MTIME, "T");
		rdev = _verify(RPMVERIFY_RDEV, "D");
		user = _verify(RPMVERIFY_USER, "U");
		group = _verify(RPMVERIFY_GROUP, "G");
		mode = _verify(RPMVERIFY_MODE, "M");

#undef _verify
#undef _verifylink
#undef _verifyfile

		fprintf(stdout, "%s%s%s%s%s%s%s%s %c %s\n",
		       size, mode, md5, rdev, link, user, group, mtime, 
		       fileFlagsList[i] & RPMFILE_CONFIG ? 'c' : ' ', 
		       fileList[i]);
	    }
	    if (rc)
		ec = rc;
	}
	
	free(fileList);
    }
    return ec;
}

static int verifyDependencies(rpmdb db, Header h) {
    rpmTransactionSet rpmdep;
    struct rpmDependencyConflict * conflicts;
    int numConflicts;
    const char * name, * version, * release;
    int type, count, i;

    rpmdep = rpmtransCreateSet(db, NULL);
    rpmtransAddPackage(rpmdep, h, NULL, NULL, 0, NULL);

    rpmdepCheck(rpmdep, &conflicts, &numConflicts);
    rpmtransFree(rpmdep);

    if (numConflicts) {
	headerGetEntry(h, RPMTAG_NAME, &type, (void **) &name, &count);
	headerGetEntry(h, RPMTAG_VERSION, &type, (void **) &version, &count);
	headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) &release, &count);
	fprintf(stdout, _("Unsatisfied dependencies for %s-%s-%s: "),
		name, version, release);
	for (i = 0; i < numConflicts; i++) {
	    if (i) fprintf(stdout, ", ");
	    fprintf(stdout, "%s", conflicts[i].needsName);
	    if (conflicts[i].needsFlags) {
		printDepFlags(stdout, conflicts[i].needsVersion, 
			      conflicts[i].needsFlags);
	    }
	}
	fprintf(stdout, "\n");
	rpmdepFreeConflicts(conflicts, numConflicts);
	return 1;
    }
    return 0;
}

static int verifyPackage(const char * root, rpmdb db, Header h, int verifyFlags) {
    int ec, rc;
    FD_t fdo;
    ec = 0;
    if ((verifyFlags & VERIFY_DEPS) &&
	(rc = verifyDependencies(db, h)) != 0)
	    ec = rc;
    if ((verifyFlags & VERIFY_FILES) &&
	(rc = verifyHeader(root, h, verifyFlags)) != 0)
	    ec = rc;;
    fdo = fdDup(STDOUT_FILENO);
    if ((verifyFlags & VERIFY_SCRIPT) &&
	(rc = rpmVerifyScript(root, h, fdo)) != 0)
	    ec = rc;
    fdClose(fdo);
    return ec;
}

static int verifyMatches(const char * prefix, rpmdb db, dbiIndexSet matches,
			  int verifyFlags) {
    int ec, rc;
    int i;
    Header h;

    ec = 0;
    for (i = 0; i < dbiIndexSetCount(matches); i++) {
	unsigned int recOffset = dbiIndexRecordOffset(matches, i);
	if (recOffset == 0)
	    continue;
	rpmMessage(RPMMESS_DEBUG, _("verifying record number %u\n"),
		recOffset);
	    
	h = rpmdbGetRecord(db, recOffset);
	if (h == NULL) {
		fprintf(stderr, _("error: could not read database record\n"));
		ec = 1;
	} else {
		if ((rc = verifyPackage(prefix, db, h, verifyFlags)) != 0)
		    ec = rc;
		headerFree(h);
	}
    }
    return ec;
}

int doVerify(const char * prefix, enum verifysources source, const char ** argv,
	      int verifyFlags) {
    Header h;
    int offset;
    int ec, rc;
    int isSource;
    rpmdb db;
    dbiIndexSet matches;

    ec = 0;
    if (source == VERIFY_RPM && !(verifyFlags & VERIFY_DEPS)) {
	db = NULL;
    } else {
	if (rpmdbOpen(prefix, &db, O_RDONLY, 0644)) {
	    return 1;	/* XXX was exit(EXIT_FAILURE) */
	}
    }

    if (source == VERIFY_EVERY) {
	for (offset = rpmdbFirstRecNum(db);
	     offset != 0;
	     offset = rpmdbNextRecNum(db, offset)) {
		h = rpmdbGetRecord(db, offset);
		if (h == NULL) {
		    fprintf(stderr, _("could not read database record!\n"));
		    return 1;	/* XXX was exit(EXIT_FAILURE) */
		}
		
		if ((rc = verifyPackage(prefix, db, h, verifyFlags)) != 0)
		    ec = rc;
		headerFree(h);
	}
    } else {
	while (*argv) {
	    const char *arg = *argv++;

	    rc = 0;
	    switch (source) {
	    case VERIFY_RPM:
	      { FD_t fd;

		fd = ufdOpen(arg, O_RDONLY, 0);
		if (fd == NULL) {
		    fprintf(stderr, _("open of %s failed\n"), arg);
		    break;
		}

		if (fdFileno(fd) >= 0) {
		    rc = rpmReadPackageHeader(fd, &h, &isSource, NULL, NULL);
		}

		ufdClose(fd);

		switch (rc) {
		case 0:
			rc = verifyPackage(prefix, db, h, verifyFlags);
			headerFree(h);
			break;
		case 1:
			fprintf(stderr, _("%s is not an RPM\n"), arg);
			break;
		}
	      }	break;

	    case VERIFY_GRP:
		if (rpmdbFindByGroup(db, arg, &matches)) {
		    fprintf(stderr, 
				_("group %s does not contain any packages\n"), 
				arg);
		} else {
		    rc = verifyMatches(prefix, db, matches, verifyFlags);
		    dbiFreeIndexRecord(matches);
		}
		break;

	    case VERIFY_PATH:
		if (rpmdbFindByFile(db, arg, &matches)) {
		    fprintf(stderr, _("file %s is not owned by any package\n"), 
				arg);
		} else {
		    rc = verifyMatches(prefix, db, matches, verifyFlags);
		    dbiFreeIndexRecord(matches);
		}
		break;

	    case VERIFY_PACKAGE:
		rc = rpmdbFindByLabel(db, arg, &matches);
		if (rc == 1) 
		    fprintf(stderr, _("package %s is not installed\n"), arg);
		else if (rc == 2) {
		    fprintf(stderr, _("error looking for package %s\n"), arg);
		} else {
		    rc = verifyMatches(prefix, db, matches, verifyFlags);
		    dbiFreeIndexRecord(matches);
		}
		break;

	    case VERIFY_EVERY:
		break;
	    }
	    if (rc)
		ec = rc;
	}
    }
   
    if (source != VERIFY_RPM) {
	rpmdbClose(db);
    }
    return ec;
}
