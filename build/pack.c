/** \ingroup rpmbuild
 * \file build/pack.c
 *  Assemble components of an RPM package.
 */

#include "system.h"

#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <sys/wait.h>

#include <rpm/rpmlib.h>			/* RPMSIGTAG*, rpmReadPackageFile */
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlog.h>

#include "rpmio/rpmio_internal.h"	/* fdInitDigest, fdFiniDigest */
#include "lib/fsm.h"
#include "lib/signature.h"
#include "lib/rpmlead.h"
#include "build/rpmbuild_internal.h"
#include "build/rpmbuild_misc.h"

#include "debug.h"

static int rpmPackageFilesArchive(rpmfiles fi, int isSrc,
				  FD_t cfd, ARGV_t dpaths,
				  rpm_loff_t * archiveSize, char ** failedFile)
{
    int rc = 0;
    rpmfi archive = rpmfiNewArchiveWriter(cfd, fi);

    while (!rc && (rc = rpmfiNext(archive)) >= 0) {
        /* Copy file into archive. */
	FD_t rfd = NULL;
	const char *path = dpaths[rpmfiFX(archive)];

	rfd = Fopen(path, "r.ufdio");
	if (Ferror(rfd)) {
	    rc = RPMERR_OPEN_FAILED;
	} else {
	    rc = rpmfiArchiveWriteFile(archive, rfd);
	}

	if (rc && failedFile)
	    *failedFile = xstrdup(path);
	if (rfd) {
	    /* preserve any prior errno across close */
	    int myerrno = errno;
	    Fclose(rfd);
	    errno = myerrno;
	}
    }

    if (rc == RPMERR_ITER_END)
	rc = 0;

    /* Finish the payload stream */
    if (!rc)
	rc = rpmfiArchiveClose(archive);

    if (archiveSize)
	*archiveSize = (rc == 0) ? rpmfiArchiveTell(archive) : 0;

    rpmfiFree(archive);

    return rc;
}
/**
 * @todo Create transaction set *much* earlier.
 */
static rpmRC cpio_doio(FD_t fdo, Package pkg, const char * fmodeMacro)
{
    char *failedFile = NULL;
    FD_t cfd;
    int fsmrc;

    (void) Fflush(fdo);
    cfd = Fdopen(fdDup(Fileno(fdo)), fmodeMacro);
    if (cfd == NULL)
	return RPMRC_FAIL;

    fsmrc = rpmPackageFilesArchive(pkg->cpioList, headerIsSource(pkg->header),
				   cfd, pkg->dpaths,
				   &pkg->cpioArchiveSize, &failedFile);

    if (fsmrc) {
	char *emsg = rpmfileStrerror(fsmrc);
	if (failedFile)
	    rpmlog(RPMLOG_ERR, _("create archive failed on file %s: %s\n"),
		   failedFile, emsg);
	else
	    rpmlog(RPMLOG_ERR, _("create archive failed: %s\n"), emsg);
	free(emsg);
    }

    free(failedFile);
    Fclose(cfd);

    return (fsmrc == 0) ? RPMRC_OK : RPMRC_FAIL;
}

static rpmRC addFileToTag(rpmSpec spec, const char * file,
			  Header h, rpmTagVal tag, int append)
{
    StringBuf sb = NULL;
    char buf[BUFSIZ];
    char * fn;
    FILE * f;
    rpmRC rc = RPMRC_FAIL; /* assume failure */

    /* no script file is not an error */
    if (file == NULL)
	return RPMRC_OK;

    fn = rpmGetPath("%{_builddir}/%{?buildsubdir:%{buildsubdir}/}", file, NULL);

    f = fopen(fn, "r");
    if (f == NULL) {
	rpmlog(RPMLOG_ERR,_("Could not open %s file: %s\n"),
			   rpmTagGetName(tag), file);
	goto exit;
    }

    sb = newStringBuf();
    if (append) {
	const char *s = headerGetString(h, tag);
	if (s) {
	    appendLineStringBuf(sb, s);
	    headerDel(h, tag);
	}
    }

    while (fgets(buf, sizeof(buf), f)) {
	char *expanded;
	if(rpmExpandMacros(spec->macros, buf, &expanded, 0) < 0) {
	    rpmlog(RPMLOG_ERR, _("%s: line: %s\n"), fn, buf);
	    goto exit;
	}
	appendStringBuf(sb, expanded);
	free(expanded);
    }
    headerPutString(h, tag, getStringBuf(sb));
    rc = RPMRC_OK;

exit:
    if (f) fclose(f);
    free(fn);
    freeStringBuf(sb);

    return rc;
}

