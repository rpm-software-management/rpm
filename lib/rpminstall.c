/** \ingroup rpmcli
 * \file lib/rpminstall.c
 */

#include "system.h"

#include <rpm/rpmcli.h>
#include <rpm/rpmtag.h>
#include <rpm/rpmlib.h>		/* rpmReadPackageFile, vercmp etc */
#include <rpm/rpmdb.h>
#include <rpm/rpmds.h>
#include <rpm/rpmts.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmfileutil.h>

#include "lib/rpmgi.h"
#include "lib/manifest.h"
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

    if (rpmcliQueryFlags & VERIFY_DIGEST)
	vsflags |= _RPMVSF_NODIGESTS;
    if (rpmcliQueryFlags & VERIFY_SIGNATURE)
	vsflags |= _RPMVSF_NOSIGNATURES;
    if (rpmcliQueryFlags & VERIFY_HDRCHK)
	vsflags |= RPMVSF_NOHDRCHK;

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
	if (flags & INSTALL_PERCENT)
	    fprintf(stdout, "%%%% %f\n", (double) (total
				? ((((float) amount) / total) * 100)
				: 100.0));
	else if (flags & INSTALL_HASH)
	    printHash(amount, total);
	(void) fflush(stdout);
	break;

    case RPMCALLBACK_TRANS_START:
	rpmcliHashesCurrent = 0;
	rpmcliProgressTotal = 1;
	rpmcliProgressCurrent = 0;
	rpmcliPackagesTotal = total;
	rpmcliProgressState = what;
	if (!(flags & INSTALL_LABEL))
	    break;
	if (flags & INSTALL_HASH)
	    fprintf(stdout, "%-38s", _("Preparing..."));
	else
	    fprintf(stdout, "%s\n", _("Preparing packages..."));
	(void) fflush(stdout);
	break;

    case RPMCALLBACK_TRANS_STOP:
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

struct rpmEIU {
    int numFailed;
    int numPkgs;
    char ** pkgURL;
    char ** fnp;
    char * pkgState;
    int prevx;
    int pkgx;
    int numRPMS;
    int numSRPMS;
    char ** sourceURL;
    int argc;
    char ** argv;
    rpmRelocation * relocations;
    rpmRC rpmrc;
};

