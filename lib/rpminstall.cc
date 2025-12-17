/** \ingroup rpmcli
 * \file lib/rpminstall.c
 */

#include "system.h"

#include <deque>
#include <string>
#include <vector>

#include <string.h>

#include <rpm/rpmcli.h>
#include <rpm/rpmtag.h>
#include <rpm/rpmlib.h>		/* rpmReadPackageFile, vercmp etc */
#include <rpm/rpmdb.h>
#include <rpm/rpmds.h>
#include <rpm/rpmts.h>
#include <rpm/rpmsq.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>

#include "rpmgi.hh"
#include "manifest.hh"
#include "debug.h"

static int rpmcliPackagesTotal = 0;
static int rpmcliHashesCurrent = 0;
static int rpmcliHashesTotal = 0;
static int rpmcliProgressCurrent = 0;
static int rpmcliProgressTotal = 0;
static int rpmcliProgressState = 0;

/**
 * Print a CLI progress bar.
 * @todo Unsnarl isatty(STDOUT_FILENO) from the control flow.
 * @param amount	current
 * @param total		final
 */
static void printHash(const rpm_loff_t amount, const rpm_loff_t total)
{
    int hashesNeeded;

    rpmcliHashesTotal = (isatty (STDOUT_FILENO) ? 34 : 40);

    if (rpmcliHashesCurrent != rpmcliHashesTotal) {
	float pct = (total ? (((float) amount) / total) : 1.0);
	hashesNeeded = (rpmcliHashesTotal * pct) + 0.5;
	while (hashesNeeded > rpmcliHashesCurrent) {
	    if (isatty (STDOUT_FILENO)) {
		int i;
		for (i = 0; i < rpmcliHashesCurrent; i++)
		    (void) putchar ('#');
		for (; i < rpmcliHashesTotal; i++)
		    (void) putchar (' ');
		fprintf(stdout, "(%3d%%)", (int)((100 * pct) + 0.5));
		for (i = 0; i < (rpmcliHashesTotal + 6); i++)
		    (void) putchar ('\b');
	    } else
		fprintf(stdout, "#");

	    rpmcliHashesCurrent++;
	}
	(void) fflush(stdout);

	if (rpmcliHashesCurrent == rpmcliHashesTotal) {
	    int i;
	    rpmcliProgressCurrent++;
	    if (isatty(STDOUT_FILENO)) {
	        for (i = 1; i < rpmcliHashesCurrent; i++)
		    (void) putchar ('#');
		pct = (rpmcliProgressTotal
		    ? (((float) rpmcliProgressCurrent) / rpmcliProgressTotal)
		    : 1);
		fprintf(stdout, " [%3d%%]", (int)((100 * pct) + 0.5));
	    }
	    fprintf(stdout, "\n");
	}
	(void) fflush(stdout);
    }
}

static rpmVSFlags setvsFlags(struct rpmInstallArguments_s * ia)
{
    rpmVSFlags vsflags;

    if (ia->installInterfaceFlags & (INSTALL_UPGRADE | INSTALL_ERASE))
	vsflags = rpmExpandNumeric("%{?_vsflags_erase}");
    else
	vsflags = rpmExpandNumeric("%{?_vsflags_install}");

    vsflags |= rpmcliVSFlags;

    return vsflags;
}