static rpm_time_t * getBuildTime(void)
{
    static rpm_time_t buildTime[1];

    if (buildTime[0] == 0)
	buildTime[0] = (int32_t) time(NULL);
    return buildTime;
}

static const char * buildHost(void)
{
    static char hostname[1024];
    static int oneshot = 0;
    struct hostent *hbn;
    char *bhMacro;

    if (! oneshot) {
        bhMacro = rpmExpand("%{?_buildhost}", NULL);
        if (strcmp(bhMacro, "") != 0 && strlen(bhMacro) < 1024) {
            strcpy(hostname, bhMacro);
        } else {
            if (strcmp(bhMacro, "") != 0)
                rpmlog(RPMLOG_WARNING, _("The _buildhost macro is too long\n"));
            (void) gethostname(hostname, sizeof(hostname));
            hbn = gethostbyname(hostname);
            if (hbn)
                strcpy(hostname, hbn->h_name);
            else
                rpmlog(RPMLOG_WARNING,
                        _("Could not canonicalize hostname: %s\n"), hostname);
        }
        free(bhMacro);
        oneshot = 1;
    }
    return(hostname);
}

static rpmRC processScriptFiles(rpmSpec spec, Package pkg)
{
    struct TriggerFileEntry *p;
    int addflags = 0;
    rpmRC rc = RPMRC_FAIL;
    Header h = pkg->header;
    struct TriggerFileEntry *tfa[] = {pkg->triggerFiles,
				      pkg->fileTriggerFiles,
				      pkg->transFileTriggerFiles};

    rpmTagVal progTags[] = {RPMTAG_TRIGGERSCRIPTPROG,
			    RPMTAG_FILETRIGGERSCRIPTPROG,
			    RPMTAG_TRANSFILETRIGGERSCRIPTPROG};

    rpmTagVal flagTags[] = {RPMTAG_TRIGGERSCRIPTFLAGS,
			    RPMTAG_FILETRIGGERSCRIPTFLAGS,
			    RPMTAG_TRANSFILETRIGGERSCRIPTFLAGS};

    rpmTagVal scriptTags[] = {RPMTAG_TRIGGERSCRIPTS,
			      RPMTAG_FILETRIGGERSCRIPTS,
			      RPMTAG_TRANSFILETRIGGERSCRIPTS};
    rpmTagVal priorityTags[] = {0,
				RPMTAG_FILETRIGGERPRIORITIES,
				RPMTAG_TRANSFILETRIGGERPRIORITIES};
    int i;
    
    if (addFileToTag(spec, pkg->preInFile, h, RPMTAG_PREIN, 1) ||
	addFileToTag(spec, pkg->preUnFile, h, RPMTAG_PREUN, 1) ||
	addFileToTag(spec, pkg->preTransFile, h, RPMTAG_PRETRANS, 1) ||
	addFileToTag(spec, pkg->postInFile, h, RPMTAG_POSTIN, 1) ||
	addFileToTag(spec, pkg->postUnFile, h, RPMTAG_POSTUN, 1) ||
	addFileToTag(spec, pkg->postTransFile, h, RPMTAG_POSTTRANS, 1) ||
	addFileToTag(spec, pkg->verifyFile, h, RPMTAG_VERIFYSCRIPT, 1))
    {
	goto exit;
    }


    for (i = 0; i < sizeof(tfa)/sizeof(tfa[0]); i++) {
	addflags = 0;
	/* if any trigger has flags, we need to add flags entry for all of them */
	for (p = tfa[i]; p != NULL; p = p->next) {
	    if (p->flags) {
		addflags = 1;
		break;
	    }
	}

	for (p = tfa[i]; p != NULL; p = p->next) {
	    headerPutString(h, progTags[i], p->prog);

	    if (priorityTags[i]) {
		headerPutUint32(h, priorityTags[i], &p->priority, 1);
	    }

	    if (addflags) {
		headerPutUint32(h, flagTags[i], &p->flags, 1);
	    }

	    if (p->script) {
		headerPutString(h, scriptTags[i], p->script);
	    } else if (p->fileName) {
		if (addFileToTag(spec, p->fileName, h, scriptTags[i], 0)) {
		    goto exit;
		}
	    } else {
		/* This is dumb.  When the header supports NULL string */
		/* this will go away.                                  */
		headerPutString(h, scriptTags[i], "");
	    }
	}
    }
    rc = RPMRC_OK;

exit:
    return rc;
}

