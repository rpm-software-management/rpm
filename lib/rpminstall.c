/** \ingroup rpmcli
 * \file lib/rpminstall.c
 */

#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>
#include <rpmurl.h>

#include "misc.h"
#include "debug.h"

/*@access Header@*/		/* XXX compared with NULL */
/*@access FD_t@*/		/* XXX compared with NULL */

/* Define if you want percentage progress in the hash bars when
 * writing to a tty (ordinary hash bars otherwise) --claudio
 */
#define FANCY_HASH

static int hashesPrinted = 0;

#ifdef FANCY_HASH
static int packagesTotal = 0;
static int progressTotal = 0;
static int progressCurrent = 0;
#endif

static void printHash(const unsigned long amount, const unsigned long total)
{
    int hashesNeeded;
    int hashesTotal = 50;

#ifdef FANCY_HASH
    if (isatty (STDOUT_FILENO))
	hashesTotal = 44;
#endif

    if (hashesPrinted != hashesTotal) {
	hashesNeeded = hashesTotal * (total ? (((float) amount) / total) : 1);
	while (hashesNeeded > hashesPrinted) {
#ifdef FANCY_HASH
	    if (isatty (STDOUT_FILENO)) {
		int i;
		for (i = 0; i < hashesPrinted; i++) putchar ('#');
		for (; i < hashesTotal; i++) putchar (' ');
		printf ("(%3d%%)",
			(int)(100 * (total ? (((float) amount) / total) : 1)));
		for (i = 0; i < (hashesTotal + 6); i++) putchar ('\b');
	    } else
#endif
	    fprintf(stdout, "#");

	    fflush(stdout);
	    hashesPrinted++;
	}
	fflush(stdout);
	hashesPrinted = hashesNeeded;

	if (hashesPrinted == hashesTotal) {
#ifdef FANCY_HASH
	    int i;
	    progressCurrent++;
	    for (i = 1; i < hashesPrinted; i++) putchar ('#');
	    printf (" [%3d%%]\n", (int)(100 * (progressTotal ?
			(((float) progressCurrent) / progressTotal) : 1)));
#else
	    fprintf (stdout, "\n");
#endif
	}
    }
}

/**
 */
static void * showProgress(const void * arg, const rpmCallbackType what, 
			   const unsigned long amount, 
			   const unsigned long total,
			   const void * pkgKey, void * data)
{
    Header h = (Header) arg;
    char * s;
    int flags = (int) ((long)data);
    void * rc = NULL;
    const char * filename = pkgKey;
    static FD_t fd;

    switch (what) {
    case RPMCALLBACK_INST_OPEN_FILE:
	fd = Fopen(filename, "r.ufdio");
	if (fd)
	    fd = fdLink(fd, "persist (showProgress)");
	return fd;
	/*@notreached@*/ break;

    case RPMCALLBACK_INST_CLOSE_FILE:
	fd = fdFree(fd, "persist (showProgress)");
	if (fd) {
	    Fclose(fd);
	    fd = NULL;
	}
	break;

    case RPMCALLBACK_INST_START:
	hashesPrinted = 0;
	if (!(flags & INSTALL_LABEL))
	    break;
	if (flags & INSTALL_HASH) {
	    s = headerSprintf(h, "%{NAME}", rpmTagTable, rpmHeaderFormats,NULL);
#ifdef FANCY_HASH
	    if (isatty (STDOUT_FILENO))
		fprintf(stdout, "%4d:%-23.23s", progressCurrent + 1, s);
	    else
#else
		fprintf(stdout, "%-28s", s);
#endif
	    fflush(stdout);
	} else {
	    s = headerSprintf(h, "%{NAME}-%{VERSION}-%{RELEASE}", 
				  rpmTagTable, rpmHeaderFormats, NULL);
	    fprintf(stdout, "%s\n", s);
	}
	free(s);
	break;

    case RPMCALLBACK_TRANS_PROGRESS:
    case RPMCALLBACK_INST_PROGRESS:
	if (flags & INSTALL_PERCENT) {
	    fprintf(stdout, "%%%% %f\n", (total
				? ((float) ((((float) amount) / total) * 100))
				: 100.0));
	} else if (flags & INSTALL_HASH) {
	    printHash(amount, total);
	}
	break;

    case RPMCALLBACK_TRANS_START:
	hashesPrinted = 0;
#ifdef FANCY_HASH
	progressTotal = 1;
	progressCurrent = 0;
#endif
	if (!(flags & INSTALL_LABEL))
	    break;
	if (flags & INSTALL_HASH) {
	    fprintf(stdout, "%-28s", _("Preparing..."));
	    fflush(stdout);
	} else {
	    printf("%s\n", _("Preparing packages for installation..."));
	}
	break;

    case RPMCALLBACK_TRANS_STOP:
	if (flags & INSTALL_HASH)
	    printHash(1, 1);	/* Fixes "preparing..." progress bar */
#ifdef FANCY_HASH
	progressTotal = packagesTotal;
	progressCurrent = 0;
#endif
	break;

    case RPMCALLBACK_UNINST_PROGRESS:
    case RPMCALLBACK_UNINST_START:
    case RPMCALLBACK_UNINST_STOP:
	/* ignore */
	break;
    }

    return rc;
}	

