#include "system.h"

#include "build/rpmbuild.h"

#include "install.h"
#include "url.h"
#include "ftp.h"

static void printHash(const unsigned long amount, const unsigned long total);
static void printDepProblems(FILE * f, struct rpmDependencyConflict * conflicts,
			     int numConflicts);
static void showProgress(const Header h, const rpmNotifyType what, 
			 const unsigned long amount, 
			 const unsigned long total,
			 void * data);

static int hashesPrinted = 0;

static void printHash(const unsigned long amount, const unsigned long total) {
    int hashesNeeded;

    if (hashesPrinted != 50) {
	hashesNeeded = 50 * (total ? (((float) amount) / total) : 1);
	while (hashesNeeded > hashesPrinted) {
	    printf("#");
	    fflush(stdout);
	    hashesPrinted++;
	}
	fflush(stdout);
	hashesPrinted = hashesNeeded;

	if (hashesPrinted == 50)
	    fprintf(stdout, "\n");
    }
}

static void showProgress(const Header h, const rpmNotifyType what, 
			 const unsigned long amount, 
			 const unsigned long total,
			 void * data) {
    char * s;
    int flags = (int) data;

    switch (what) {
      case RPMNOTIFY_INST_START:
	hashesPrinted = 0;
	if (flags & INSTALL_LABEL) {
	    s = headerSprintf(h, "%{NAME}-%{VERSION}-%{RELEASE}", 
			      rpmTagTable, rpmHeaderFormats, NULL);
	    if (flags & INSTALL_HASH) 
		printf("%-28s", s);
	    else
		printf("%s\n", s);
	    fflush(stdout);
	    free(s);
	}
	break;

      case RPMNOTIFY_INST_PROGRESS:
	if (flags & INSTALL_PERCENT) {
	    fprintf(stdout, "%%%% %f\n", (total
				? ((float) ((((float) amount) / total) * 100))
				: 100.0));
	} else if (flags & INSTALL_HASH) {
	    printHash(amount, total);
	}
	break;
    }
}	

