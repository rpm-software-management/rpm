#include "system.h"

#include "build/rpmbuild.h"

#include "install.h"
#include "url.h"
#include "verify.h"

#define	POPT_NOFILES	1000

/* ========== Verify specific popt args */
static void verifyArgCallback(poptContext con, enum poptCallbackReason reason,
			     const struct poptOption * opt, const char * arg, 
			     struct rpmQVArguments * data)
{
    switch (opt->val) {
      case POPT_NOFILES: data->flags |= VERIFY_FILES; break;
    }
}

static int noFiles = 0;
struct poptOption rpmVerifyPoptTable[] = {
	{ NULL, '\0', POPT_ARG_CALLBACK | POPT_CBFLAG_INC_DATA, 
		verifyArgCallback, 0, NULL, NULL },
 	{ "nofiles", '\0', 0, &noFiles, POPT_NOFILES,
		N_("don't verify files in package"), NULL},
	{ 0, 0, 0, 0, 0,	NULL, NULL }
};

/* ======================================================================== */
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

int rpmVerify(const char * prefix, enum rpmQVSources source, int verifyFlags,
	      const char *arg)
{
    Header h;
    int offset;
    int rc;
    int isSource;
    rpmdb db = NULL;
    dbiIndexSet matches;
    int recNumber;
    int retcode = 0;
    char *end = NULL;

    switch (source) {
    case RPMQV_RPM:
	if (!(verifyFlags & VERIFY_DEPS))
	    break;
	/* fall thru */
    default:
	if (rpmdbOpen(prefix, &db, O_RDONLY, 0644))
	    return 1;	/* XXX was exit(EXIT_FAILURE) */
	break;
    }

    switch (source) {
      case RPMQV_RPM:
      {	FD_t fd;

	fd = ufdOpen(arg, O_RDONLY, 0);
	if (fdFileno(fd) < 0) {
	    fprintf(stderr, _("open of %s failed\n"), arg);
	    ufdClose(fd);
	    retcode = 1;
	    break;
	}

	retcode = rpmReadPackageHeader(fd, &h, &isSource, NULL, NULL);

	ufdClose(fd);

	switch (retcode) {
	case 0:
	    retcode = verifyPackage(prefix, db, h, verifyFlags);
	    headerFree(h);
	    break;
	case 1:
	    fprintf(stderr, _("%s does not appear to be a RPM package\n"), arg);
	    /* fallthrough */
	case 2:
	    fprintf(stderr, _("query of %s failed\n"), arg);
	    retcode = 1;
	    break;
	}
      }	break;

      case RPMQV_SPECFILE:	/* XXX FIXME */
	break;

      case RPMQV_ALL:
	for (offset = rpmdbFirstRecNum(db);
	     offset != 0;
	     offset = rpmdbNextRecNum(db, offset)) {
		h = rpmdbGetRecord(db, offset);
		if (h == NULL) {
		    fprintf(stderr, _("could not read database record!\n"));
		    return 1;	/* XXX was exit(EXIT_FAILURE) */
		}
		
		if ((rc = verifyPackage(prefix, db, h, verifyFlags)) != 0)
		    retcode = rc;
		headerFree(h);
	}
	break;
      case RPMQV_GROUP:
	if (rpmdbFindByGroup(db, arg, &matches)) {
	    fprintf(stderr, _("group %s does not contain any packages\n"), 
			arg);
	} else {
	    retcode = verifyMatches(prefix, db, matches, verifyFlags);
	    dbiFreeIndexRecord(matches);
	}
	break;

      case RPMQV_WHATPROVIDES:
	if (rpmdbFindByProvides(db, arg, &matches)) {
	    fprintf(stderr, _("no package provides %s\n"), arg);
	    retcode = 1;
	} else {
	    retcode = verifyMatches(prefix, db, matches, verifyFlags);
	    dbiFreeIndexRecord(matches);
	}
	break;

      case RPMQV_TRIGGEREDBY:
	if (rpmdbFindByTriggeredBy(db, arg, &matches)) {
	    fprintf(stderr, _("no package triggers %s\n"), arg);
	    retcode = 1;
	} else {
	    retcode = verifyMatches(prefix, db, matches, verifyFlags);
	    dbiFreeIndexRecord(matches);
	}
	break;

      case RPMQV_WHATREQUIRES:
	if (rpmdbFindByRequiredBy(db, arg, &matches)) {
	    fprintf(stderr, _("no package requires %s\n"), arg);
	    retcode = 1;
	} else {
	    retcode = verifyMatches(prefix, db, matches, verifyFlags);
	    dbiFreeIndexRecord(matches);
	}
	break;

      case RPMQV_PATH:
	if (rpmdbFindByFile(db, arg, &matches)) {
	    int myerrno = 0;
	    if (access(arg, F_OK) != 0)
		myerrno = errno;
	    switch (myerrno) {
	    default:
		fprintf(stderr, _("file %s: %s\n"), arg, strerror(myerrno));
		break;
	    case 0:
		fprintf(stderr, _("file %s is not owned by any package\n"), arg);
		break;
	    }
	    retcode = 1;
	} else {
	    retcode = verifyMatches(prefix, db, matches, verifyFlags);
	    dbiFreeIndexRecord(matches);
	}
	break;

      case RPMQV_PACKAGE:
	rc = rpmdbFindByLabel(db, arg, &matches);
	if (rc == 1) {
	    retcode = 1;
	    fprintf(stderr, _("package %s is not installed\n"), arg);
	} else if (rc == 2) {
	    retcode = 1;
	    fprintf(stderr, _("error looking for package %s\n"), arg);
	} else {
	    retcode = verifyMatches(prefix, db, matches, verifyFlags);
	    dbiFreeIndexRecord(matches);
	}
	break;

      case RPMQV_DBOFFSET:
	recNumber = strtoul(arg, &end, 10);
	if ((*end) || (end == arg) || (recNumber == ULONG_MAX)) {
	    fprintf(stderr, _("invalid package number: %s\n"), arg);
	    return 1;
	}
	rpmMessage(RPMMESS_DEBUG, _("verifying package: %d\n"), recNumber);
	h = rpmdbGetRecord(db, recNumber);
	if (h == NULL)  {
	    fprintf(stderr, _("record %d could not be read\n"), recNumber);
	    retcode = 1;
	} else {
	    rc = verifyMatches(prefix, db, matches, verifyFlags);
	    headerFree(h);
	}
	break;
    }
   
    switch(source) {
    case RPMQV_RPM:
	if (!(verifyFlags & VERIFY_DEPS))
	    break;
	/* fall thru */
    default:
	if (db) {
	    rpmdbClose(db);
	    db = NULL;
	}
	break;
    }
    return retcode;
}