/** @todo Generalize --freshen policies. */
int rpmInstall(const char * rootdir, const char ** fileArgv,
		rpmtransFlags transFlags, 
		rpmInstallInterfaceFlags interfaceFlags,
		rpmprobFilterFlags probFilter, 
		rpmRelocation * relocations)
{
    rpmdb db = NULL;
    FD_t fd;
    int i;
    int mode, rc, major;
    const char ** pkgURL = NULL;
    const char ** tmppkgURL = NULL;
    const char ** fileURL;
    int numPkgs;
    int numTmpPkgs = 0, numRPMS = 0, numSRPMS = 0;
    int numFailed = 0;
    Header h;
    int isSource;
    rpmTransactionSet ts = NULL;
    int numConflicts;
    int stopInstall = 0;
    int notifyFlags = interfaceFlags | (rpmIsVerbose() ? INSTALL_LABEL : 0 );
    int dbIsOpen = 0;
    const char ** sourceURL;
    rpmRelocation * defaultReloc;

    if (transFlags & RPMTRANS_FLAG_TEST) 
	mode = O_RDONLY;
    else
	mode = O_RDWR | O_CREAT;	/* XXX can't O_EXCL */

    for (defaultReloc = relocations; defaultReloc && defaultReloc->oldPath;
	 defaultReloc++);
    if (defaultReloc && !defaultReloc->newPath) defaultReloc = NULL;

    rpmMessage(RPMMESS_DEBUG, _("counting packages to install\n"));
    for (fileURL = fileArgv, numPkgs = 0; *fileURL; fileURL++, numPkgs++)
	;

    rpmMessage(RPMMESS_DEBUG, _("found %d packages\n"), numPkgs);

    pkgURL = xcalloc( (numPkgs + 1), sizeof(*pkgURL) );
    tmppkgURL = xcalloc( (numPkgs + 1), sizeof(*tmppkgURL) );

    rpmMessage(RPMMESS_DEBUG, _("looking for packages to download\n"));
    for (fileURL = fileArgv, i = 0; *fileURL; fileURL++) {

	switch (urlIsURL(*fileURL)) {
	case URL_IS_FTP:
	case URL_IS_HTTP:
	{   int myrc;
	    int j;
	    const char *tfn;
	    const char ** argv;
	    int argc;

	    myrc = rpmGlob(*fileURL, &argc, &argv);
	    if (myrc) {
		rpmMessage(RPMMESS_ERROR, 
			_("skipping %s - rpmGlob failed(%d)\n"),
			*fileURL, myrc);
		numFailed++;
		pkgURL[i] = NULL;
		break;
	    }
	    if (argc > 1) {
		numPkgs += argc - 1;
		pkgURL = xrealloc(pkgURL, (numPkgs + 1) * sizeof(*pkgURL));
		tmppkgURL = xrealloc(tmppkgURL, (numPkgs + 1) * sizeof(*tmppkgURL));
	    }

	    for (j = 0; j < argc; j++) {

		if (rpmIsVerbose())
		    fprintf(stdout, _("Retrieving %s\n"), argv[j]);

		{   char tfnbuf[64];
		    strcpy(tfnbuf, "rpm-xfer.XXXXXX");
		    /*@-unrecog@*/ mktemp(tfnbuf) /*@=unrecog@*/;
		    tfn = rpmGenPath(rootdir, "%{_tmppath}/", tfnbuf);
		}

		/* XXX undefined %{name}/%{version}/%{release} here */
		/* XXX %{_tmpdir} does not exist */
		rpmMessage(RPMMESS_DEBUG, _(" ... as %s\n"), tfn);
		myrc = urlGetFile(argv[j], tfn);
		if (myrc < 0) {
		    rpmMessage(RPMMESS_ERROR, 
			_("skipping %s - transfer failed - %s\n"), 
			argv[j], ftpStrerror(myrc));
		    numFailed++;
		    pkgURL[i] = NULL;
		    free((void *)tfn);
		} else {
		    tmppkgURL[numTmpPkgs++] = pkgURL[i++] = tfn;
		}
	    }
	    if (argv) {
		for (j = 0; j < argc; j++)
		    free((void *)argv[j]);
		free((void *)argv);
		argv = NULL;
	    }
	}   break;
	case URL_IS_PATH:
	default:
	    pkgURL[i++] = *fileURL;
	    break;
	}
    }
    pkgURL[i] = NULL;
    tmppkgURL[numTmpPkgs] = NULL;

    sourceURL = alloca(sizeof(*sourceURL) * i);

    rpmMessage(RPMMESS_DEBUG, _("retrieved %d packages\n"), numTmpPkgs);

    if (numFailed) goto errxit;

    /**
     * Build up the transaction set. As a special case, v1 source packages
     * are installed right here, only because they don't have headers and
     * would create all sorts of confusion later.
     */
    for (fileURL = pkgURL; *fileURL; fileURL++) {
	const char * fileName;
	rpmRC rpmrc;

	(void) urlPath(*fileURL, &fileName);
	fd = Fopen(*fileURL, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmMessage(RPMMESS_ERROR, _("cannot open file %s: %s\n"),
				*fileURL, Fstrerror(fd));
	    if (fd) Fclose(fd);
	    numFailed++;
	    pkgURL[i] = NULL;
	    continue;
	}

	rpmrc = rpmReadPackageHeader(fd, &h, &isSource, &major, NULL);

	switch (rpmrc) {
	case RPMRC_BADMAGIC:
	    Fclose(fd);
	    rpmMessage(RPMMESS_ERROR, 
			_("%s does not appear to be a RPM package\n"), 
			*fileURL);
	    numFailed++;
	    pkgURL[i] = NULL;
	    break;
	case RPMRC_FAIL:
	case RPMRC_SHORTREAD:
	default:
	    rpmMessage(RPMMESS_ERROR, _("%s cannot be installed\n"), *fileURL);
	    numFailed++;
	    pkgURL[i] = NULL;
	    break;
	case RPMRC_BADSIZE:
	case RPMRC_OK:
	    if (isSource) {
		sourceURL[numSRPMS++] = fileName;
		Fclose(fd);
	    } else {
		if (!dbIsOpen) {
		    if (rpmdbOpen(rootdir, &db, mode, 0644)) {
			const char *dn;
			dn = rpmGetPath( (rootdir ? rootdir : ""), 
					"%{_dbpath}", NULL);
			rpmMessage(RPMMESS_ERROR, 
				_("cannot open Packages database in %s\n"), dn);
			free((void *)dn);
			numFailed++;
			pkgURL[i] = NULL;
			break;
		    }
		    ts = rpmtransCreateSet(db, rootdir);
		    dbIsOpen = 1;
		}

		if (defaultReloc) {
		    const char ** paths;
		    int c;

		    if (headerGetEntry(h, RPMTAG_PREFIXES, NULL,
				       (void **) &paths, &c) && (c == 1)) {
			defaultReloc->oldPath = xstrdup(paths[0]);
			free((void *)paths);
		    } else {
			const char * name;
			headerNVR(h, &name, NULL, NULL);
			rpmMessage(RPMMESS_ERROR, 
			       _("package %s is not relocateable\n"), name);

			goto errxit;
			/*@notreached@*/
		    }
		}

		/* On --freshen, verify package is installed and newer */
		if (interfaceFlags & INSTALL_FRESHEN) {
		    rpmdbMatchIterator mi;
		    const char * name;
		    Header oldH;
		    int count;

		    headerNVR(h, &name, NULL, NULL);
		    mi = rpmdbInitIterator(db, RPMTAG_NAME, name, 0);
		    count = rpmdbGetIteratorCount(mi);
		    while ((oldH = rpmdbNextIterator(mi)) != NULL) {
			if (rpmVersionCompare(oldH, h) < 0)
			    continue;
			/* same or newer package already installed */
			count = 0;
			break;
		    }
		    rpmdbFreeIterator(mi);
		    if (count == 0) {
			headerFree(h);
			Fclose(fd);
			break;	/* XXX out of switch */
		    }
		    /* Package is newer than those currently installed. */
		}

		rc = rpmtransAddPackage(ts, h, NULL, fileName,
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
			goto errxit;
			/*@notreached@*/ break;
		case 2:
			rpmMessage(RPMMESS_ERROR, 
			    _("file %s requires a newer version of RPM\n"),
			    *fileURL);
			goto errxit;
			/*@notreached@*/ break;
		}

		if (defaultReloc) {
		    free((void *)defaultReloc->oldPath);
		    defaultReloc->oldPath = NULL;
		}

		numRPMS++;
	    }
	    break;
	}
    }

    rpmMessage(RPMMESS_DEBUG, _("found %d source and %d binary packages\n"), 
		numSRPMS, numRPMS);

    if (numFailed) goto errxit;

    if (numRPMS && !(interfaceFlags & INSTALL_NODEPS)) {
	struct rpmDependencyConflict * conflicts;
	if (rpmdepCheck(ts, &conflicts, &numConflicts)) {
	    numFailed = numPkgs;
	    stopInstall = 1;
	}

	if (!stopInstall && conflicts) {
	    rpmMessage(RPMMESS_ERROR, _("failed dependencies:\n"));
	    printDepProblems(stderr, conflicts, numConflicts);
	    rpmdepFreeConflicts(conflicts, numConflicts);
	    numFailed = numPkgs;
	    stopInstall = 1;
	}
    }

    if (numRPMS && !(interfaceFlags & INSTALL_NOORDER)) {
	if (rpmdepOrder(ts)) {
	    numFailed = numPkgs;
	    stopInstall = 1;
	}
    }

    if (numRPMS && !stopInstall) {
	rpmProblemSet probs = NULL;

#ifdef FANCY_HASH
	packagesTotal = numRPMS;
#endif
	rpmMessage(RPMMESS_DEBUG, _("installing binary packages\n"));
	rc = rpmRunTransactions(ts, showProgress, (void *) ((long)notifyFlags), 
				    NULL, &probs, transFlags, probFilter);

	if (rc < 0) {
	    numFailed += numRPMS;
	} else if (rc > 0) {
	    numFailed += rc;
	    rpmProblemSetPrint(stderr, probs);
	}

	if (probs) rpmProblemSetFree(probs);
    }

    if (numRPMS && ts) rpmtransFree(ts);

    if (numSRPMS && !stopInstall) {
	for (i = 0; i < numSRPMS; i++) {
	    fd = Fopen(sourceURL[i], "r.ufdio");
	    if (fd == NULL || Ferror(fd)) {
		rpmMessage(RPMMESS_ERROR, _("cannot open file %s: %s\n"), 
			   sourceURL[i], Fstrerror(fd));
		if (fd) Fclose(fd);
		continue;
	    }

	    if (!(transFlags & RPMTRANS_FLAG_TEST)) {
		rpmRC rpmrc = rpmInstallSourcePackage(rootdir, fd, NULL,
			showProgress, (void *) ((long)notifyFlags), NULL);
		if (rpmrc != RPMRC_OK) numFailed++;
	    }

	    Fclose(fd);
	}
    }

    for (i = 0; i < numTmpPkgs; i++) {
	Unlink(tmppkgURL[i]);
	free((void *)tmppkgURL[i]);
    }
    free((void *)tmppkgURL);	tmppkgURL = NULL;
    free((void *)pkgURL);	pkgURL = NULL;

    /* FIXME how do we close our various fd's? */

    if (dbIsOpen) rpmdbClose(db);

    return numFailed;

errxit:
    if (numRPMS && ts) rpmtransFree(ts);
    if (tmppkgURL) {
	for (i = 0; i < numTmpPkgs; i++)
	    free((void *)tmppkgURL[i]);
	free((void *)tmppkgURL);
    }
    if (pkgURL)
	free((void *)pkgURL);
    if (dbIsOpen) rpmdbClose(db);
    return numPkgs;
}