void * rpmShowProgress(const void * arg,
			const rpmCallbackType what,
			const rpm_loff_t amount,
			const rpm_loff_t total,
			fnpyKey key,
			void * data)
{
    Header h = (Header) arg;
    int flags = (int) ((long)data);
    void * rc = NULL;
    const char * filename = (const char *)key;
    static FD_t fd = NULL;

    switch (what) {
    case RPMCALLBACK_INST_OPEN_FILE:
	if (filename == NULL || filename[0] == '\0')
	    return NULL;
	fd = Fopen(filename, "r.ufdio");
	/* FIX: still necessary? */
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("open of %s failed: %s\n"), filename,
			Fstrerror(fd));
	    if (fd != NULL) {
		Fclose(fd);
		fd = NULL;
	    }
	} else
	    fd = fdLink(fd);
	return (void *)fd;
	break;

    case RPMCALLBACK_INST_CLOSE_FILE:
	/* FIX: still necessary? */
	fd = fdFree(fd);
	if (fd != NULL) {
	    Fclose(fd);
	    fd = NULL;
	}
	break;

    case RPMCALLBACK_INST_START:
    case RPMCALLBACK_UNINST_START:
	if (rpmcliProgressState != what) {
	    rpmcliProgressState = what;
	    if (flags & INSTALL_HASH) {
		if (what == RPMCALLBACK_INST_START) {
		    fprintf(stdout, _("Updating / installing...\n"));
		} else {
		    fprintf(stdout, _("Cleaning up / removing...\n"));
		}
		fflush(stdout);
	    }
	}
		
	rpmcliHashesCurrent = 0;
	if (h == NULL || !(flags & INSTALL_LABEL))
	    break;
	if (flags & INSTALL_HASH) {
	    char *s = headerGetAsString(h, RPMTAG_NEVR);
	    if (isatty (STDOUT_FILENO))
		fprintf(stdout, "%4d:%-33.33s", rpmcliProgressCurrent + 1, s);
	    else
		fprintf(stdout, "%-38.38s", s);
	    (void) fflush(stdout);
	    free(s);
	} else {
	    char *s = headerGetAsString(h, RPMTAG_NEVRA);
	    fprintf(stdout, "%s\n", s);
	    (void) fflush(stdout);
	    free(s);
	}
	break;

    case RPMCALLBACK_INST_STOP:
	break;

    case RPMCALLBACK_TRANS_PROGRESS:
    case RPMCALLBACK_INST_PROGRESS:
    case RPMCALLBACK_UNINST_PROGRESS:
    case RPMCALLBACK_VERIFY_PROGRESS:
	if (flags & INSTALL_PERCENT)
	    fprintf(stdout, "%%%% %f\n", (double) (total
				? ((((float) amount) / total) * 100)
				: 100.0));
	else if (flags & INSTALL_HASH)
	    printHash(amount, total);
	(void) fflush(stdout);
	break;

    case RPMCALLBACK_TRANS_START:
    case RPMCALLBACK_VERIFY_START:
	rpmcliHashesCurrent = 0;
	rpmcliProgressTotal = 1;
	rpmcliProgressCurrent = 0;
	rpmcliPackagesTotal = total;
	rpmcliProgressState = what;
	if (!(flags & INSTALL_LABEL))
	    break;
	if (flags & INSTALL_HASH) {
	    fprintf(stdout, "%-38s", (what == RPMCALLBACK_TRANS_START) ?
		    _("Preparing...") : _("Verifying..."));
	} else {
	    fprintf(stdout, "%s\n", (what == RPMCALLBACK_TRANS_START) ?
		    _("Preparing packages...") : _("Verifying packages..."));
	}
	(void) fflush(stdout);
	break;

    case RPMCALLBACK_TRANS_STOP:
    case RPMCALLBACK_VERIFY_STOP:
	if (flags & INSTALL_HASH)
	    printHash(1, 1);	/* Fixes "preparing..." progress bar */
	rpmcliProgressTotal = rpmcliPackagesTotal;
	rpmcliProgressCurrent = 0;
	break;

    case RPMCALLBACK_UNINST_STOP:
	break;
    case RPMCALLBACK_UNPACK_ERROR:
	break;
    case RPMCALLBACK_CPIO_ERROR:
	break;
    case RPMCALLBACK_SCRIPT_ERROR:
	break;
    case RPMCALLBACK_SCRIPT_START:
	break;
    case RPMCALLBACK_SCRIPT_STOP:
	break;
    case RPMCALLBACK_UNKNOWN:
    default:
	break;
    }

    return rc;
}	

static void setNotifyFlag(struct rpmInstallArguments_s * ia,
			  rpmts ts)
{
    int notifyFlags;

    notifyFlags = ia->installInterfaceFlags | (rpmIsVerbose() ? INSTALL_LABEL : 0 );
    rpmtsSetNotifyCallback(ts, rpmShowProgress, (void *) ((long)notifyFlags));
}