int doInstall(const char * rootdir, const char ** argv, int installFlags, 
	      int interfaceFlags, int probFilter, 
	      rpmRelocation * relocations) {
    rpmdb db = NULL;
    FD_t fd;
    int i;
    int mode, rc, major;
    const char ** packages, ** tmpPackages;
    const char ** filename;
    int numPackages;
    int numTmpPackages = 0, numBinaryPackages = 0, numSourcePackages = 0;
    int numFailed = 0;
    Header h;
    int isSource;
    int tmpnum = 0;
    rpmTransactionSet rpmdep;
    struct rpmDependencyConflict * conflicts;
    int numConflicts;
    int stopInstall = 0;
    size_t nb;
    int notifyFlags = interfaceFlags | (rpmIsVerbose ? INSTALL_LABEL : 0 );
    int transFlags = 0;
    rpmProblemSet probs, finalProbs;
    int dbIsOpen = 0;
    const char ** sourcePackages;

    if (installFlags & RPMINSTALL_TEST) 
	mode = O_RDONLY;
    else
	mode = O_RDWR | O_CREAT;

    if (installFlags & RPMINSTALL_TEST)
	transFlags |= RPMTRANS_FLAG_TEST;

    rpmMessage(RPMMESS_DEBUG, _("counting packages to install\n"));
    for (filename = argv, numPackages = 0; *filename; filename++, numPackages++)
	;

    rpmMessage(RPMMESS_DEBUG, _("found %d packages\n"), numPackages);

    nb = (numPackages + 1) * sizeof(char *);
    packages = alloca(nb);
    memset(packages, 0, nb);
    tmpPackages = alloca(nb);
    memset(tmpPackages, 0, nb);
    nb = (numPackages + 1) * sizeof(Header);

    rpmMessage(RPMMESS_DEBUG, _("looking for packages to download\n"));
    for (filename = argv, i = 0; *filename; filename++) {

	switch (urlIsURL(*filename)) {
	case URL_IS_FTP:
	case URL_IS_HTTP:
	case URL_IS_PATH:
	{   int myrc;
	    const char *tfn;
	    char tfnpid[64];
	    if (rpmIsVerbose()) {
		fprintf(stdout, _("Retrieving %s\n"), *filename);
	    }

	    sprintf(tfnpid, "rpm-%.5u-%.5u", (int) getpid(), tmpnum++);
	    /* XXX watchout for rootdir = NULL */
	    tfn = rpmGetPath( (rootdir ? rootdir : ""),
		"%{_tmppath}", tfnpid, NULL);

	    rpmMessage(RPMMESS_DEBUG, _(" ... as %s\n"), tfn);
	    myrc = urlGetFile(*filename, tfn);
	    if (myrc < 0) {
		rpmMessage(RPMMESS_ERROR, 
			_("skipping %s - transfer failed - %s\n"), 
			*filename, ftpStrerror(myrc));
		numFailed++;
		packages[i] = NULL;
		xfree(tfn);
	    } else {
		tmpPackages[numTmpPackages++] = packages[i++] = tfn;
	    }
	}   break;
	default:
	    packages[i++] = *filename;
	    break;
	}
    }

    sourcePackages = alloca(sizeof(*sourcePackages) * i);

    rpmMessage(RPMMESS_DEBUG, _("retrieved %d packages\n"), numTmpPackages);

    /* Build up the transaction set. As a special case, v1 source packages
       are installed right here, only because they don't have headers and
       would create all sorts of confusion later. */

    for (filename = packages; *filename; filename++) {
	fd = fdOpen(*filename, O_RDONLY, 0);
	if (fdFileno(fd) < 0) {
	    rpmMessage(RPMMESS_ERROR, _("cannot open file %s\n"), *filename);
	    numFailed++;
	    packages[i] = NULL;
	    continue;
	}

	rc = rpmReadPackageHeader(fd, &h, &isSource, &major, NULL);

	switch (rc) {
	case 1:
	    fdClose(fd);
	    rpmMessage(RPMMESS_ERROR, 
			_("%s does not appear to be a RPM package\n"), 
			*filename);
	    break;
	default:
	    rpmMessage(RPMMESS_ERROR, _("%s cannot be installed\n"), *filename);
	    numFailed++;
	    packages[i] = NULL;
	    break;
	case 0:
	    if (isSource && major == 1) {
		printf("XXX FIXME I can't install v1 source packages!!!\n");
	    } else if (isSource) {
		sourcePackages[numSourcePackages++] = *filename;
		fdClose(fd);
	    } else {
		if (!dbIsOpen) {
		    rpmMessage(RPMMESS_DEBUG, _("opening database "
				"mode: 0%o\n"), mode);
		    if (rpmdbOpen(rootdir, &db, mode, 0644)) {
			const char *dn;
			dn = rpmGetPath( (rootdir ? rootdir : ""), 
					"%{_dbpath}", NULL);
			rpmMessage(RPMMESS_ERROR, 
				_("cannot open %s/packages.rpm\n"), dn);
			xfree(dn);
			exit(EXIT_FAILURE);
		    }
		    rpmdep = rpmtransCreateSet(db, rootdir);
		    dbIsOpen = 1;
		}

		rpmtransAddPackage(rpmdep, h, fd,
			       packages[i], 
			       (interfaceFlags & INSTALL_UPGRADE) != 0,
			       relocations);
		numBinaryPackages++;
	    }
	    break;
	}
    }

    rpmMessage(RPMMESS_DEBUG, _("found %d source and %d binary packages\n"), 
		numSourcePackages, numBinaryPackages);

    if (numBinaryPackages && !(interfaceFlags & INSTALL_NODEPS)) {
	if (rpmdepCheck(rpmdep, &conflicts, &numConflicts)) {
	    numFailed = numPackages;
	    stopInstall = 1;
	}

	if (!stopInstall && conflicts) {
	    rpmMessage(RPMMESS_ERROR, _("failed dependencies:\n"));
	    printDepProblems(stderr, conflicts, numConflicts);
	    rpmdepFreeConflicts(conflicts, numConflicts);
	    numFailed = numPackages;
	    stopInstall = 1;
	}
    }

    if (numBinaryPackages && !(interfaceFlags & INSTALL_NOORDER)) {
	if (rpmdepOrder(rpmdep, (void ***) &packages)) {
	    numFailed = numPackages;
	    stopInstall = 1;
	}
    }

    if (numBinaryPackages && !stopInstall) {
	rpmMessage(RPMMESS_DEBUG, _("installing binary packages\n"));
	rc = rpmRunTransactions(rpmdep, showProgress, (void *) notifyFlags, 
				    NULL, &probs, transFlags);
	if (rc < 0) {
	    numFailed += numPackages;
	} else if (rc) {
	    rpmProblemSetFilter(probs, probFilter);

	    rc = rpmRunTransactions(rpmdep, showProgress, (void *) notifyFlags, 
					probs, &finalProbs, transFlags);
	    rpmProblemSetFree(probs);

	    if (rc < 0) {
		numFailed += numBinaryPackages;
	    } else if (rc) {
		numFailed += rc;
		for (i = 0; i < finalProbs->numProblems; i++)
		    if (!finalProbs->probs[i].ignoreProblem)
			rpmMessage(RPMMESS_ERROR, "%s\n", 
			        rpmProblemString(finalProbs->probs[i]));

		rpmProblemSetFree(finalProbs);
	    }
	}
    }

    if (numBinaryPackages) rpmtransFree(rpmdep);


    if (!stopInstall) {
	for (i = 0; i < numSourcePackages; i++) {
	    fd = fdOpen(sourcePackages[i], O_RDONLY, 0);
	    if (fdFileno(fd) < 0) {
		rpmMessage(RPMMESS_ERROR, _("cannot open file %s\n"), 
			   sourcePackages[i]);
		continue;
	    }

	    numFailed += rpmInstallSourcePackage(rootdir, fd, NULL,
			    showProgress, (void *) notifyFlags, NULL);

	    fdClose(fd);
	}
    }

    for (i = 0; i < numTmpPackages; i++) {
	unlink(tmpPackages[i]);
	xfree(tmpPackages[i]);
    }

    /* FIXME how do we close our various fd's? */

    if (db != NULL) rpmdbClose(db);

    return numFailed;
}

