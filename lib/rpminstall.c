/** \ingroup rpmcli
 * \file lib/rpminstall.c
 */

#include "system.h"

#include <rpmlib.h>
#include <rpmmacro.h>
#include <rpmurl.h>

#include "manifest.h"
#include "misc.h"
#include "debug.h"

/*@access rpmTransactionSet@*/	/* XXX compared with NULL */
/*@access rpmProblemSet@*/	/* XXX compared with NULL */
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

/**
 */
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
		for (i = 0; i < hashesPrinted; i++) (void) putchar ('#');
		for (; i < hashesTotal; i++) (void) putchar (' ');
		printf ("(%3d%%)",
			(int)(100 * (total ? (((float) amount) / total) : 1)));
		for (i = 0; i < (hashesTotal + 6); i++) (void) putchar ('\b');
	    } else
#endif
	    fprintf(stdout, "#");

	    hashesPrinted++;
	}
	(void) fflush(stdout);
	hashesPrinted = hashesNeeded;

	if (hashesPrinted == hashesTotal) {
#ifdef FANCY_HASH
	    int i;
	    progressCurrent++;
	    for (i = 1; i < hashesPrinted; i++) (void) putchar ('#');
	    printf (" [%3d%%]\n", (int)(100 * (progressTotal ?
			(((float) progressCurrent) / progressTotal) : 1)));
#else
	    fprintf (stdout, "\n");
#endif
	}
	(void) fflush(stdout);
    }
}

/**
 */