static int haveTildeDep(Package pkg)
{
    for (int i = 0; i < PACKAGE_NUM_DEPS; i++) {
	rpmds ds = rpmdsInit(pkg->dependencies[i]);
	while (rpmdsNext(ds) >= 0) {
	    if (strchr(rpmdsEVR(ds), '~'))
		return 1;
	}
    }
    return 0;
}

static int haveRichDep(Package pkg)
{
    for (int i = 0; i < PACKAGE_NUM_DEPS; i++) {
	rpmds ds = rpmdsInit(pkg->dependencies[i]);
	rpmTagVal tagN = rpmdsTagN(ds);
	if (tagN != RPMTAG_REQUIRENAME && tagN != RPMTAG_CONFLICTNAME)
	    continue;
	while (rpmdsNext(ds) >= 0) {
	    if (rpmdsIsRich(ds))
		return 1;
	}
    }
    return 0;
}

static rpm_loff_t estimateCpioSize(Package pkg)
{
    rpmfi fi;
    rpm_loff_t size = 0;

    fi = rpmfilesIter(pkg->cpioList, RPMFI_ITER_FWD);
    while (rpmfiNext(fi) >= 0) {
	size += strlen(rpmfiDN(fi)) + strlen(rpmfiBN(fi));
	size += rpmfiFSize(fi);
	size += 300;
    }
    return size;
}

