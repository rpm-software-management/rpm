#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>
#include <rpmurl.h>

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

static void * showProgress(const Header h, const rpmCallbackType what, 
			   const unsigned long amount, 
			   const unsigned long total,
			   const void * pkgKey, void * data) {
    char * s;
    int flags = (int) data;
    void * rc = NULL;
    const char * filename = pkgKey;
    static FD_t fd;

    switch (what) {
      case RPMCALLBACK_INST_OPEN_FILE:
	fd = Fopen(filename, "r.ufdio");
	fd = fdLink(fd, "persist (showProgress)");
	return fd;

      case RPMCALLBACK_INST_CLOSE_FILE:
	fd = fdFree(fd, "persist (showProgress)");
	if (fd) {
	    Fclose(fd);
	    fd = NULL;
	}
	break;

      case RPMCALLBACK_INST_START:
	hashesPrinted = 0;
	if (flags & INSTALL_LABEL) {
	    if (flags & INSTALL_HASH) {
		s = headerSprintf(h, "%{NAME}",
				  rpmTagTable, rpmHeaderFormats, NULL);
		printf("%-28s", s);
		fflush(stdout);
	    } else {
		s = headerSprintf(h, "%{NAME}-%{VERSION}-%{RELEASE}", 
				  rpmTagTable, rpmHeaderFormats, NULL);
		printf("%s\n", s);
	    }
	    free(s);
	}
	break;

      case RPMCALLBACK_INST_PROGRESS:
	if (flags & INSTALL_PERCENT) {
	    fprintf(stdout, "%%%% %f\n", (total
				? ((float) ((((float) amount) / total) * 100))
				: 100.0));
	} else if (flags & INSTALL_HASH) {
	    printHash(amount, total);
	}
	break;

      case RPMCALLBACK_TRANS_PROGRESS:
      case RPMCALLBACK_TRANS_START:
      case RPMCALLBACK_TRANS_STOP:
      case RPMCALLBACK_UNINST_PROGRESS:
      case RPMCALLBACK_UNINST_START:
      case RPMCALLBACK_UNINST_STOP:
	/* ignore */
	break;
    }

    return rc;
}	