int doUninstall(const char * rootdir, const char ** argv, int uninstallFlags,
		 int interfaceFlags) {
    rpmdb db;
    dbiIndexSet matches;
    int i, j;
    int mode;
    int rc;
    int count;
    const char ** arg;
    int numFailed = 0;
    rpmTransactionSet rpmdep;
    struct rpmDependencyConflict * conflicts;
    int numConflicts;
    int stopUninstall = 0;
    int transFlags = 0;
    int numPackages = 0;
    rpmProblemSet probs;

    if (uninstallFlags & RPMUNINSTALL_TEST)
	transFlags |= RPMTRANS_FLAG_TEST;

    if (uninstallFlags & RPMUNINSTALL_TEST) 
	mode = O_RDONLY;
    else
	mode = O_RDWR | O_EXCL;
	
    if (rpmdbOpen(rootdir, &db, mode, 0644)) {
	const char *dn;
	dn = rpmGetPath( (rootdir ? rootdir : ""), "%{_dbpath}", NULL);
	rpmMessage(RPMMESS_ERROR, _("cannot open %s/packages.rpm\n"), dn);
	xfree(dn);
	exit(EXIT_FAILURE);
    }

    j = 0;
    rpmdep = rpmtransCreateSet(db, rootdir);
    for (arg = argv; *arg; arg++) {
	rc = rpmdbFindByLabel(db, *arg, &matches);
	switch (rc) {
	case 1:
	    rpmMessage(RPMMESS_ERROR, _("package %s is not installed\n"), *arg);
	    numFailed++;
	    break;
	case 2:
	    rpmMessage(RPMMESS_ERROR, _("searching for package %s\n"), *arg);
	    numFailed++;
	    break;
	default:
	    count = 0;
	    for (i = 0; i < dbiIndexSetCount(matches); i++)
		if (dbiIndexRecordOffset(matches, i)) count++;

	    if (count > 1 && !(interfaceFlags & UNINSTALL_ALLMATCHES)) {
		rpmMessage(RPMMESS_ERROR, _("\"%s\" specifies multiple packages\n"), 
			*arg);
		numFailed++;
	    }
	    else { 
		for (i = 0; i < dbiIndexSetCount(matches); i++) {
		    unsigned int recOffset = dbiIndexRecordOffset(matches, i);
		    if (recOffset) {
			rpmtransRemovePackage(rpmdep, recOffset);
			numPackages++;
		    }
		}
	    }

	    dbiFreeIndexRecord(matches);
	    break;
	}
    }

    if (!(interfaceFlags & UNINSTALL_NODEPS)) {
	if (rpmdepCheck(rpmdep, &conflicts, &numConflicts)) {
	    numFailed = numPackages;
	    stopUninstall = 1;
	}

	if (!stopUninstall && conflicts) {
	    rpmMessage(RPMMESS_ERROR, _("removing these packages would break "
			      "dependencies:\n"));
	    printDepProblems(stderr, conflicts, numConflicts);
	    rpmdepFreeConflicts(conflicts, numConflicts);
	    numFailed += numPackages;
	    stopUninstall = 1;
	}
    }

    if (!stopUninstall) {
	numFailed += rpmRunTransactions(rpmdep, NULL, NULL, NULL, &probs,
					transFlags);
    }

    rpmtransFree(rpmdep);
    rpmdbClose(db);

    return numFailed;
}