static rpmRC writeRPM(Package pkg, unsigned char ** pkgidp,
		      const char *fileName, char **cookie)
{
    FD_t fd = NULL;
    char * rpmio_flags = NULL;
    char * SHA1 = NULL;
    uint8_t * MD5 = NULL;
    const char *s;
    rpmRC rc = RPMRC_OK;
    rpm_loff_t estimatedCpioSize;
    unsigned char buf[32*BUFSIZ];
    uint8_t zeros[] =  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    char *zerosS = "0000000000000000000000000000000000000000";
    off_t sigStart;
    off_t sigTargetStart;
    off_t sigTargetSize;

    if (pkgidp)
	*pkgidp = NULL;

    /* Save payload information */
    if (headerIsSource(pkg->header))
	rpmio_flags = rpmExpand("%{?_source_payload}", NULL);
    else 
	rpmio_flags = rpmExpand("%{?_binary_payload}", NULL);

    /* If not configured or bogus, fall back to gz */
    if (rpmio_flags[0] != 'w') {
	free(rpmio_flags);
	rpmio_flags = xstrdup("w9.gzdio");
    }
    s = strchr(rpmio_flags, '.');
    if (s) {
	char *buf = NULL;
	const char *compr = NULL;
	headerPutString(pkg->header, RPMTAG_PAYLOADFORMAT, "cpio");

	if (rstreq(s+1, "ufdio")) {
	    compr = NULL;
	} else if (rstreq(s+1, "gzdio")) {
	    compr = "gzip";
#if HAVE_BZLIB_H
	} else if (rstreq(s+1, "bzdio")) {
	    compr = "bzip2";
	    /* Add prereq on rpm version that understands bzip2 payloads */
	    (void) rpmlibNeedsFeature(pkg, "PayloadIsBzip2", "3.0.5-1");
#endif
#if HAVE_LZMA_H
	} else if (rstreq(s+1, "xzdio")) {
	    compr = "xz";
	    (void) rpmlibNeedsFeature(pkg, "PayloadIsXz", "5.2-1");
	} else if (rstreq(s+1, "lzdio")) {
	    compr = "lzma";
	    (void) rpmlibNeedsFeature(pkg, "PayloadIsLzma", "4.4.6-1");
#endif
	} else {
	    rpmlog(RPMLOG_ERR, _("Unknown payload compression: %s\n"),
		   rpmio_flags);
	    rc = RPMRC_FAIL;
	    goto exit;
	}

	if (compr)
	    headerPutString(pkg->header, RPMTAG_PAYLOADCOMPRESSOR, compr);
	buf = xstrdup(rpmio_flags);
	buf[s - rpmio_flags] = '\0';
	headerPutString(pkg->header, RPMTAG_PAYLOADFLAGS, buf+1);
	free(buf);
    }

    /* check if the package has a dependency with a '~' */
    if (haveTildeDep(pkg))
	(void) rpmlibNeedsFeature(pkg, "TildeInVersions", "4.10.0-1");

    /* check if the package has a rich dependency */
    if (haveRichDep(pkg))
	(void) rpmlibNeedsFeature(pkg, "RichDependencies", "4.12.0-1");

    /* All dependencies added finally, write them into the header */
    for (int i = 0; i < PACKAGE_NUM_DEPS; i++) {
	/* Nuke any previously added dependencies from the header */
	headerDel(pkg->header, rpmdsTagN(pkg->dependencies[i]));
	headerDel(pkg->header, rpmdsTagEVR(pkg->dependencies[i]));
	headerDel(pkg->header, rpmdsTagF(pkg->dependencies[i]));
	headerDel(pkg->header, rpmdsTagTi(pkg->dependencies[i]));
	/* ...and add again, now with automatic dependencies included */
	rpmdsPutToHeader(pkg->dependencies[i], pkg->header);
    }

    /* Create and add the cookie */
    if (cookie) {
	rasprintf(cookie, "%s %d", buildHost(), (int) (*getBuildTime()));
	headerPutString(pkg->header, RPMTAG_COOKIE, *cookie);
    }
    
    /* Check for UTF-8 encoding of string tags, add encoding tag if all good */
    if (checkForEncoding(pkg->header, 1)) {
	rc = RPMRC_FAIL;
	goto exit;
    }

    /* Reallocate the header into one contiguous region. */
    pkg->header = headerReload(pkg->header, RPMTAG_HEADERIMMUTABLE);
    if (pkg->header == NULL) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Unable to create immutable header region.\n"));
	goto exit;
    }

    /* Open the output file */
    fd = Fopen(fileName, "w+.ufdio");
    if (fd == NULL || Ferror(fd)) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Could not open %s: %s\n"),
		fileName, Fstrerror(fd));
	goto exit;
    }

    /* Write the lead section into the package. */
    {	
	rpmlead lead = rpmLeadFromHeader(pkg->header);
	rc = rpmLeadWrite(fd, lead);
	rpmLeadFree(lead);
	if (rc != RPMRC_OK) {
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("Unable to write package: %s\n"),
		 Fstrerror(fd));
	    goto exit;
	}
    }

    /* Estimate cpio archive size to decide if use 32 bits or 64 bit tags. */
    estimatedCpioSize = estimateCpioSize(pkg);

    /* Save the position of signature section */
    sigStart = Ftell(fd);

    /* Generate and write a placeholder signature header */
    rc = rpmGenerateSignature(zerosS, zeros, 0, estimatedCpioSize, fd);
    if (rc != RPMRC_OK) {
	rc = RPMRC_FAIL;
	goto exit;
    }

    /* Write the header and archive section. Calculate SHA1 from them. */
    sigTargetStart = Ftell(fd);
    fdInitDigest(fd, PGPHASHALGO_SHA1, 0);
    if (headerWrite(fd, pkg->header, HEADER_MAGIC_YES)) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Unable to write temp header\n"));
	goto exit;
    }
    (void) Fflush(fd);
    fdFiniDigest(fd, PGPHASHALGO_SHA1, (void **)&SHA1, NULL, 1);

    /* Write payload section (cpio archive) */
    if (pkg->cpioList == NULL) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Bad CSA data\n"));
	goto exit;
    }
    rc = cpio_doio(fd, pkg, rpmio_flags);
    if (rc != RPMRC_OK)
	goto exit;

    sigTargetSize = Ftell(fd) - sigTargetStart;

    if(Fseek(fd, sigTargetStart, SEEK_SET) < 0) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Could not seek in file %s: %s\n"),
		fileName, Fstrerror(fd));
	goto exit;
    }

    /* Calculate MD5 checksum from header and archive section. */
    fdInitDigest(fd, PGPHASHALGO_MD5, 0);
    while (Fread(buf, sizeof(buf[0]), sizeof(buf), fd) > 0)
	;
    if (Ferror(fd)) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Fread failed in file %s: %s\n"),
		fileName, Fstrerror(fd));
	goto exit;
    }
    fdFiniDigest(fd, PGPHASHALGO_MD5, (void **)&MD5, NULL, 0);

    if(Fseek(fd, sigStart, SEEK_SET) < 0) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Could not seek in file %s: %s\n"),
		fileName, Fstrerror(fd));
	goto exit;
    }

    /* Generate the signature. Now with right values */
    rc = rpmGenerateSignature(SHA1, MD5, sigTargetSize, pkg->cpioArchiveSize, fd);
    if (rc != RPMRC_OK) {
	rc = RPMRC_FAIL;
	goto exit;
    }