static int rpmcliTransaction(rpmts ts, struct rpmInstallArguments_s * ia,
		      int numPackages, int appStore, void *probPtr)
{
    rpmps ps;

    int rc = 0;
    int stop = 0;

    int eflags = ia->installInterfaceFlags & INSTALL_ERASE;

    if (!(ia->installInterfaceFlags & INSTALL_NODEPS)) {

    if (appStore) {
        if (rpmtsCheckAppStore(ts, NULL)) {
            rc = numPackages;
            stop = 1;
        }
        ps = rpmtsProblems(ts);
    } else {
        if (rpmtsCheck(ts)) {
            rc = numPackages;
            stop = 1;
        }
        ps = rpmtsProblems(ts);
    }

	if (!stop && rpmpsNumProblems(ps) > 0) {
	    rpmlog(RPMLOG_ERR, _("Failed dependencies:\n"));
	    rpmpsPrint(NULL, ps);
        rpmpsToChunk(probPtr, ps);
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
	rpmlog(RPMLOG_DEBUG, eflags ? "erasing packages\n" :
				      "installing binary packages\n");
	rpmtsClean(ts);
    if (appStore)
        rc = rpmtsRunAppStore(ts, NULL, ia->probFilter);
    else
	    rc = rpmtsRun(ts, NULL, ia->probFilter);

	ps = rpmtsProblems(ts);

	if (rpmpsNumProblems(ps) > 0 && (eflags || rc > 0)) {
	    rpmpsPrint(NULL, ps);
        rpmpsToChunk(probPtr, ps);
    }
	ps = rpmpsFree(ps);
    }

    return rc;
}

static int tryReadManifest(struct rpmEIU * eiu, void *probPtr)
{
    int rc;

    /* Try to read a package manifest. */
    FD_t fd = Fopen(*eiu->fnp, "r.ufdio");
    if (fd == NULL || Ferror(fd)) {
        rpmlog(RPMLOG_ERR, _("open of %s failed: %s\n"), *eiu->fnp,
	       Fstrerror(fd));
	if (fd != NULL) {
	    Fclose(fd);
	    fd = NULL;
	}
	eiu->numFailed++;
	*eiu->fnp = NULL;
	return RPMRC_FAIL;
    }

    /* Read list of packages from manifest. */
    rc = rpmReadPackageManifest(fd, &eiu->argc, &eiu->argv);
    if (rc != RPMRC_OK) {
        rpmlog(RPMLOG_ERR, _("%s: not an rpm package (or package manifest): %s\n"),
	       *eiu->fnp, Fstrerror(fd));
        if (probPtr) {
            char *error = "not an rpm package (or package manifest)";
            size_t realsize = strlen(error);
            chunk_t *mem = (chunk_t *)probPtr;
            mem->memory = realloc(mem->memory, mem->size + realsize + 1);
            if (mem->memory) {
                memcpy(&(mem->memory[mem->size]), error, realsize);
                mem->size += realsize;
                mem->memory[mem->size] = 0;
            }
        }
    }
    Fclose(fd);
    fd = NULL;

    if (rc != RPMRC_OK) {
        eiu->numFailed++;
        *eiu->fnp = NULL;
    }

    return rc;
}

static int tryReadHeader(rpmts ts, struct rpmEIU * eiu, Header * hdrp)
{
   /* Try to read the header from a package file. */
   FD_t fd = Fopen(*eiu->fnp, "r.ufdio");
   if (fd == NULL || Ferror(fd)) {
       rpmlog(RPMLOG_ERR, _("open of %s failed: %s\n"), *eiu->fnp,
	      Fstrerror(fd));
       if (fd != NULL) {
           Fclose(fd);
	   fd = NULL;
       }
       eiu->numFailed++;
       *eiu->fnp = NULL;
       return RPMRC_FAIL;
   }

   /* Read the header, verifying signatures (if present). */
   eiu->rpmrc = rpmReadPackageFile(ts, fd, *eiu->fnp, hdrp);
   Fclose(fd);
   fd = NULL;
   
   /* Honor --nomanifest */
   if (eiu->rpmrc == RPMRC_NOTFOUND && (giFlags & RPMGI_NOMANIFEST))
       eiu->rpmrc = RPMRC_FAIL;

   if(eiu->rpmrc == RPMRC_FAIL) {
       rpmlog(RPMLOG_ERR, _("%s cannot be installed\n"), *eiu->fnp);
       eiu->numFailed++;
       *eiu->fnp = NULL;
   }

   return RPMRC_OK;
}


/* On --freshen, verify package is installed and newer */
static int checkFreshenStatus(rpmts ts, Header h)
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

/** @todo Generalize --freshen policies. */
int rpmInstall(rpmts ts, struct rpmInstallArguments_s * ia, ARGV_t fileArgv)
{
    struct rpmEIU * eiu = xcalloc(1, sizeof(*eiu));
    rpmRelocation * relocations;
    char * fileURL = NULL;
    rpmVSFlags vsflags, ovsflags;
    int rc;
    int i;

    vsflags = setvsFlags(ia);
    ovsflags = rpmtsSetVSFlags(ts, (vsflags | RPMVSF_NEEDPAYLOAD));

    if (fileArgv == NULL) goto exit;

    (void) rpmtsSetFlags(ts, ia->transFlags);

    relocations = ia->relocations;

    setNotifyFlag(ia, ts); 

    if ((eiu->relocations = relocations) != NULL) {
	while (eiu->relocations->oldPath)
	    eiu->relocations++;
	if (eiu->relocations->newPath == NULL)
	    eiu->relocations = NULL;
    }

    /* Build fully globbed list of arguments in argv[argc]. */
    for (eiu->fnp = fileArgv; *eiu->fnp != NULL; eiu->fnp++) {
    	ARGV_t av = NULL;
    	int ac = 0;

	if (giFlags & RPMGI_NOGLOB) {
	    rc = rpmNoGlob(*eiu->fnp, &ac, &av);
	} else {
	    char * fn = rpmEscapeSpaces(*eiu->fnp);
	    rc = rpmGlob(fn, &ac, &av);
	    fn = _free(fn);
	}
	if (rc || ac == 0) {
	    if (giFlags & RPMGI_NOGLOB) {
		rpmlog(RPMLOG_ERR, _("File not found: %s\n"), *eiu->fnp);
	    } else {
		rpmlog(RPMLOG_ERR, _("File not found by glob: %s\n"), *eiu->fnp);
	    }
	    eiu->numFailed++;
	    continue;
	}

	argvAppend(&(eiu->argv), av);
	argvFree(av);
	eiu->argc += ac;
    }

restart:
    /* Allocate sufficient storage for next set of args. */
    if (eiu->pkgx >= eiu->numPkgs) {
	eiu->numPkgs = eiu->pkgx + eiu->argc;
	eiu->pkgURL = xrealloc(eiu->pkgURL,
			(eiu->numPkgs + 1) * sizeof(*eiu->pkgURL));
	memset(eiu->pkgURL + eiu->pkgx, 0,
			((eiu->argc + 1) * sizeof(*eiu->pkgURL)));
	eiu->pkgState = xrealloc(eiu->pkgState,
			(eiu->numPkgs + 1) * sizeof(*eiu->pkgState));
	memset(eiu->pkgState + eiu->pkgx, 0,
			((eiu->argc + 1) * sizeof(*eiu->pkgState)));
    }

    /* Retrieve next set of args, cache on local storage. */
    for (i = 0; i < eiu->argc; i++) {
	fileURL = _free(fileURL);
	fileURL = eiu->argv[i];
	eiu->argv[i] = NULL;

	switch (urlIsURL(fileURL)) {
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_FTP:
	{   char *tfn = NULL;
	    FD_t tfd;

	    if (rpmIsVerbose())
		fprintf(stdout, _("Retrieving %s\n"), fileURL);

	    tfd = rpmMkTempFile(rpmtsRootDir(ts), &tfn);
	    if (tfd && tfn) {
		Fclose(tfd);
	    	rc = urlGetFile(fileURL, tfn);
	    } else {
		rc = -1;
	    }

	    if (rc != 0) {
		rpmlog(RPMLOG_ERR,
			_("skipping %s - transfer failed\n"), fileURL);
		eiu->numFailed++;
		eiu->pkgURL[eiu->pkgx] = NULL;
		tfn = _free(tfn);
		break;
	    }
	    eiu->pkgState[eiu->pkgx] = 1;
	    eiu->pkgURL[eiu->pkgx] = tfn;
	    eiu->pkgx++;
	}   break;
	case URL_IS_PATH:
	case URL_IS_DASH:	/* WRONG WRONG WRONG */
	case URL_IS_HKP:	/* WRONG WRONG WRONG */
	default:
	    eiu->pkgURL[eiu->pkgx] = fileURL;
	    fileURL = NULL;
	    eiu->pkgx++;
	    break;
	}
    }
    fileURL = _free(fileURL);

    if (eiu->numFailed) goto exit;

    /* Continue processing file arguments, building transaction set. */
    for (eiu->fnp = eiu->pkgURL+eiu->prevx;
	 *eiu->fnp != NULL;
	 eiu->fnp++, eiu->prevx++)
    {
	Header h = NULL;
	const char * fileName;

	rpmlog(RPMLOG_DEBUG, "============== %s\n", *eiu->fnp);
	(void) urlPath(*eiu->fnp, &fileName);

	if (tryReadHeader(ts, eiu, &h) == RPMRC_FAIL)
	    continue;

	if (eiu->rpmrc == RPMRC_NOTFOUND) {
	    rc = tryReadManifest(eiu, NULL);
	    if (rc == RPMRC_OK) {
	        eiu->prevx++;
	        goto restart;
	    }
	}

	if (headerIsSource(h)) {
	    rpmlog(RPMLOG_DEBUG, "\tadded source package [%d]\n",
		eiu->numSRPMS);
	    eiu->sourceURL = xrealloc(eiu->sourceURL,
				(eiu->numSRPMS + 2) * sizeof(*eiu->sourceURL));
	    eiu->sourceURL[eiu->numSRPMS] = *eiu->fnp;
	    *eiu->fnp = NULL;
	    eiu->numSRPMS++;
	    eiu->sourceURL[eiu->numSRPMS] = NULL;
	    continue;
	}

	if (eiu->relocations) {
	    struct rpmtd_s prefixes;

	    headerGet(h, RPMTAG_PREFIXES, &prefixes, HEADERGET_DEFAULT);
	    if (rpmtdCount(&prefixes) == 1) {
		eiu->relocations->oldPath = xstrdup(rpmtdGetString(&prefixes));
		rpmtdFreeData(&prefixes);
	    } else {
		rpmlog(RPMLOG_ERR, _("package %s is not relocatable\n"),
		       headerGetString(h, RPMTAG_NAME));
		eiu->numFailed++;
		goto exit;
	    }
	}

	if (ia->installInterfaceFlags & INSTALL_FRESHEN)
	    if (checkFreshenStatus(ts, h) != 1) {
		headerFree(h);
	        continue;
	    }

	if (ia->installInterfaceFlags & INSTALL_REINSTALL)
	    rc = rpmtsAddReinstallElement(ts, h, (fnpyKey)fileName);
	else
	    rc = rpmtsAddInstallElement(ts, h, (fnpyKey)fileName,
			(ia->installInterfaceFlags & INSTALL_UPGRADE) != 0,
			relocations);

	headerFree(h);
	if (eiu->relocations)
	    eiu->relocations->oldPath = _free(eiu->relocations->oldPath);

	switch(rc) {
	case 0:
	    rpmlog(RPMLOG_DEBUG, "\tadded binary package [%d]\n",
			eiu->numRPMS);
	    break;
	case 1:
	    rpmlog(RPMLOG_ERR,
			    _("error reading from file %s\n"), *eiu->fnp);
	    eiu->numFailed++;
	    goto exit;
	    break;
	default:
	    eiu->numFailed++;
	    goto exit;
	    break;
	}

	eiu->numRPMS++;
    }

    rpmlog(RPMLOG_DEBUG, "found %d source and %d binary packages\n",
		eiu->numSRPMS, eiu->numRPMS);

    if (eiu->numFailed) goto exit;

    if (eiu->numRPMS) {
        int rc = rpmcliTransaction(ts, ia, eiu->numPkgs, 0, NULL);
        if (rc < 0)
            eiu->numFailed += eiu->numRPMS;
	else if (rc > 0)
            eiu->numFailed += rc;
    }

    if (eiu->numSRPMS && (eiu->sourceURL != NULL)) {
	rpmcliProgressState = 0;
	rpmcliProgressTotal = 0;
	rpmcliProgressCurrent = 0;
	for (i = 0; i < eiu->numSRPMS; i++) {
	    rpmdbCheckSignals();
	    if (eiu->sourceURL[i] != NULL) {
	        rc = RPMRC_OK;
		if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_TEST))
		    rc = rpmInstallSource(ts, eiu->sourceURL[i], NULL, NULL);
		if (rc != 0)
		    eiu->numFailed++;
	    }
	}
    }