static int rpmcliTransaction(rpmts ts, struct rpmInstallArguments_s * ia,
		      int numPackages)
{
    rpmps ps;

    int rc = 0;
    int stop = 0;

    int eflags = ia->installInterfaceFlags & INSTALL_ERASE;

    if (!(ia->installInterfaceFlags & INSTALL_NODEPS)) {

	if (rpmtsCheck(ts)) {
	    rc = numPackages;
	    stop = 1;
	}

	ps = rpmtsProblems(ts);
	if (!stop && rpmpsNumProblems(ps) > 0) {
	    rpmlog(RPMLOG_ERR, _("Failed dependencies:\n"));
	    rpmpsPrint(NULL, ps);
	    rc = numPackages;
	    stop = 1;
	}
	ps = rpmpsFree(ps);
    }

    if (!stop && !(ia->installInterfaceFlags & INSTALL_NOORDER)) {
	if (rpmtsOrder(ts)) {
	    rc = numPackages;
	    stop = 1;
	}
    }

    if (numPackages && !stop) {
	const char *msg;
	if (ia->installInterfaceFlags & INSTALL_RESTORE)
	    msg = "restoring packages";
	else if (ia->installInterfaceFlags & INSTALL_ERASE)
	    msg = "erasing packages";
	else
	    msg = "installing binary packages";

	rpmlog(RPMLOG_DEBUG, "%s\n", msg);
	rpmtsClean(ts);
	rc = rpmtsRun(ts, NULL, ia->probFilter);

	ps = rpmtsProblems(ts);

	if (rpmpsNumProblems(ps) > 0 && (eflags || rc > 0))
	    rpmpsPrint(NULL, ps);
	ps = rpmpsFree(ps);
    }

    return rc;
}

static int tryReadManifest(const char *fn, std::deque<std::string> & queue)
{
    int rc = RPMRC_FAIL;

    /* Try to read a package manifest. */
    FD_t fd = Fopen(fn, "r.ufdio");
    if (fd == NULL || Ferror(fd)) {
        rpmlog(RPMLOG_ERR, _("open of %s failed: %s\n"), fn, Fstrerror(fd));
    } else {
	ARGV_t argv = NULL;
	int argc = 0;
	/* Read list of packages from manifest. */
	rc = rpmReadPackageManifest(fd, &argc, &argv);
	if (rc == RPMRC_OK)
	    queue.insert(queue.end(), argv, argv+argc);
	argvFree(argv);
    }
    Fclose(fd);
    return rc;
}

static int tryReadHeader(rpmts ts, const char *fn, Header * hdrp)
{
    /* Try to read the header from a package file. */
    rpmRC rc = RPMRC_FAIL;
    FD_t fd = Fopen(fn, "r.ufdio");
    if (fd == NULL || Ferror(fd)) {
	rpmlog(RPMLOG_ERR, _("open of %s failed: %s\n"), fn, Fstrerror(fd));
    } else {
	/* Read the header, verifying signatures (if present). */
	rc = rpmReadPackageFile(ts, fd, fn, hdrp);
	/* Honor --nomanifest */
	if (rc == RPMRC_NOTFOUND && (giFlags & RPMGI_NOMANIFEST))
	    rc = RPMRC_FAIL;

	if (rc == RPMRC_FAIL)
	    rpmlog(RPMLOG_ERR, _("%s cannot be installed\n"), fn);
   }
   Fclose(fd);
   
   return rc;
}


/* On --freshen, verify package is installed and newer */
static int checkFreshenStatus(rpmts ts, Header h, int allowOld)
{
    rpmdbMatchIterator mi = NULL;
    const char * name = headerGetString(h, RPMTAG_NAME);
    const char *arch = headerGetString(h, RPMTAG_ARCH);
    Header oldH = NULL;
    if (name != NULL)
        mi = rpmtsInitIterator(ts, RPMDBI_NAME, name, 0);
    if (rpmtsColor(ts) && arch)
	rpmdbSetIteratorRE(mi, RPMTAG_ARCH, RPMMIRE_DEFAULT, arch);

    while ((oldH = rpmdbNextIterator(mi)) != NULL) {
	/* Package is older than those currently installed. */
	if (allowOld && rpmVersionCompare(oldH, h) > 0)
	    break;
	/* Package is newer than those currently installed. */
        if (rpmVersionCompare(oldH, h) < 0)
	    break;
    }

    rpmdbFreeIterator(mi);
    return (oldH != NULL);
}

static int rpmNoGlob(const char *fn, int *argcPtr, ARGV_t * argvPtr)
{
    struct stat sb;
    int rc = stat(fn, &sb);
    if (rc == 0) {
	argvAdd(argvPtr, fn);
	*argcPtr = 1;
    } else {
	*argcPtr = 0;
    }
    return rc;
}