exit:
    free(rpmio_flags);
    free(SHA1);

    /* XXX Fish the pkgid out of the signature header. */
    if (pkgidp != NULL) {
	if (MD5 != NULL) {
	    *pkgidp = MD5;
	}
    }

    Fclose(fd);

    if (rc == RPMRC_OK)
	rpmlog(RPMLOG_NOTICE, _("Wrote: %s\n"), fileName);
    else
	(void) unlink(fileName);

    return rc;
}

static const rpmTagVal copyTags[] = {
    RPMTAG_CHANGELOGTIME,
    RPMTAG_CHANGELOGNAME,
    RPMTAG_CHANGELOGTEXT,
    0
};

static rpmRC checkPackages(char *pkgcheck)
{
    int fail = rpmExpandNumeric("%{?_nonzero_exit_pkgcheck_terminate_build}");
    int xx;
    
    rpmlog(RPMLOG_NOTICE, _("Executing \"%s\":\n"), pkgcheck);
    xx = system(pkgcheck);
    if (WEXITSTATUS(xx) == -1 || WEXITSTATUS(xx) == 127) {
	rpmlog(RPMLOG_ERR, _("Execution of \"%s\" failed.\n"), pkgcheck);
	if (fail) return RPMRC_NOTFOUND;
    }
    if (WEXITSTATUS(xx) != 0) {
	rpmlog(RPMLOG_ERR, _("Package check \"%s\" failed.\n"), pkgcheck);
	if (fail) return RPMRC_FAIL;
    }
    
    return RPMRC_OK;
}