int rpmErase(const char * rootdir, const char ** argv,
		rpmtransFlags transFlags, 
		rpmEraseInterfaceFlags interfaceFlags)
{
    rpmdb db;
    int mode;
    int count;
    const char ** arg;
    int numFailed = 0;
    rpmTransactionSet ts;
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
	free((void *)dn);
	return -1;
    }

    ts = rpmtransCreateSet(db, rootdir);
    for (arg = argv; *arg; arg++) {
	rpmdbMatchIterator mi;

	/* XXX HACK to get rpmdbFindByLabel out of the API */
	mi = rpmdbInitIterator(db, RPMDBI_LABEL, *arg, 0);
	count = rpmdbGetIteratorCount(mi);
	if (count <= 0) {
	    rpmMessage(RPMMESS_ERROR, _("package %s is not installed\n"), *arg);
	    numFailed++;
	} else if (!(count == 1 || (interfaceFlags & UNINSTALL_ALLMATCHES))) {
	    rpmMessage(RPMMESS_ERROR, _("\"%s\" specifies multiple packages\n"),
			*arg);
	    numFailed++;
	} else {
	    Header h;	/* XXX iterator owns the reference */
	    while ((h = rpmdbNextIterator(mi)) != NULL) {
		unsigned int recOffset = rpmdbGetIteratorOffset(mi);
		if (recOffset) {
		    rpmtransRemovePackage(ts, recOffset);
		    numPackages++;
		}
	    }
	}
	rpmdbFreeIterator(mi);
    }

    if (!(interfaceFlags & UNINSTALL_NODEPS)) {
	if (rpmdepCheck(ts, &conflicts, &numConflicts)) {
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
	transFlags |= RPMTRANS_FLAG_REVERSE;
	numFailed += rpmRunTransactions(ts, NULL, NULL, NULL, &probs,
					transFlags, 0);
    }

    rpmtransFree(ts);
    rpmdbClose(db);

    return numFailed;
}

int rpmInstallSource(const char * rootdir, const char * arg,
		const char ** specFile, char ** cookie)
{
    FD_t fd;
    int rc;

    fd = Fopen(arg, "r.ufdio");
    if (fd == NULL || Ferror(fd)) {
	rpmMessage(RPMMESS_ERROR, _("cannot open %s: %s\n"), arg, Fstrerror(fd));
	if (fd) Fclose(fd);
	return 1;
    }

    if (rpmIsVerbose())
	fprintf(stdout, _("Installing %s\n"), arg);

    {	rpmRC rpmrc = rpmInstallSourcePackage(rootdir, fd, specFile, NULL, NULL,
				 cookie);
	rc = (rpmrc == RPMRC_OK ? 0 : 1);
    }
    if (rc != 0) {
	rpmMessage(RPMMESS_ERROR, _("%s cannot be installed\n"), arg);
	if (specFile && *specFile) {
	    free((void *)*specFile);
	    *specFile = NULL;
	}
	if (cookie && *cookie) {
	    free(*cookie);
	    *cookie = NULL;
	}
    }

    Fclose(fd);

    return rc;
}