static int globArgs(ARGV_t fileArgv, std::deque<std::string> & queue)
{
    int numFailed = 0;
    for (char **fnp = fileArgv; fnp && *fnp != NULL; fnp++) {
	ARGV_t av = NULL;
	int ac = 0;
	int rc = 0;

	if (giFlags & RPMGI_NOGLOB) {
	    rc = rpmNoGlob(*fnp, &ac, &av);
	} else {
	    rc = rpmGlobPath(*fnp, RPMGLOB_NOCHECK, &ac, &av);
	}
	if (rc || ac == 0) {
	    if (giFlags & RPMGI_NOGLOB) {
		rpmlog(RPMLOG_ERR, _("File not found: %s\n"), *fnp);
	    } else {
		rpmlog(RPMLOG_ERR, _("File not found by glob: %s\n"), *fnp);
	    }
	    numFailed++;
	} else {
	    queue.insert(queue.end(), av, av+ac);
	}
	argvFree(av);
    }
    return numFailed;
}

/** @todo Generalize --freshen policies. */
int rpmInstall(rpmts ts, struct rpmInstallArguments_s * ia, ARGV_t fileArgv)
{
    rpmRelocation * relocations = ia->relocations;
    rpmVSFlags vsflags, ovsflags;
    rpmVSFlags ovfyflags;
    int numFailed = 0;
    std::deque<std::string> queue;
    std::vector<std::string> cleanup, RPMS, SRPMS;

    vsflags = setvsFlags(ia);
    ovsflags = rpmtsSetVSFlags(ts, (vsflags | RPMVSF_NEEDPAYLOAD));
    /* for rpm cli, --nosignature/--nodigest applies to both vs and vfyflags */
    ovfyflags = rpmtsSetVfyFlags(ts, (rpmtsVfyFlags(ts) | rpmcliVSFlags));

    if (fileArgv == NULL) goto exit;

    if (rpmcliVfyLevelMask) {
	int vfylevel = rpmtsVfyLevel(ts);
	vfylevel &= ~rpmcliVfyLevelMask;
	rpmtsSetVfyLevel(ts, vfylevel);
    }

    (void) rpmtsSetFlags(ts, ia->transFlags);

    setNotifyFlag(ia, ts); 

    if (relocations != NULL) {
	while (relocations->oldPath)
	    relocations++;
	if (relocations->newPath == NULL)
	    relocations = NULL;
    }

    /* Build fully globbed list of arguments in argv[argc]. */
    numFailed = globArgs(fileArgv, queue);

    while (queue.empty() == false) {
	std::string arg = queue.front();
	const char *fileURL = arg.c_str();
	std::string fn;
	queue.pop_front();

	switch (urlIsURL(fileURL)) {
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_FTP:
	{   char *tfn = NULL;
	    FD_t tfd;
	    int rc = -1;

	    if (rpmIsVerbose())
		fprintf(stdout, _("Retrieving %s\n"), fileURL);

	    tfd = rpmMkTempFile(rpmtsRootDir(ts), &tfn);
	    if (tfn) {
		Fclose(tfd);
		rc = urlGetFile(fileURL, tfn);
		fn = tfn;
		cleanup.push_back(fn);
		free(tfn);
	    }

	    if (rc != 0) {
		rpmlog(RPMLOG_ERR,
			_("skipping %s - transfer failed\n"), fileURL);
		numFailed++;
		continue;
	    }
	}   break;
	case URL_IS_PATH:
	case URL_IS_DASH:	/* WRONG WRONG WRONG */
	case URL_IS_HKP:	/* WRONG WRONG WRONG */
	default: {
	    const char *tfn = NULL;
	    (void) urlPath(fileURL, &tfn);
	    fn = tfn;
	    }
	    break;
	}

	Header h = NULL;
	const char *cfn = fn.c_str();
	rpmlog(RPMLOG_DEBUG, "============== %s\n", cfn);

	int rc = tryReadHeader(ts, cfn, &h);
	if (rc == RPMRC_NOTFOUND) {
	    /* Add manifest contents to the queue */
	    if (tryReadManifest(cfn, queue) == RPMRC_OK)
		continue;
	}
	if (h == NULL) {
	    numFailed++;
	    continue;
	}

	if (headerIsSource(h)) {
	    headerFree(h);
	    if (ia->installInterfaceFlags & INSTALL_FRESHEN) {
	        continue;
	    }
	    rpmlog(RPMLOG_DEBUG, "added source package %s\n", cfn);
	    SRPMS.push_back(fn);
	    continue;
	}

	if (relocations) {
	    struct rpmtd_s prefixes;

	    headerGet(h, RPMTAG_PREFIXES, &prefixes, HEADERGET_DEFAULT);
	    if (rpmtdCount(&prefixes) == 1) {
		relocations->oldPath = xstrdup(rpmtdGetString(&prefixes));
		rpmtdFreeData(&prefixes);
	    } else {
		rpmlog(RPMLOG_ERR, _("package %s is not relocatable\n"),
		       headerGetString(h, RPMTAG_NAME));
		numFailed++;
		headerFree(h);
		goto exit;
	    }
	}

	if (ia->installInterfaceFlags & INSTALL_FRESHEN)
	    if (checkFreshenStatus(ts, h, ia->probFilter & RPMPROB_FILTER_OLDPACKAGE) != 1) {
		headerFree(h);
	        continue;
	    }

	/* fnpyKey must persist for the duration of the transaction */
	RPMS.push_back(fn);
	const char *pkgfn = RPMS.back().c_str();
	if (ia->installInterfaceFlags & INSTALL_REINSTALL)
	    rc = rpmtsAddReinstallElement(ts, h, (fnpyKey)pkgfn);
	else
	    rc = rpmtsAddInstallElement(ts, h, (fnpyKey)pkgfn,
			(ia->installInterfaceFlags & INSTALL_UPGRADE) != 0,
			ia->relocations);

	headerFree(h);
	if (relocations)
	    relocations->oldPath = _free(relocations->oldPath);

	switch (rc) {
	case 0:
	    rpmlog(RPMLOG_DEBUG, "added binary package %s\n", cfn);
	    break;
	case 1:
	    rpmlog(RPMLOG_ERR, _("error reading from file %s\n"), cfn);
	    break;
	case 3:
	    rpmlog(RPMLOG_ERR, _("package format not supported: %s\n"), cfn);
	    break;
	default:
	    break;
	}
	if (rc) {
	    numFailed++;
	    goto exit;
	}
    }

    rpmlog(RPMLOG_DEBUG, "found %zu source and %zu binary packages\n",
		SRPMS.size(), RPMS.size());

    if (numFailed) goto exit;

    if (RPMS.empty() == false) {
        int rc = rpmcliTransaction(ts, ia, RPMS.size());
        if (rc < 0)
            numFailed += RPMS.size();
	else if (rc > 0)
            numFailed += rc;
    }

    if (SRPMS.empty() == false) {
	rpmcliProgressState = 0;
	rpmcliProgressTotal = 0;
	rpmcliProgressCurrent = 0;
	rpmtsEmpty(ts);
	for (const auto & p : SRPMS) {
	    int rc = RPMRC_OK;
	    if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_TEST))
		rc = rpmInstallSource(ts, p.c_str(), NULL, NULL);
	    if (rc)
		numFailed++;
	}
    }