rpmRC packageBinaries(rpmSpec spec, const char *cookie, int cheating)
{
    rpmRC rc;
    const char *errorString;
    Package pkg;
    char *pkglist = NULL;

    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	char *fn;

	if (pkg->fileList == NULL)
	    continue;

	if ((rc = processScriptFiles(spec, pkg)))
	    return rc;
	
	if (cookie) {
	    headerPutString(pkg->header, RPMTAG_COOKIE, cookie);
	}

	/* Copy changelog from src rpm */
	headerCopyTags(spec->packages->header, pkg->header, copyTags);
	
	headerPutString(pkg->header, RPMTAG_RPMVERSION, VERSION);
	headerPutString(pkg->header, RPMTAG_BUILDHOST, buildHost());
	headerPutUint32(pkg->header, RPMTAG_BUILDTIME, getBuildTime(), 1);

	if (spec->sourcePkgId != NULL) {
	    headerPutBin(pkg->header, RPMTAG_SOURCEPKGID, spec->sourcePkgId,16);
	}

	if (cheating) {
	    (void) rpmlibNeedsFeature(pkg, "ShortCircuited", "4.9.0-1");
	}
	
	{   char *binFormat = rpmGetPath("%{_rpmfilename}", NULL);
	    char *binRpm, *binDir;
	    binRpm = headerFormat(pkg->header, binFormat, &errorString);
	    free(binFormat);
	    if (binRpm == NULL) {
		rpmlog(RPMLOG_ERR, _("Could not generate output "
		     "filename for package %s: %s\n"), 
		     headerGetString(pkg->header, RPMTAG_NAME), errorString);
		return RPMRC_FAIL;
	    }
	    fn = rpmGetPath("%{_rpmdir}/", binRpm, NULL);
	    if ((binDir = strchr(binRpm, '/')) != NULL) {
		struct stat st;
		char *dn;
		*binDir = '\0';
		dn = rpmGetPath("%{_rpmdir}/", binRpm, NULL);
		if (stat(dn, &st) < 0) {
		    switch(errno) {
		    case  ENOENT:
			if (mkdir(dn, 0755) == 0)
			    break;
		    default:
			rpmlog(RPMLOG_ERR,_("cannot create %s: %s\n"),
			    dn, strerror(errno));
			break;
		    }
		}
		free(dn);
	    }
	    free(binRpm);
	}

	rc = writeRPM(pkg, NULL, fn, NULL);
	if (rc == RPMRC_OK) {
	    /* Do check each written package if enabled */
	    char *pkgcheck = rpmExpand("%{?_build_pkgcheck} ", fn, NULL);
	    if (pkgcheck[0] != ' ') {
		rc = checkPackages(pkgcheck);
	    }
	    free(pkgcheck);
	    rstrcat(&pkglist, fn);
	    rstrcat(&pkglist, " ");
	}
	free(fn);
	if (rc != RPMRC_OK) {
	    pkglist = _free(pkglist);
	    return rc;
	}
    }

    /* Now check the package set if enabled */
    if (pkglist != NULL) {
	char *pkgcheck_set = rpmExpand("%{?_build_pkgcheck_set} ", pkglist,  NULL);
	if (pkgcheck_set[0] != ' ') {	/* run only if _build_pkgcheck_set is defined */
	    checkPackages(pkgcheck_set);
	}
	free(pkgcheck_set);
	pkglist = _free(pkglist);
    }
    
    return RPMRC_OK;
}

rpmRC packageSources(rpmSpec spec, char **cookie)
{
    Package sourcePkg = spec->sourcePackage;
    rpmRC rc;

    /* Add some cruft */
    headerPutString(sourcePkg->header, RPMTAG_RPMVERSION, VERSION);
    headerPutString(sourcePkg->header, RPMTAG_BUILDHOST, buildHost());
    headerPutUint32(sourcePkg->header, RPMTAG_BUILDTIME, getBuildTime(), 1);

    /* XXX this should be %_srpmdir */
    {	char *fn = rpmGetPath("%{_srcrpmdir}/", spec->sourceRpmName,NULL);
	char *pkgcheck = rpmExpand("%{?_build_pkgcheck_srpm} ", fn, NULL);

	spec->sourcePkgId = NULL;
	rc = writeRPM(sourcePkg, &spec->sourcePkgId, fn, cookie);

	/* Do check SRPM package if enabled */
	if (rc == RPMRC_OK && pkgcheck[0] != ' ') {
	    rc = checkPackages(pkgcheck);
	}

	free(pkgcheck);
	free(fn);
    }
    return rc;
}
