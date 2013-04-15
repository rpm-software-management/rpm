/** \ingroup rpmbuild
 * \file build/pack.c
 *  Assemble components of an RPM package.
 */

#include "system.h"

#include <errno.h>
#include <netdb.h>
#include <time.h>

#include <rpm/rpmlib.h>			/* RPMSIGTAG*, rpmReadPackageFile */
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlog.h>

#include "rpmio/rpmio_internal.h"	/* fdInitDigest, fdFiniDigest */
#include "lib/fsm.h"
#include "lib/cpio.h"
#include "lib/signature.h"
#include "lib/rpmlead.h"
#include "build/rpmbuild_internal.h"
#include "build/rpmbuild_misc.h"

#include "debug.h"

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
				   cfd, &pkg->cpioArchiveSize, &failedFile);

    if (fsmrc) {
	if (failedFile)
	    rpmlog(RPMLOG_ERR, _("create archive failed on file %s: %s\n"),
		   failedFile, rpmcpioStrerror(fsmrc));
	else
	    rpmlog(RPMLOG_ERR, _("create archive failed: %s\n"),
		   rpmcpioStrerror(fsmrc));
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
	if (expandMacros(spec, spec->macros, buf, sizeof(buf))) {
	    rpmlog(RPMLOG_ERR, _("%s: line: %s\n"), fn, buf);
	    goto exit;
	}
	appendStringBuf(sb, buf);
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

    if (! oneshot) {
        (void) gethostname(hostname, sizeof(hostname));
	hbn = gethostbyname(hostname);
	if (hbn)
	    strcpy(hostname, hbn->h_name);
	else
	    rpmlog(RPMLOG_WARNING,
			_("Could not canonicalize hostname: %s\n"), hostname);
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

    /* if any trigger has flags, we need to add flags entry for all of them */
    for (p = pkg->triggerFiles; p != NULL; p = p->next) {
	if (p->flags) {
	    addflags = 1;
	    break;
	}
    }

    for (p = pkg->triggerFiles; p != NULL; p = p->next) {
	headerPutString(h, RPMTAG_TRIGGERSCRIPTPROG, p->prog);
	if (addflags) {
	    headerPutUint32(h, RPMTAG_TRIGGERSCRIPTFLAGS, &p->flags, 1);
	}

	if (p->script) {
	    headerPutString(h, RPMTAG_TRIGGERSCRIPTS, p->script);
	} else if (p->fileName) {
	    if (addFileToTag(spec, p->fileName, h, RPMTAG_TRIGGERSCRIPTS, 0)) {
		goto exit;
	    }
	} else {
	    /* This is dumb.  When the header supports NULL string */
	    /* this will go away.                                  */
	    headerPutString(h, RPMTAG_TRIGGERSCRIPTS, "");
	}
    }
    rc = RPMRC_OK;

exit:
    return rc;
}

static rpmRC copyPayload(FD_t ifd, const char *ifn, FD_t ofd, const char *ofn)
{
    char buf[BUFSIZ];
    size_t nb;
    rpmRC rc = RPMRC_OK;

    while ((nb = Fread(buf, 1, sizeof(buf), ifd)) > 0) {
	if (Fwrite(buf, sizeof(buf[0]), nb, ofd) != nb) {
	    rpmlog(RPMLOG_ERR, _("Unable to write payload to %s: %s\n"),
		     ofn, Fstrerror(ofd));
	    rc = RPMRC_FAIL;
	    break;
	}
    }

    if (nb < 0) {
	rpmlog(RPMLOG_ERR, _("Unable to read payload from %s: %s\n"),
		 ifn, Fstrerror(ifd));
	rc = RPMRC_FAIL;
    }

    return rc;
}

static int depContainsTilde(Header h, rpmTagVal tagEVR)
{
    struct rpmtd_s evrs;
    const char *evr = NULL;

    if (headerGet(h, tagEVR, &evrs, HEADERGET_MINMEM)) {
	while ((evr = rpmtdNextString(&evrs)) != NULL)
	    if (strchr(evr, '~'))
		break;
	rpmtdFreeData(&evrs);
    }
    return evr != NULL;
}

static rpmTagVal depevrtags[] = {
    RPMTAG_PROVIDEVERSION,
    RPMTAG_REQUIREVERSION,
    RPMTAG_OBSOLETEVERSION,
    RPMTAG_CONFLICTVERSION,
    RPMTAG_ORDERVERSION,
    RPMTAG_TRIGGERVERSION,
    RPMTAG_SUGGESTSVERSION,
    RPMTAG_ENHANCESVERSION,
    0
};

static int haveTildeDep(Header h)
{
    int i;

    for (i = 0; depevrtags[i] != 0; i++)
	if (depContainsTilde(h, depevrtags[i]))
	    return 1;
    return 0;
}

static rpmRC writeRPM(Package pkg, unsigned char ** pkgidp,
		      const char *fileName, char **cookie)
{
    FD_t fd = NULL;
    FD_t ifd = NULL;
    char * sigtarget = NULL;;
    char * rpmio_flags = NULL;
    char * SHA1 = NULL;
    const char *s;
    Header sig = NULL;
    int xx;
    rpmRC rc = RPMRC_OK;
    struct rpmtd_s td;
    rpmTagVal sizetag;
    rpmTagVal payloadtag;

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
    if (haveTildeDep(pkg->header))
	(void) rpmlibNeedsFeature(pkg, "TildeInVersions", "4.10.0-1");

    /* Create and add the cookie */
    if (cookie) {
	rasprintf(cookie, "%s %d", buildHost(), (int) (*getBuildTime()));
	headerPutString(pkg->header, RPMTAG_COOKIE, *cookie);
    }
    
    /* Reallocate the header into one contiguous region. */
    pkg->header = headerReload(pkg->header, RPMTAG_HEADERIMMUTABLE);
    if (pkg->header == NULL) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Unable to create immutable header region.\n"));
	goto exit;
    }

    /*
     * Write the header+archive into a temp file so that the size of
     * archive (after compression) can be added to the header.
     */
    fd = rpmMkTempFile(NULL, &sigtarget);
    if (fd == NULL || Ferror(fd)) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Unable to open temp file.\n"));
	goto exit;
    }

    fdInitDigest(fd, PGPHASHALGO_SHA1, 0);
    if (headerWrite(fd, pkg->header, HEADER_MAGIC_YES)) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Unable to write temp header\n"));
    } else { /* Write the archive and get the size */
	(void) Fflush(fd);
	fdFiniDigest(fd, PGPHASHALGO_SHA1, (void **)&SHA1, NULL, 1);
	if (pkg->cpioList != NULL) {
	    rc = cpio_doio(fd, pkg, rpmio_flags);
	} else {
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("Bad CSA data\n"));
	}
    }

    if (rc != RPMRC_OK)
	goto exit;

    (void) Fclose(fd);
    fd = NULL;
    (void) unlink(fileName);

    /* Generate the signature */
    (void) fflush(stdout);
    sig = rpmNewSignature();

    /*
     * There should be rpmlib() dependency on this, but that doesn't
     * really do much good as these are signature tags that get read
     * way before dependency checking has a chance to figure out anything.
     * On the positive side, not inserting the 32bit tag at all means
     * older rpm will just bail out with error message on attempt to read
     * such a package.
     */
    if (pkg->cpioArchiveSize < UINT32_MAX) {
	sizetag = RPMSIGTAG_SIZE;
	payloadtag = RPMSIGTAG_PAYLOADSIZE;
    } else {
	sizetag = RPMSIGTAG_LONGSIZE;
	payloadtag = RPMSIGTAG_LONGARCHIVESIZE;
    }
    (void) rpmGenDigest(sig, sigtarget, sizetag);
    (void) rpmGenDigest(sig, sigtarget, RPMSIGTAG_MD5);

    if (SHA1) {
	/* XXX can't use rpmtdFromFoo() on RPMSIGTAG_* items */
	rpmtdReset(&td);
	td.tag = RPMSIGTAG_SHA1;
	td.type = RPM_STRING_TYPE;
	td.data = SHA1;
	td.count = 1;
	headerPut(sig, &td, HEADERPUT_DEFAULT);
	SHA1 = _free(SHA1);
    }

    {	
	/* XXX can't use headerPutType() on legacy RPMSIGTAG_* items */
	rpmtdReset(&td);
	td.tag = payloadtag;
	td.count = 1;
	if (payloadtag == RPMSIGTAG_PAYLOADSIZE) {
	    rpm_off_t asize = pkg->cpioArchiveSize;
	    td.type = RPM_INT32_TYPE;
	    td.data = &asize;
	    headerPut(sig, &td, HEADERPUT_DEFAULT);
	} else {
	    rpm_loff_t asize = pkg->cpioArchiveSize;
	    td.type = RPM_INT64_TYPE;
	    td.data = &asize;
	    headerPut(sig, &td, HEADERPUT_DEFAULT);
	}
    }

    /* Reallocate the signature into one contiguous region. */
    sig = headerReload(sig, RPMTAG_HEADERSIGNATURES);
    if (sig == NULL) {	/* XXX can't happen */
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Unable to reload signature header.\n"));
	goto exit;
    }

    /* Open the output file */
    fd = Fopen(fileName, "w.ufdio");
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

    /* Write the signature section into the package. */
    if (rpmWriteSignature(fd, sig)) {
	rc = RPMRC_FAIL;
	goto exit;
    }

    /* Append the header and archive */
    ifd = Fopen(sigtarget, "r.ufdio");
    if (ifd == NULL || Ferror(ifd)) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Unable to open sigtarget %s: %s\n"),
		sigtarget, Fstrerror(ifd));
	goto exit;
    }

    /* Add signatures to header, and write header into the package. */
    /* XXX header+payload digests/signatures might be checked again here. */
    {	Header nh = headerRead(ifd, HEADER_MAGIC_YES);

	if (nh == NULL) {
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("Unable to read header from %s: %s\n"),
			sigtarget, Fstrerror(ifd));
	    goto exit;
	}

	xx = headerWrite(fd, nh, HEADER_MAGIC_YES);
	headerFree(nh);

	if (xx) {
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("Unable to write header to %s: %s\n"),
			fileName, Fstrerror(fd));
	    goto exit;
	}
    }
	
    /* Write the payload into the package. */
    rc = copyPayload(ifd, fileName, fd, sigtarget);

exit:
    free(rpmio_flags);
    free(SHA1);

    /* XXX Fish the pkgid out of the signature header. */
    if (sig != NULL && pkgidp != NULL) {
	struct rpmtd_s md5tag;
	headerGet(sig, RPMSIGTAG_MD5, &md5tag, HEADERGET_DEFAULT);
	if (rpmtdType(&md5tag) == RPM_BIN_TYPE &&
	    			md5tag.count == 16 && md5tag.data != NULL) {
	    *pkgidp = md5tag.data;
	}
    }

    rpmFreeSignature(sig);
    Fclose(ifd);
    Fclose(fd);

    if (sigtarget) {
	(void) unlink(sigtarget);
	free(sigtarget);
    }

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