exit:
    for (const auto & fn : cleanup)
	(void) unlink(fn.c_str());

    rpmtsEmpty(ts);
    rpmtsSetVSFlags(ts, ovsflags);
    rpmtsSetVfyFlags(ts, ovfyflags);

    return numFailed;
}

int rpmErase(rpmts ts, struct rpmInstallArguments_s * ia, ARGV_const_t argv)
{
    char * const * arg;
    char *qfmt = NULL;
    int numFailed = 0;
    int numPackages = 0;
    rpmVSFlags vsflags, ovsflags;

    if (argv == NULL) return 0;

    vsflags = setvsFlags(ia);
    ovsflags = rpmtsSetVSFlags(ts, vsflags);

    (void) rpmtsSetFlags(ts, ia->transFlags);

    setNotifyFlag(ia, ts);

    qfmt = rpmExpand("%{?_query_all_fmt}\n", NULL);
    for (arg = argv; *arg; arg++) {
	rpmdbMatchIterator mi = rpmtsInitIterator(ts, RPMDBI_LABEL, *arg, 0);
	int matches = rpmdbGetIteratorCount(mi);
	int erasing = 1;

	if (! matches) {
	    rpmlog(RPMLOG_ERR, _("package %s is not installed\n"), *arg);
	    numFailed++;
	} else {
	    Header h;	/* XXX iterator owns the reference */

	    if (matches > 1 && 
		!(ia->installInterfaceFlags & UNINSTALL_ALLMATCHES)) {
		rpmlog(RPMLOG_ERR, _("\"%s\" specifies multiple packages:\n"),
			*arg);
		numFailed++;
		erasing = 0;
	    }

	    while ((h = rpmdbNextIterator(mi)) != NULL) {
		if (erasing) {
		    (void) rpmtsAddEraseElement(ts, h, -1);
		    numPackages++;
		} else {
		    char *nevra = headerFormat(h, qfmt, NULL);
		    rpmlog(RPMLOG_NOTICE, "  %s", nevra);
		    free(nevra);
		}
	    }
	}
	rpmdbFreeIterator(mi);
    }
    free(qfmt);

    if (numFailed) goto exit;
    numFailed = rpmcliTransaction(ts, ia, numPackages);
exit:
    rpmtsEmpty(ts);
    rpmtsSetVSFlags(ts, ovsflags);

    return (numFailed < 0) ? numPackages : numFailed;
}