exit:
    if (eiu->pkgURL != NULL) {
        for (i = 0; i < eiu->numPkgs; i++) {
	    if (eiu->pkgURL[i] == NULL) continue;
	    if (eiu->pkgState[i] == 1)
	        (void) unlink(eiu->pkgURL[i]);
	    eiu->pkgURL[i] = _free(eiu->pkgURL[i]);
	}
    }
    eiu->pkgState = _free(eiu->pkgState);
    eiu->pkgURL = _free(eiu->pkgURL);
    eiu->argv = _free(eiu->argv);
    rc = eiu->numFailed;
    free(eiu);

    rpmtsEmpty(ts);
    rpmtsSetVSFlags(ts, ovsflags);

    return rc;
}

int rpmInstallAppStore(rpmts ts,
                       struct rpmInstallArguments_s *ia,
                       ARGV_t fileArgv, 
                       void *probPtr)
{
    struct rpmEIU * eiu = xcalloc(1, sizeof(*eiu));
    rpmRelocation * relocations;
    char * fileURL = NULL;
    rpmVSFlags vsflags, ovsflags;
    int rc;
    int i;

    vsflags = setvsFlags(ia);
    ovsflags = rpmtsSetVSFlags(ts, (vsflags | RPMVSF_NEEDPAYLOAD));

    if (fileArgv == NULL) goto exit;

    (void) rpmtsSetFlags(ts, ia->transFlags);

    relocations = ia->relocations;

    setNotifyFlag(ia, ts); 

    if ((eiu->relocations = relocations) != NULL) {
	while (eiu->relocations->oldPath)
	    eiu->relocations++;
	if (eiu->relocations->newPath == NULL)
	    eiu->relocations = NULL;
    }

    /* Build fully globbed list of arguments in argv[argc]. */
    for (eiu->fnp = fileArgv; *eiu->fnp != NULL; eiu->fnp++) {
    	ARGV_t av = NULL;
    	int ac = 0;

	if (giFlags & RPMGI_NOGLOB) {
	    rc = rpmNoGlob(*eiu->fnp, &ac, &av);
	} else {
	    char * fn = rpmEscapeSpaces(*eiu->fnp);
	    rc = rpmGlob(fn, &ac, &av);
	    fn = _free(fn);
	}
	if (rc || ac == 0) {
	    if (giFlags & RPMGI_NOGLOB) {
		rpmlog(RPMLOG_ERR, _("File not found: %s\n"), *eiu->fnp);
	    } else {
		rpmlog(RPMLOG_ERR, _("File not found by glob: %s\n"), *eiu->fnp);
	    }
	    eiu->numFailed++;
	    continue;
	}

	argvAppend(&(eiu->argv), av);
	argvFree(av);
	eiu->argc += ac;
    }

restart:
    /* Allocate sufficient storage for next set of args. */
    if (eiu->pkgx >= eiu->numPkgs) {
	eiu->numPkgs = eiu->pkgx + eiu->argc;
	eiu->pkgURL = xrealloc(eiu->pkgURL,
			(eiu->numPkgs + 1) * sizeof(*eiu->pkgURL));
	memset(eiu->pkgURL + eiu->pkgx, 0,
			((eiu->argc + 1) * sizeof(*eiu->pkgURL)));
	eiu->pkgState = xrealloc(eiu->pkgState,
			(eiu->numPkgs + 1) * sizeof(*eiu->pkgState));
	memset(eiu->pkgState + eiu->pkgx, 0,
			((eiu->argc + 1) * sizeof(*eiu->pkgState)));
    }

    /* Retrieve next set of args, cache on local storage. */
    for (i = 0; i < eiu->argc; i++) {
	fileURL = _free(fileURL);
	fileURL = eiu->argv[i];
	eiu->argv[i] = NULL;

	switch (urlIsURL(fileURL)) {
	case URL_IS_HTTPS:
	case URL_IS_HTTP:
	case URL_IS_FTP:
	{   char *tfn = NULL;
	    FD_t tfd;

	    if (rpmIsVerbose())
		fprintf(stdout, _("Retrieving %s\n"), fileURL);

	    tfd = rpmMkTempFile(rpmtsRootDir(ts), &tfn);
	    if (tfd && tfn) {
		Fclose(tfd);
	    	rc = urlGetFile(fileURL, tfn);
	    } else {
		rc = -1;
	    }

	    if (rc != 0) {
		rpmlog(RPMLOG_ERR,
			_("skipping %s - transfer failed\n"), fileURL);
		eiu->numFailed++;
		eiu->pkgURL[eiu->pkgx] = NULL;
		tfn = _free(tfn);
		break;
	    }
	    eiu->pkgState[eiu->pkgx] = 1;
	    eiu->pkgURL[eiu->pkgx] = tfn;
	    eiu->pkgx++;
	}   break;
	case URL_IS_PATH:
	case URL_IS_DASH:	/* WRONG WRONG WRONG */
	case URL_IS_HKP:	/* WRONG WRONG WRONG */
	default:
	    eiu->pkgURL[eiu->pkgx] = fileURL;
	    fileURL = NULL;
	    eiu->pkgx++;
	    break;
	}
    }
    fileURL = _free(fileURL);

    if (eiu->numFailed) goto exit;

    /* Continue processing file arguments, building transaction set. */
    for (eiu->fnp = eiu->pkgURL+eiu->prevx;
	 *eiu->fnp != NULL;
	 eiu->fnp++, eiu->prevx++)
    {
	Header h = NULL;
	const char * fileName;

	rpmlog(RPMLOG_DEBUG, "============== %s\n", *eiu->fnp);
	(void) urlPath(*eiu->fnp, &fileName);

	if (tryReadHeader(ts, eiu, &h) == RPMRC_FAIL)
	    continue;

	if (eiu->rpmrc == RPMRC_NOTFOUND) {
	    rc = tryReadManifest(eiu, probPtr);
	    if (rc == RPMRC_OK) {
	        eiu->prevx++;
	        goto restart;
	    }
	}

	if (headerIsSource(h)) {
	    rpmlog(RPMLOG_DEBUG, "\tadded source package [%d]\n",
		eiu->numSRPMS);
	    eiu->sourceURL = xrealloc(eiu->sourceURL,
				(eiu->numSRPMS + 2) * sizeof(*eiu->sourceURL));
	    eiu->sourceURL[eiu->numSRPMS] = *eiu->fnp;
	    *eiu->fnp = NULL;
	    eiu->numSRPMS++;
	    eiu->sourceURL[eiu->numSRPMS] = NULL;
	    continue;
	}

	if (eiu->relocations) {
	    struct rpmtd_s prefixes;

	    headerGet(h, RPMTAG_PREFIXES, &prefixes, HEADERGET_DEFAULT);
	    if (rpmtdCount(&prefixes) == 1) {
		eiu->relocations->oldPath = xstrdup(rpmtdGetString(&prefixes));
		rpmtdFreeData(&prefixes);
	    } else {
		rpmlog(RPMLOG_ERR, _("package %s is not relocatable\n"),
		       headerGetString(h, RPMTAG_NAME));
		eiu->numFailed++;
		goto exit;
	    }
	}

	if (ia->installInterfaceFlags & INSTALL_FRESHEN)
	    if (checkFreshenStatus(ts, h) != 1) {
		headerFree(h);
	        continue;
	    }

	if (ia->installInterfaceFlags & INSTALL_REINSTALL)
	    rc = rpmtsAddReinstallElement(ts, h, (fnpyKey)fileName);
	else
	    rc = rpmtsAddInstallElementAppStore(ts, h, (fnpyKey)fileName,
			(ia->installInterfaceFlags & INSTALL_UPGRADE) != 0,
			relocations);

	headerFree(h);
	if (eiu->relocations)
	    eiu->relocations->oldPath = _free(eiu->relocations->oldPath);

	switch(rc) {
	case 0:
	    rpmlog(RPMLOG_DEBUG, "\tadded binary package [%d]\n",
			eiu->numRPMS);
	    break;
	case 1:
	    rpmlog(RPMLOG_ERR,
			    _("error reading from file %s\n"), *eiu->fnp);
	    eiu->numFailed++;
	    goto exit;
	    break;
	default:
	    eiu->numFailed++;
	    goto exit;
	    break;
	}

	eiu->numRPMS++;
    }

    rpmlog(RPMLOG_DEBUG, "found %d source and %d binary packages\n",
		eiu->numSRPMS, eiu->numRPMS);

    if (eiu->numFailed) goto exit;

    if (eiu->numRPMS) {
        int rc = rpmcliTransaction(ts, ia, eiu->numPkgs, 1, probPtr);
        if (rc < 0)
            eiu->numFailed += eiu->numRPMS;
	else if (rc > 0)
            eiu->numFailed += rc;
    }

    if (eiu->numSRPMS && (eiu->sourceURL != NULL)) {
	rpmcliProgressState = 0;
	rpmcliProgressTotal = 0;
	rpmcliProgressCurrent = 0;
	for (i = 0; i < eiu->numSRPMS; i++) {
	    rpmdbCheckSignals();
	    if (eiu->sourceURL[i] != NULL) {
	        rc = RPMRC_OK;
		if (!(rpmtsFlags(ts) & RPMTRANS_FLAG_TEST))
		    rc = rpmInstallSource(ts, eiu->sourceURL[i], NULL, NULL);
		if (rc != 0)
		    eiu->numFailed++;
	    }
	}
    }

exit:
    if (eiu->pkgURL != NULL) {
        for (i = 0; i < eiu->numPkgs; i++) {
	    if (eiu->pkgURL[i] == NULL) continue;
	    if (eiu->pkgState[i] == 1)
	        (void) unlink(eiu->pkgURL[i]);
	    eiu->pkgURL[i] = _free(eiu->pkgURL[i]);
	}
    }
    eiu->pkgState = _free(eiu->pkgState);
    eiu->pkgURL = _free(eiu->pkgURL);
    eiu->argv = _free(eiu->argv);
    rc = eiu->numFailed;
    free(eiu);

    rpmtsEmpty(ts);
    rpmtsSetVSFlags(ts, ovsflags);

    return rc;
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
    numFailed = rpmcliTransaction(ts, ia, numPackages, 0, NULL);
exit:
    rpmtsEmpty(ts);
    rpmtsSetVSFlags(ts, ovsflags);

    return numFailed;
}

int rpmEraseAppStore(rpmts ts, 
                     struct rpmInstallArguments_s *ia, 
                     ARGV_const_t argv)
{
    char * const * arg;
    char *qfmt = NULL;
    int numFailed = 0;
    int numPackages = 0;
    rpmVSFlags vsflags, ovsflags;

    if (argv == NULL) return 0;

    addMacro(NULL, "_dbpath", NULL, "/var/lib/appstore", RMIL_GLOBAL);

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
    numFailed = rpmcliTransaction(ts, ia, numPackages, 1, NULL);
exit:
    rpmtsEmpty(ts);
    rpmtsSetVSFlags(ts, ovsflags);

    return numFailed;
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