int doSourceInstall(const char * rootdir, const char * arg, const char ** specFile,
		    char ** cookie) {
    FD_t fd;
    int rc;

    fd = fdOpen(arg, O_RDONLY, 0);
    if (fdFileno(fd) < 0) {
	rpmMessage(RPMMESS_ERROR, _("cannot open %s\n"), arg);
	return 1;
    }

    if (rpmIsVerbose())
	fprintf(stdout, _("Installing %s\n"), arg);

    rc = rpmInstallSourcePackage(rootdir, fd, specFile, NULL, NULL, 
				 cookie);
    if (rc == 1) {
	rpmMessage(RPMMESS_ERROR, _("%s cannot be installed\n"), arg);
	if (specFile) FREE(*specFile);
	if (cookie) FREE(*cookie);
    }

    fdClose(fd);

    return rc;
}

void printDepFlags(FILE * f, const char * version, int flags) {
    if (flags)
	fprintf(f, " ");

    if (flags & RPMSENSE_LESS) 
	fprintf(f, "<");
    if (flags & RPMSENSE_GREATER)
	fprintf(f, ">");
    if (flags & RPMSENSE_EQUAL)
	fprintf(f, "=");
    if (flags & RPMSENSE_SERIAL)
	fprintf(f, "S");

    if (flags)
	fprintf(f, " %s", version);
}

static void printDepProblems(FILE * f, struct rpmDependencyConflict * conflicts,
			     int numConflicts) {
    int i;

    for (i = 0; i < numConflicts; i++) {
	fprintf(f, "\t%s", conflicts[i].needsName);
	if (conflicts[i].needsFlags) {
	    printDepFlags(stderr, conflicts[i].needsVersion, 
			  conflicts[i].needsFlags);
	}

	if (conflicts[i].sense == RPMDEP_SENSE_REQUIRES) 
	    fprintf(f, _(" is needed by %s-%s-%s\n"), conflicts[i].byName, 
		    conflicts[i].byVersion, conflicts[i].byRelease);
	else
	    fprintf(f, _(" conflicts with %s-%s-%s\n"), conflicts[i].byName, 
		    conflicts[i].byVersion, conflicts[i].byRelease);
    }
}