static /*@null@*/
void * showProgress(/*@null@*/ const void * arg, const rpmCallbackType what,
			const unsigned long amount,
			const unsigned long total,
			/*@null@*/ const void * pkgKey,
			/*@null@*/ void * data)
{
    Header h = (Header) arg;
    char * s;
    int flags = (int) ((long)data);
    void * rc = NULL;
    const char * filename = pkgKey;
    static FD_t fd = NULL;

    switch (what) {
    case RPMCALLBACK_INST_OPEN_FILE:
	if (filename == NULL || filename[0] == '\0')
	    return NULL;
	fd = Fopen(filename, "r.ufdio");
	if (fd)
	    fd = fdLink(fd, "persist (showProgress)");
	return fd;
	/*@notreached@*/ break;

    case RPMCALLBACK_INST_CLOSE_FILE:
	fd = fdFree(fd, "persist (showProgress)");
	if (fd) {
	    (void) Fclose(fd);
	    fd = NULL;
	}
	break;

    case RPMCALLBACK_INST_START:
	hashesPrinted = 0;
	if (h == NULL || !(flags & INSTALL_LABEL))
	    break;
	if (flags & INSTALL_HASH) {
	    s = headerSprintf(h, "%{NAME}",
				rpmTagTable, rpmHeaderFormats, NULL);
#ifdef FANCY_HASH
	    if (isatty (STDOUT_FILENO))
		fprintf(stdout, "%4d:%-23.23s", progressCurrent + 1, s);
	    else
#else
		fprintf(stdout, "%-28s", s);
#endif
	    (void) fflush(stdout);
	    s = _free(s);
	} else {
	    s = headerSprintf(h, "%{NAME}-%{VERSION}-%{RELEASE}",
				  rpmTagTable, rpmHeaderFormats, NULL);
	    fprintf(stdout, "%s\n", s);
	    (void) fflush(stdout);
	    s = _free(s);
	}
	break;

    case RPMCALLBACK_TRANS_PROGRESS:
    case RPMCALLBACK_INST_PROGRESS:
	if (flags & INSTALL_PERCENT)
	    fprintf(stdout, "%%%% %f\n", (total
				? ((float) ((((float) amount) / total) * 100))
				: 100.0));
	else if (flags & INSTALL_HASH)
	    printHash(amount, total);
	(void) fflush(stdout);
	break;

    case RPMCALLBACK_TRANS_START:
	hashesPrinted = 0;
#ifdef FANCY_HASH
	progressTotal = 1;
	progressCurrent = 0;
#endif
	if (!(flags & INSTALL_LABEL))
	    break;
	if (flags & INSTALL_HASH)
	    fprintf(stdout, "%-28s", _("Preparing..."));
	else
	    printf("%s\n", _("Preparing packages for installation..."));
	(void) fflush(stdout);
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
    rpmTransactionSet ts = NULL;
    int notifyFlags = interfaceFlags | (rpmIsVerbose() ? INSTALL_LABEL : 0 );
    rpmdb db = NULL;
    const char ** pkgURL = NULL;
    char * pkgState = NULL;
    const char ** fnp;
    const char * fileURL = NULL;
    int numPkgs = 0;
    int numRPMS = 0;
    int numSRPMS = 0;
    int numFailed = 0;
    int stopInstall = 0;
    int dbIsOpen = 0;
    rpmRelocation * defaultReloc = relocations;
    const char ** sourceURL = NULL;
    int prevx;
    int pkgx;
    const char ** argv = NULL;
    int argc = 0;
    const char ** av = NULL;
    int ac = 0;
    Header h;
    FD_t fd;
    int rc;
    int i;

    if (fileArgv == NULL) return 0;

    while (defaultReloc && defaultReloc->oldPath)
	defaultReloc++;
    if (defaultReloc && !defaultReloc->newPath) defaultReloc = NULL;

    /* Build fully globbed list of arguments in argv[argc]. */
    for (fnp = fileArgv; *fnp; fnp++) {
	av = _free(av);
	ac = 0;
	rc = rpmGlob(*fnp, &ac, &av);
	if (rc || ac == 0) continue;

	if (argv == NULL)
	    argv = xmalloc((argc+ac+1) * sizeof(*argv));
	else
	    argv = xrealloc(argv, (argc+ac+1) * sizeof(*argv));
	memcpy(argv+argc, av, ac * sizeof(*av));
	argc += ac;
	argv[argc] = NULL;
    }
    av = _free(av);

    numPkgs = 0;
    prevx = 0;
    pkgx = 0;

restart:
    /* Allocate sufficient storage for next set of args. */
    if (pkgx >= numPkgs) {
	numPkgs = pkgx + argc;
	if (pkgURL == NULL)
	    pkgURL = xmalloc( (numPkgs + 1) * sizeof(*pkgURL));
	else
	    pkgURL = xrealloc(pkgURL, (numPkgs + 1) * sizeof(*pkgURL));
	memset(pkgURL + pkgx, 0, ((argc + 1) * sizeof(*pkgURL)));
	if (pkgState == NULL)
	    pkgState = xmalloc( (numPkgs + 1) * sizeof(*pkgState));
	else
	    pkgState = xrealloc(pkgState, (numPkgs + 1) * sizeof(*pkgState));
	memset(pkgState + pkgx, 0, ((argc + 1) * sizeof(*pkgState)));
    }

    /* Retrieve next set of args, cache on local storage. */
    for (i = 0; i < argc; i++) {
	fileURL = _free(fileURL);
	fileURL = argv[i];
	argv[i] = NULL;

	switch (urlIsURL(fileURL)) {
	case URL_IS_FTP:
	case URL_IS_HTTP:
	{   const char *tfn;

	    if (rpmIsVerbose())
		fprintf(stdout, _("Retrieving %s\n"), fileURL);

	    {	char tfnbuf[64];
		strcpy(tfnbuf, "rpm-xfer.XXXXXX");
		/*@-unrecog@*/ mktemp(tfnbuf) /*@=unrecog@*/;
		tfn = rpmGenPath(rootdir, "%{_tmppath}/", tfnbuf);
	    }

	    /* XXX undefined %{name}/%{version}/%{release} here */
	    /* XXX %{_tmpdir} does not exist */
	    rpmMessage(RPMMESS_DEBUG, _(" ... as %s\n"), tfn);
	    rc = urlGetFile(fileURL, tfn);
	    if (rc < 0) {
		rpmMessage(RPMMESS_ERROR,
			_("skipping %s - transfer failed - %s\n"),
			fileURL, ftpStrerror(rc));
		numFailed++;
		pkgURL[pkgx] = NULL;
		tfn = _free(tfn);
		break;
	    }
	    pkgState[pkgx] = 1;
	    pkgURL[pkgx] = tfn;
	    pkgx++;
	}   break;
	case URL_IS_PATH:
	default:
	    pkgURL[pkgx] = fileURL;
	    fileURL = NULL;
	    pkgx++;
	    break;
	}
    }
    fileURL = _free(fileURL);

    if (numFailed) goto exit;

    /* Continue processing file arguments, building transaction set. */
    for (fnp = pkgURL+prevx; *fnp != NULL; fnp++, prevx++) {
	const char * fileName;
	rpmRC rpmrc;
	int isSource;

	rpmMessage(RPMMESS_DEBUG, "============== %s\n", *fnp);
	(void) urlPath(*fnp, &fileName);

	/* Try to read the header from a package file. */
	fd = Fopen(*fnp, "r.ufdio");
	if (fd == NULL || Ferror(fd)) {
	    rpmError(RPMERR_OPEN, _("open of %s failed: %s\n"), *fnp,
			Fstrerror(fd));
	    if (fd) (void) Fclose(fd);
	    numFailed++; *fnp = NULL;
	    continue;
	}

	rpmrc = rpmReadPackageHeader(fd, &h, &isSource, NULL, NULL);
	(void) Fclose(fd);

	if (rpmrc == RPMRC_FAIL || rpmrc == RPMRC_SHORTREAD) {
	    numFailed++; *fnp = NULL;
	    continue;
	}
	if ((rpmrc == RPMRC_OK || rpmrc == RPMRC_BADSIZE) && isSource) {
	    rpmMessage(RPMMESS_DEBUG, "\tadded source package [%d]\n",
		numSRPMS);
	    if (sourceURL == NULL)
		sourceURL = xmalloc((numSRPMS + 2) * sizeof(*sourceURL));
	    else
		sourceURL = xrealloc(sourceURL,
				(numSRPMS + 2) * sizeof(*sourceURL));
	    sourceURL[numSRPMS++] = *fnp;
	    sourceURL[numSRPMS] = NULL;
	    *fnp = NULL;
	    continue;
	}
	if (rpmrc == RPMRC_OK || rpmrc == RPMRC_BADSIZE) {
	    if (!dbIsOpen) {
		int mode = (transFlags & RPMTRANS_FLAG_TEST)
				? O_RDONLY : (O_RDWR | O_CREAT);

		if (rpmdbOpen(rootdir, &db, mode, 0644)) {
		    const char *dn;
		    dn = rpmGetPath( (rootdir ? rootdir : ""),
					"%{_dbpath}", NULL);
		    rpmMessage(RPMMESS_ERROR,
				_("cannot open Packages database in %s\n"), dn);
		    dn = _free(dn);
		    numFailed++; *fnp = NULL;
		    break;
		}
		ts = rpmtransCreateSet(db, rootdir);
		dbIsOpen = 1;
	    }

	    if (defaultReloc) {
		const char ** paths;
		int pft;
		int c;

		if (headerGetEntry(h, RPMTAG_PREFIXES, &pft,
				       (void **) &paths, &c) && (c == 1)) {
		    defaultReloc->oldPath = xstrdup(paths[0]);
		    paths = headerFreeData(paths, pft);
		} else {
		    const char * name;
		    (void) headerNVR(h, &name, NULL, NULL);
		    rpmMessage(RPMMESS_ERROR,
			       _("package %s is not relocateable\n"), name);
		    numFailed++;
		    goto exit;
		    /*@notreached@*/
		}
	    }

	    /* On --freshen, verify package is installed and newer */
	    if (interfaceFlags & INSTALL_FRESHEN) {
		rpmdbMatchIterator mi;
		const char * name;
		Header oldH;
		int count;

		(void) headerNVR(h, &name, NULL, NULL);
		mi = rpmdbInitIterator(db, RPMTAG_NAME, name, 0);
		count = rpmdbGetIteratorCount(mi);
		while ((oldH = rpmdbNextIterator(mi)) != NULL) {
		    if (rpmVersionCompare(oldH, h) < 0)
			continue;
		    /* same or newer package already installed */
		    count = 0;
		    break;
		}
		mi = rpmdbFreeIterator(mi);
		if (count == 0) {
		    h = headerFree(h);
		    continue;
		}
		/* Package is newer than those currently installed. */
	    }

	    rc = rpmtransAddPackage(ts, h, NULL, fileName,
			       (interfaceFlags & INSTALL_UPGRADE) != 0,
			       relocations);
	    h = headerFree(h);	/* XXX reference held by transaction set */
	    if (defaultReloc)
		defaultReloc->oldPath = _free(defaultReloc->oldPath);

	    switch(rc) {
	    case 0:
		rpmMessage(RPMMESS_DEBUG, "\tadded binary package [%d]\n",
			numRPMS);
		break;
	    case 1:
		rpmMessage(RPMMESS_ERROR,
			    _("error reading from file %s\n"), *fnp);
		numFailed++;
		goto exit;
		/*@notreached@*/ break;
	    case 2:
		rpmMessage(RPMMESS_ERROR,
			    _("file %s requires a newer version of RPM\n"),
			    *fnp);
		numFailed++;
		goto exit;
		/*@notreached@*/ break;
	    }

	    numRPMS++;
	    continue;
	}

	if (rpmrc != RPMRC_BADMAGIC) {
	    rpmMessage(RPMMESS_ERROR, _("%s cannot be installed\n"), *fnp);
	    numFailed++; *fnp = NULL;
	    break;
	}

	/* Try to read a package manifest. */
	fd = Fopen(*fnp, "r.fpio");
	if (fd == NULL || Ferror(fd)) {
	    rpmError(RPMERR_OPEN, _("open of %s failed: %s\n"), *fnp,
			Fstrerror(fd));
	    if (fd) (void) Fclose(fd);
	    numFailed++; *fnp = NULL;
	    break;
	}

	/* Read list of packages from manifest. */
	rc = rpmReadPackageManifest(fd, &argc, &argv);
	if (rc)
	    rpmError(RPMERR_MANIFEST, _("%s: read manifest failed: %s\n"),
			*fnp, Fstrerror(fd));
	(void) Fclose(fd);

	/* If successful, restart the query loop. */
	if (rc == 0) {
	    prevx++;
	    goto restart;
	}

	numFailed++; *fnp = NULL;
	break;
    }

    rpmMessage(RPMMESS_DEBUG, _("found %d source and %d binary packages\n"),
		numSRPMS, numRPMS);

    if (numFailed) goto exit;

    if (numRPMS && !(interfaceFlags & INSTALL_NODEPS)) {
	struct rpmDependencyConflict * conflicts;
	int numConflicts;

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

	if (probs != NULL) rpmProblemSetFree(probs);
    }

    if (numSRPMS && !stopInstall) {
	for (i = 0; i < numSRPMS; i++) {
	    fd = Fopen(sourceURL[i], "r.ufdio");
	    if (fd == NULL || Ferror(fd)) {
		rpmMessage(RPMMESS_ERROR, _("cannot open file %s: %s\n"),
			   sourceURL[i], Fstrerror(fd));
		if (fd) (void) Fclose(fd);
		continue;
	    }

	    if (!(transFlags & RPMTRANS_FLAG_TEST)) {
		rpmRC rpmrc = rpmInstallSourcePackage(rootdir, fd, NULL,
			showProgress, (void *) ((long)notifyFlags), NULL);
		if (rpmrc != RPMRC_OK) numFailed++;
	    }

	    (void) Fclose(fd);
	}
    }

exit:
    if (ts) rpmtransFree(ts);
    for (i = 0; i < numPkgs; i++) {
	if (pkgState[i] == 1)
	    (void) Unlink(pkgURL[i]);
	pkgURL[i] = _free(pkgURL[i]);
    }
    pkgState = _free(pkgState);
    pkgURL = _free(pkgURL);
    argv = _free(argv);
    if (dbIsOpen) (void) rpmdbClose(db);
    return numFailed;
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

    if (argv == NULL) return 0;

    if (transFlags & RPMTRANS_FLAG_TEST)
	mode = O_RDONLY;
    else
	mode = O_RDWR | O_EXCL;
	
    if (rpmdbOpen(rootdir, &db, mode, 0644)) {
	const char *dn;
	dn = rpmGetPath( (rootdir ? rootdir : ""), "%{_dbpath}", NULL);
	rpmMessage(RPMMESS_ERROR, _("cannot open %s/packages.rpm\n"), dn);
	dn = _free(dn);
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
	mi = rpmdbFreeIterator(mi);
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
    (void) rpmdbClose(db);

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
	if (fd) (void) Fclose(fd);
	return 1;
    }

    if (rpmIsVerbose())
	fprintf(stdout, _("Installing %s\n"), arg);

    {
	/*@-mayaliasunique@*/
	rpmRC rpmrc = rpmInstallSourcePackage(rootdir, fd, specFile, NULL, NULL,
				 cookie);
	/*@=mayaliasunique@*/
	rc = (rpmrc == RPMRC_OK ? 0 : 1);
    }
    if (rc != 0) {
	rpmMessage(RPMMESS_ERROR, _("%s cannot be installed\n"), arg);
	/*@-unqualifiedtrans@*/
	if (specFile && *specFile)
	    *specFile = _free(*specFile);
	if (cookie && *cookie)
	    *cookie = _free(*cookie);
	/*@=unqualifiedtrans@*/
    }

    (void) Fclose(fd);

    return rc;
}