static int handleRestorePackage(QVA_t qva, rpmts ts, Header h)
{
    /* filter out gpg-pubkey headers, there's nothing to restore */
    if (rstreq(headerGetString(h, RPMTAG_NAME), "gpg-pubkey"))
	return 0;

    int rc = rpmtsAddRestoreElement(ts, h);
    if (rc) {
	/* shouldn't happen, corrupted package in the rpmdb? */
	char *nevra = headerGetAsString(h, RPMTAG_NEVRA);
	rpmlog(RPMLOG_ERR,
		_("failed to add restore element to transaction: %s\n"), nevra);
	free(nevra);
    }
    return rc;
}

int rpmRestore(rpmts ts, struct rpmInstallArguments_s * ia, ARGV_const_t argv)
{
    QVA_t qva = &rpmQVKArgs;
    int rc;
    rpmVSFlags vsflags = setvsFlags(ia);
    rpmVSFlags ovsflags = rpmtsSetVSFlags(ts, vsflags);

    rpmtsSetFlags(ts, ia->transFlags);
    setNotifyFlag(ia, ts);

    if (qva->qva_showPackage == NULL)
	qva->qva_showPackage = handleRestorePackage;

    rc = rpmcliArgIter(ts, qva, argv);
    if (rc == 0) {
	rc = rpmcliTransaction(ts, ia, rpmtsNElements(ts));
    }

    rpmtsEmpty(ts);
    rpmtsSetVSFlags(ts, ovsflags);

    return rc;
}

int rpmInstallSource(rpmts ts, const char * arg,
		char ** specFilePtr, char ** cookie)
{
    FD_t fd;
    int rc;


    fd = Fopen(arg, "r.ufdio");
    if (fd == NULL || Ferror(fd)) {
	rpmlog(RPMLOG_ERR, _("cannot open %s: %s\n"), arg, Fstrerror(fd));
	if (fd != NULL) (void) Fclose(fd);
	return 1;
    }

    if (rpmIsVerbose() && specFilePtr != NULL)
	fprintf(stdout, _("Installing %s\n"), arg);

    {
	rpmVSFlags ovsflags =
		rpmtsSetVSFlags(ts, (specFilePtr) ? (rpmtsVSFlags(ts) | RPMVSF_NEEDPAYLOAD) : rpmtsVSFlags(ts));
	rpmRC rpmrc = rpmInstallSourcePackage(ts, fd, specFilePtr, cookie);
	rc = (rpmrc == RPMRC_OK ? 0 : 1);
	rpmtsSetVSFlags(ts, ovsflags);
    }
    if (rc != 0) {
	rpmlog(RPMLOG_ERR, _("%s cannot be installed\n"), arg);
	if (specFilePtr && *specFilePtr)
	    *specFilePtr = _free(*specFilePtr);
	if (cookie && *cookie)
	    *cookie = _free(*cookie);
    }

    (void) Fclose(fd);

    return rc;
}