int rpmInstall(const char * rootdir, const char ** argv, int transFlags, 
	      int interfaceFlags, int probFilter, 
	      rpmRelocation * relocations)
{
    rpmdb db = NULL;
    FD_t fd;
    int i;
    int mode, rc, major;
    const char ** pkgURL, ** tmppkgURL;
    const char ** fileURL;
    int numPackages;
    int numTmpPackages = 0, numBinaryPackages = 0, numSourcePackages = 0;
    int numFailed = 0;
    Header h;
    int isSource;
    rpmTransactionSet rpmdep = NULL;
    int numConflicts;
    int stopInstall = 0;
    size_t nb;
    int notifyFlags = interfaceFlags | (rpmIsVerbose() ? INSTALL_LABEL : 0 );
    int dbIsOpen = 0;
    const char ** sourceURL;
    rpmRelocation * defaultReloc;

    if (transFlags & RPMTRANS_FLAG_TEST) 
	mode = O_RDONLY;
    else
	mode = O_RDWR | O_CREAT;

    for (defaultReloc = relocations; defaultReloc && defaultReloc->oldPath;
	 defaultReloc++);
    if (defaultReloc && !defaultReloc->newPath) defaultReloc = NULL;

    rpmMessage(RPMMESS_DEBUG, _("counting packages to install\n"));
    for (fileURL = argv, numPackages = 0; *fileURL; fileURL++, numPackages++)
	;

    rpmMessage(RPMMESS_DEBUG, _("found %d packages\n"), numPackages);

    nb = (numPackages + 1) * sizeof(char *);
    pkgURL = alloca(nb);
    memset(pkgURL, 0, nb);
    tmppkgURL = alloca(nb);
    memset(tmppkgURL, 0, nb);
    nb = (numPackages + 1) * sizeof(Header);

    rpmMessage(RPMMESS_DEBUG, _("looking for packages to download\n"));
    for (fileURL = argv, i = 0; *fileURL; fileURL++) {

	switch (urlIsURL(*fileURL)) {
	case URL_IS_FTP:
	case URL_IS_HTTP:
	{   int myrc;
	    const char *tfn;

	    if (rpmIsVerbose())
		fprintf(stdout, _("Retrieving %s\n"), *fileURL);

#ifdef	DYING
	    {	char tfnbuf[64];
		const char * tempfn;
		strcpy(tfnbuf, "rpm-xfer.XXXXXX");
		/* XXX watchout for rootdir = NULL */
		tfn = tempfn = rpmGetPath("%{_tmppath}/", mktemp(tfnbuf), NULL);
		switch(urlIsURL(tempfn)) {
		case URL_IS_FTP:
		case URL_IS_HTTP:
		case URL_IS_DASH:
		    break;
		case URL_IS_PATH:
		    tfn += sizeof("file://") - 1;
		    tfn = strchr(tfn, '/');
		    /*@fallthrough@*/
		case URL_IS_UNKNOWN:
		    if (rootdir && rootdir[strlen(rootdir) - 1] == '/')
			while (*tfn == '/') tfn++;
		    tfn = rpmGetPath( (rootdir ? rootdir : ""), tfn, NULL);
		    xfree(tempfn);
		    break;
		}
	    }
#else
	    {	char tfnbuf[64];
		strcpy(tfnbuf, "rpm-xfer.XXXXXX");
		tfn = rpmGenPath(rootdir, "%{_tmppath}/", mktemp(tfnbuf));
	    }
#endif

	    /* XXX undefined %{name}/%{version}/%{release} here */
	    /* XXX %{_tmpdir} does not exist */
	    rpmMessage(RPMMESS_DEBUG, _(" ... as %s\n"), tfn);
	    myrc = urlGetFile(*fileURL, tfn);
	    if (myrc < 0) {
		rpmMessage(RPMMESS_ERROR, 
			_("skipping %s - transfer failed - %s\n"), 
			*fileURL, ftpStrerror(myrc));
		numFailed++;
		pkgURL[i] = NULL;
		xfree(tfn);
	    } else {
		tmppkgURL[numTmpPackages++] = pkgURL[i++] = tfn;
	    }
	}   break;
	case URL_IS_PATH:
	default:
	    pkgURL[i++] = *fileURL;
	    break;
	}
    }

    sourceURL = alloca(sizeof(*sourceURL) * i);

    rpmMessage(RPMMESS_DEBUG, _("retrieved %d packages\n"), numTmpPackages);

    /* Build up the transaction set. As a special case, v1 source packages
       are installed right here, only because they don't have headers and
       would create all sorts of confusion later. */

    for (fileURL = pkgURL; *fileURL; fileURL++) {
	const char * fileName;
	(void) urlPath(*fileURL, &fileName);
	fd = Fopen(*fileURL, "r.ufdio");
	if (Ferror(fd)) {
	    rpmMessage(RPMMESS_ERROR, _("cannot open file %s: %s\n"), *fileURL,
		Fstrerror(fd));
	    numFailed++;
	    pkgURL[i] = NULL;
	    continue;
	}

	rc = rpmReadPackageHeader(fd, &h, &isSource, &major, NULL);

	switch (rc) {
	case 1:
	    Fclose(fd);
	    rpmMessage(RPMMESS_ERROR, 
			_("%s does not appear to be a RPM package\n"), 
			*fileURL);
	    break;
	default:
	    rpmMessage(RPMMESS_ERROR, _("%s cannot be installed\n"), *fileURL);
	    numFailed++;
	    pkgURL[i] = NULL;
	    break;
	case 0:
	    if (isSource) {
		sourceURL[numSourcePackages++] = fileName;
		Fclose(fd);
	    } else {
		if (!dbIsOpen) {
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

		if (defaultReloc) {
		    char ** paths;
		    char * name;
		    int c;

		    if (headerGetEntry(h, RPMTAG_PREFIXES, NULL,
				       (void **) &paths, &c) && (c == 1)) {
			defaultReloc->oldPath = xstrdup(paths[0]);
			free(paths);
		    } else {
			headerGetEntry(h, RPMTAG_NAME, NULL, (void **) &name,
				       NULL);
			rpmMessage(RPMMESS_ERROR, 
			       _("package %s is not relocateable\n"), name);

			return numPackages;
		    }
		}

		rc = rpmtransAddPackage(rpmdep, h, NULL, fileName,
			       (interfaceFlags & INSTALL_UPGRADE) != 0,
			       relocations);

		headerFree(h);	/* XXX reference held by transaction set */
		Fclose(fd);

		switch(rc) {
		case 0:
			break;
		case 1:
			rpmMessage(RPMMESS_ERROR, 
			    _("error reading from file %s\n"), *fileURL);
			return numPackages;
			/*@notreached@*/ break;
		case 2:
			rpmMessage(RPMMESS_ERROR, 
			    _("file %s requires a newer version of RPM\n"),
			    *fileURL);
			return numPackages;
			/*@notreached@*/ break;
		}

		if (defaultReloc) {
		    xfree(defaultReloc->oldPath);
		    defaultReloc->oldPath = NULL;
		}

		numBinaryPackages++;
	    }
	    break;
	}
    }

    rpmMessage(RPMMESS_DEBUG, _("found %d source and %d binary packages\n"), 
		numSourcePackages, numBinaryPackages);

    if (numBinaryPackages && !(interfaceFlags & INSTALL_NODEPS)) {
	struct rpmDependencyConflict * conflicts;
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
	if (rpmdepOrder(rpmdep)) {
	    numFailed = numPackages;
	    stopInstall = 1;
	}
    }

    if (numBinaryPackages && !stopInstall) {
	rpmProblemSet probs = NULL;
;
	rpmMessage(RPMMESS_DEBUG, _("installing binary packages\n"));
	rc = rpmRunTransactions(rpmdep, showProgress, (void *) notifyFlags, 
				    NULL, &probs, transFlags, probFilter);

	if (rc < 0) {
	    numFailed += numBinaryPackages;
	} else if (rc) {
	    numFailed += rc;
	    rpmProblemSetPrint(stderr, probs);
	}

	if (probs) rpmProblemSetFree(probs);
    }

    if (numBinaryPackages) rpmtransFree(rpmdep);

    if (numSourcePackages && !stopInstall) {
	for (i = 0; i < numSourcePackages; i++) {
	    fd = Fopen(sourceURL[i], "r.ufdio");
	    if (Ferror(fd)) {
		rpmMessage(RPMMESS_ERROR, _("cannot open file %s: %s\n"), 
			   sourceURL[i], Fstrerror(fd));
		continue;
	    }

	    if (!(transFlags & RPMTRANS_FLAG_TEST))
		numFailed += rpmInstallSourcePackage(rootdir, fd, NULL,
				showProgress, (void *) notifyFlags, NULL);

	    Fclose(fd);
	}
    }

    for (i = 0; i < numTmpPackages; i++) {
	Unlink(tmppkgURL[i]);
	xfree(tmppkgURL[i]);
    }

    /* FIXME how do we close our various fd's? */

    if (dbIsOpen) rpmdbClose(db);

    return numFailed;
}

int rpmErase(const char * rootdir, const char ** argv, int transFlags,
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
    int numPackages = 0;
    rpmProblemSet probs;

    if (transFlags & RPMTRANS_FLAG_TEST) 
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
					transFlags, 0);
    }

    rpmtransFree(rpmdep);
    rpmdbClose(db);

    return numFailed;
}

int rpmInstallSource(const char * rootdir, const char * arg, const char ** specFile,
		    char ** cookie) {
    FD_t fd;
    int rc;

    fd = Fopen(arg, "r.ufdio");
    if (Ferror(fd)) {
	rpmMessage(RPMMESS_ERROR, _("cannot open %s: %s\n"), arg, Fstrerror(fd));
	Fclose(fd);
	return 1;
    }

    if (rpmIsVerbose())
	fprintf(stdout, _("Installing %s\n"), arg);

    rc = rpmInstallSourcePackage(rootdir, fd, specFile, NULL, NULL, 
				 cookie);
    if (rc == 1) {
	rpmMessage(RPMMESS_ERROR, _("%s cannot be installed\n"), arg);
	if (specFile && *specFile) xfree(*specFile);
	if (cookie && *cookie) free(*cookie);
    }

    Fclose(fd);

    return rc;
}
