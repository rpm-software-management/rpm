/** \ingroup rpmbuild
 * \file build/pack.c
 *  Assemble components of an RPM package.
 */

#include "system.h"

#include <rpm/rpmlib.h>			/* RPMSIGTAG*, rpmReadPackageFile */
#include <rpm/rpmts.h>
#include <rpm/rpmbuild.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlog.h>

#include "rpmio/rpmio_internal.h"	/* fdInitDigest, fdFiniDigest */
#include "lib/cpio.h"
#include "lib/fsm.h"
#include "lib/rpmfi_internal.h"		/* rpmfiFSM() */
#include "lib/rpmte_internal.h"		/* rpmfs */
#include "lib/signature.h"
#include "lib/rpmlead.h"
#include "build/buildio.h"

#include "debug.h"

/**
 * @todo Create transaction set *much* earlier.
 */
static rpmRC cpio_doio(FD_t fdo, Header h, CSA_t csa,
		const char * fmodeMacro)
{
    rpmts ts = rpmtsCreate();
    rpmfi fi = csa->cpioList;
    rpmte te = NULL;
    rpmfs fs = NULL;
    char *failedFile = NULL;
    FD_t cfd;
    rpmRC rc = RPMRC_OK;
    int xx, i;

    {	char *fmode = rpmExpand(fmodeMacro, NULL);
	if (!(fmode && fmode[0] == 'w'))
	    fmode = xstrdup("w9.gzdio");
	(void) Fflush(fdo);
	cfd = Fdopen(fdDup(Fileno(fdo)), fmode);
	fmode = _free(fmode);
    }
    if (cfd == NULL)
	return RPMRC_FAIL;

    /* make up a transaction element for passing to fsm */
    rpmtsAddInstallElement(ts, h, NULL, 0, NULL);
    te = rpmtsElement(ts, 0);
    fs = rpmteGetFileStates(te);

    fi = rpmfiInit(fi, 0);
    while ((i = rpmfiNext(fi)) >= 0) {
	if (rpmfiFFlags(fi) & RPMFILE_GHOST)
	    rpmfsSetAction(fs, i, FA_SKIP);
	else
	    rpmfsSetAction(fs, i, FA_COPYOUT);
    }

    xx = fsmSetup(rpmfiFSM(fi), FSM_PKGBUILD, ts, te, fi, cfd,
		&csa->cpioArchiveSize, &failedFile);
    if (xx)
	rc = RPMRC_FAIL;
    (void) Fclose(cfd);
    xx = fsmTeardown(rpmfiFSM(fi));
    if (rc == RPMRC_OK && xx) rc = RPMRC_FAIL;

    if (rc) {
	if (failedFile)
	    rpmlog(RPMLOG_ERR, _("create archive failed on file %s: %s\n"),
		failedFile, cpioStrerror(rc));
	else
	    rpmlog(RPMLOG_ERR, _("create archive failed: %s\n"),
		cpioStrerror(rc));
      rc = RPMRC_FAIL;
    }
    rpmtsEmpty(ts);

    failedFile = _free(failedFile);
    ts = rpmtsFree(ts);

    return rc;
}

/**
 */
static rpmRC cpio_copy(FD_t fdo, CSA_t csa)
{
    char buf[BUFSIZ];
    size_t nb;

    while((nb = Fread(buf, sizeof(buf[0]), sizeof(buf), csa->cpioFdIn)) > 0) {
	if (Fwrite(buf, sizeof(buf[0]), nb, fdo) != nb) {
	    rpmlog(RPMLOG_ERR, _("cpio_copy write failed: %s\n"),
			Fstrerror(fdo));
	    return RPMRC_FAIL;
	}
	csa->cpioArchiveSize += nb;
    }
    if (Ferror(csa->cpioFdIn)) {
	rpmlog(RPMLOG_ERR, _("cpio_copy read failed: %s\n"),
		Fstrerror(csa->cpioFdIn));
	return RPMRC_FAIL;
    }
    return RPMRC_OK;
}

/**
 */
static StringBuf addFileToTagAux(rpmSpec spec,
		const char * file, StringBuf sb)
{
    char buf[BUFSIZ];
    char * fn;
    FILE * f;

    fn = rpmGetPath("%{_builddir}/%{?buildsubdir:%{buildsubdir}/}", file, NULL);

    f = fopen(fn, "r");
    if (f == NULL || ferror(f)) {
	sb = freeStringBuf(sb);
	goto exit;
    }
    while (fgets(buf, sizeof(buf), f)) {
	if (expandMacros(spec, spec->macros, buf, sizeof(buf))) {
	    rpmlog(RPMLOG_ERR, _("%s: line: %s\n"), fn, buf);
	    sb = freeStringBuf(sb);
	    break;
	}
	appendStringBuf(sb, buf);
    }
    (void) fclose(f);

exit:
    free(fn);

    return sb;
}

/**
 */
static int addFileToTag(rpmSpec spec, const char * file, Header h, rpmTag tag)
{
    StringBuf sb = newStringBuf();
    const char *s = headerGetString(h, tag);

    if (s) {
	appendLineStringBuf(sb, s);
	(void) headerDel(h, tag);
    }

    if ((sb = addFileToTagAux(spec, file, sb)) == NULL)
	return 1;
    
    headerPutString(h, tag, getStringBuf(sb));

    sb = freeStringBuf(sb);
    return 0;
}

/**
 */
static int addFileToArrayTag(rpmSpec spec, const char *file, Header h, rpmTag tag)
{
    StringBuf sb = newStringBuf();
    const char *s;

    if ((sb = addFileToTagAux(spec, file, sb)) == NULL)
	return 1;

    s = getStringBuf(sb);
    headerPutString(h, tag, s);

    sb = freeStringBuf(sb);
    return 0;
}

/**
 */
static rpmRC processScriptFiles(rpmSpec spec, Package pkg)
{
    struct TriggerFileEntry *p;
    
    if (pkg->preInFile) {
	if (addFileToTag(spec, pkg->preInFile, pkg->header, RPMTAG_PREIN)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PreIn file: %s\n"), pkg->preInFile);
	    return RPMRC_FAIL;
	}
    }
    if (pkg->preUnFile) {
	if (addFileToTag(spec, pkg->preUnFile, pkg->header, RPMTAG_PREUN)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PreUn file: %s\n"), pkg->preUnFile);
	    return RPMRC_FAIL;
	}
    }
    if (pkg->preTransFile) {
	if (addFileToTag(spec, pkg->preTransFile, pkg->header, RPMTAG_PRETRANS)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PreTrans file: %s\n"), pkg->preTransFile);
	    return RPMRC_FAIL;
	}
    }
    if (pkg->postInFile) {
	if (addFileToTag(spec, pkg->postInFile, pkg->header, RPMTAG_POSTIN)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PostIn file: %s\n"), pkg->postInFile);
	    return RPMRC_FAIL;
	}
    }
    if (pkg->postUnFile) {
	if (addFileToTag(spec, pkg->postUnFile, pkg->header, RPMTAG_POSTUN)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PostUn file: %s\n"), pkg->postUnFile);
	    return RPMRC_FAIL;
	}
    }
    if (pkg->postTransFile) {
	if (addFileToTag(spec, pkg->postTransFile, pkg->header, RPMTAG_POSTTRANS)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PostTrans file: %s\n"), pkg->postTransFile);
	    return RPMRC_FAIL;
	}
    }
    if (pkg->verifyFile) {
	if (addFileToTag(spec, pkg->verifyFile, pkg->header,
			 RPMTAG_VERIFYSCRIPT)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open VerifyScript file: %s\n"), pkg->verifyFile);
	    return RPMRC_FAIL;
	}
    }

    for (p = pkg->triggerFiles; p != NULL; p = p->next) {
	headerPutString(pkg->header, RPMTAG_TRIGGERSCRIPTPROG, p->prog);

	if (p->script) {
	    headerPutString(pkg->header, RPMTAG_TRIGGERSCRIPTS, p->script);
	} else if (p->fileName) {
	    if (addFileToArrayTag(spec, p->fileName, pkg->header,
				  RPMTAG_TRIGGERSCRIPTS)) {
		rpmlog(RPMLOG_ERR,
			 _("Could not open Trigger script file: %s\n"),
			 p->fileName);
		return RPMRC_FAIL;
	    }
	} else {
	    /* This is dumb.  When the header supports NULL string */
	    /* this will go away.                                  */
	    headerPutString(pkg->header, RPMTAG_TRIGGERSCRIPTS, "");
	}
    }

    return RPMRC_OK;
}

rpmRC readRPM(const char *fileName, rpmSpec *specp, 
		Header *sigs, CSA_t csa)
{
    FD_t fdi;
    rpmSpec spec;
    rpmRC rc;

    fdi = (fileName != NULL)
	? Fopen(fileName, "r.ufdio")
	: fdDup(STDIN_FILENO);

    if (fdi == NULL || Ferror(fdi)) {
	rpmlog(RPMLOG_ERR, _("readRPM: open %s: %s\n"),
		(fileName ? fileName : "<stdin>"),
		Fstrerror(fdi));
	if (fdi) (void) Fclose(fdi);
	return RPMRC_FAIL;
    }

    /* XXX FIXME: EPIPE on <stdin> */
    if (Fseek(fdi, 0, SEEK_SET) == -1) {
	rpmlog(RPMLOG_ERR, _("%s: Fseek failed: %s\n"),
			(fileName ? fileName : "<stdin>"), Fstrerror(fdi));
	return RPMRC_FAIL;
    }

    /* Reallocate build data structures */
    spec = newSpec();
    spec->packages = newPackage(spec);

    /* XXX the header just allocated will be allocated again */
    spec->packages->header = headerFree(spec->packages->header);

    /* Read the rpm lead, signatures, and header */
    {	rpmts ts = rpmtsCreate();

	/* XXX W2DO? pass fileName? */
	rc = rpmReadPackageFile(ts, fdi, "readRPM",
			 &spec->packages->header);

	ts = rpmtsFree(ts);

	if (sigs) *sigs = NULL;			/* XXX HACK */
    }

    switch (rc) {
    case RPMRC_OK:
    case RPMRC_NOKEY:
    case RPMRC_NOTTRUSTED:
	break;
    case RPMRC_NOTFOUND:
	rpmlog(RPMLOG_ERR, _("readRPM: %s is not an RPM package\n"),
		(fileName ? fileName : "<stdin>"));
	return RPMRC_FAIL;
    case RPMRC_FAIL:
    default:
	rpmlog(RPMLOG_ERR, _("readRPM: reading header from %s\n"),
		(fileName ? fileName : "<stdin>"));
	return RPMRC_FAIL;
	break;
    }

    if (specp)
	*specp = spec;
    else
	spec = freeSpec(spec);

    if (csa != NULL)
	csa->cpioFdIn = fdi;
    else
	(void) Fclose(fdi);

    return RPMRC_OK;
}

rpmRC writeRPM(Header *hdrp, unsigned char ** pkgidp, const char *fileName,
	     CSA_t csa, char *passPhrase, char **cookie)
{
    FD_t fd = NULL;
    FD_t ifd = NULL;
    ssize_t count;
    rpmSigTag sigtag;
    char * sigtarget = NULL;;
    char * rpmio_flags = NULL;
    char * SHA1 = NULL;
    char *s;
    char *buf = NULL;
    Header h;
    Header sig = NULL;
    int xx;
    rpmRC rc = RPMRC_OK;
    struct rpmtd_s td;
    rpmSigTag sizetag;
    rpmTag payloadtag;

    /* Transfer header reference form *hdrp to h. */
    h = headerLink(*hdrp);
    *hdrp = headerFree(*hdrp);

    if (pkgidp)
	*pkgidp = NULL;

    /* Save payload information */
    if (headerIsSource(h))
	rpmio_flags = rpmExpand("%{?_source_payload}", NULL);
    else 
	rpmio_flags = rpmExpand("%{?_binary_payload}", NULL);

    if (!(rpmio_flags && *rpmio_flags)) {
	rpmio_flags = _free(rpmio_flags);
	rpmio_flags = xstrdup("w9.gzdio");
    }
    s = strchr(rpmio_flags, '.');
    if (s) {
	const char *compr = NULL;
	headerPutString(h, RPMTAG_PAYLOADFORMAT, "cpio");

	if (rstreq(s+1, "gzdio")) {
	    compr = "gzip";
#if HAVE_BZLIB_H
	} else if (rstreq(s+1, "bzdio")) {
	    compr = "bzip2";
	    /* Add prereq on rpm version that understands bzip2 payloads */
	    (void) rpmlibNeedsFeature(h, "PayloadIsBzip2", "3.0.5-1");
#endif
#if HAVE_LZMA_H
	} else if (rstreq(s+1, "xzdio")) {
	    compr = "xz";
	    (void) rpmlibNeedsFeature(h, "PayloadIsXz", "5.2-1");
	} else if (rstreq(s+1, "lzdio")) {
	    compr = "lzma";
	    (void) rpmlibNeedsFeature(h, "PayloadIsLzma", "4.4.6-1");
#endif
	} else {
	    rpmlog(RPMLOG_ERR, _("Unknown payload compression: %s\n"),
		   rpmio_flags);
	    rc = RPMRC_FAIL;
	    goto exit;
	}

	headerPutString(h, RPMTAG_PAYLOADCOMPRESSOR, compr);
	buf = xstrdup(rpmio_flags);
	buf[s - rpmio_flags] = '\0';
	headerPutString(h, RPMTAG_PAYLOADFLAGS, buf+1);
	free(buf);
    }

    /* Create and add the cookie */
    if (cookie) {
	rasprintf(cookie, "%s %d", buildHost(), (int) (*getBuildTime()));
	headerPutString(h, RPMTAG_COOKIE, *cookie);
    }
    
    /* Reallocate the header into one contiguous region. */
    h = headerReload(h, RPMTAG_HEADERIMMUTABLE);
    if (h == NULL) {	/* XXX can't happen */
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Unable to create immutable header region.\n"));
	goto exit;
    }
    /* Re-reference reallocated header. */
    *hdrp = headerLink(h);

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
    if (headerWrite(fd, h, HEADER_MAGIC_YES)) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Unable to write temp header\n"));
    } else { /* Write the archive and get the size */
	(void) Fflush(fd);
	fdFiniDigest(fd, PGPHASHALGO_SHA1, (void **)&SHA1, NULL, 1);
	if (csa->cpioList != NULL) {
	    rc = cpio_doio(fd, h, csa, rpmio_flags);
	} else if (Fileno(csa->cpioFdIn) >= 0) {
	    rc = cpio_copy(fd, csa);
	} else {
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("Bad CSA data\n"));
	}
    }
    rpmio_flags = _free(rpmio_flags);

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
    if (csa->cpioArchiveSize < UINT32_MAX) {
	sizetag = RPMSIGTAG_SIZE;
	payloadtag = RPMSIGTAG_PAYLOADSIZE;
    } else {
	sizetag = RPMSIGTAG_LONGSIZE;
	payloadtag = RPMSIGTAG_LONGARCHIVESIZE;
    }
    (void) rpmAddSignature(sig, sigtarget, sizetag, passPhrase);
    (void) rpmAddSignature(sig, sigtarget, RPMSIGTAG_MD5, passPhrase);

    if ((sigtag = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY)) > 0) {
	rpmlog(RPMLOG_NOTICE, _("Generating signature: %d\n"), sigtag);
	(void) rpmAddSignature(sig, sigtarget, sigtag, passPhrase);
    }
    
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
	    rpm_off_t asize = csa->cpioArchiveSize;
	    td.type = RPM_INT32_TYPE;
	    td.data = &asize;
	    headerPut(sig, &td, HEADERPUT_DEFAULT);
	} else {
	    rpm_loff_t asize = csa->cpioArchiveSize;
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
	rpmlead lead = rpmLeadFromHeader(h);
	rc = rpmLeadWrite(fd, lead);
	lead = rpmLeadFree(lead);
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

#ifdef	NOTYET
	(void) headerMergeLegacySigs(nh, sig);
#endif

	xx = headerWrite(fd, nh, HEADER_MAGIC_YES);
	nh = headerFree(nh);

	if (xx) {
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("Unable to write header to %s: %s\n"),
			fileName, Fstrerror(fd));
	    goto exit;
	}
    }
	
    /* Write the payload into the package. */
    buf = xmalloc(BUFSIZ);
    while ((count = Fread(buf, 1, BUFSIZ, ifd)) > 0) {
	if (count == -1) {
	    free(buf);
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("Unable to read payload from %s: %s\n"),
		     sigtarget, Fstrerror(ifd));
	    goto exit;
	}
	if (Fwrite(buf, sizeof(buf[0]), count, fd) != count) {
	    free(buf);
	    rc = RPMRC_FAIL;
	    rpmlog(RPMLOG_ERR, _("Unable to write payload to %s: %s\n"),
		     fileName, Fstrerror(fd));
	    goto exit;
	}
    }
    free(buf);
    rc = RPMRC_OK;

exit:
    SHA1 = _free(SHA1);
    h = headerFree(h);

    /* XXX Fish the pkgid out of the signature header. */
    if (sig != NULL && pkgidp != NULL) {
	struct rpmtd_s md5tag;
	headerGet(sig, RPMSIGTAG_MD5, &md5tag, HEADERGET_DEFAULT);
	if (rpmtdType(&md5tag) == RPM_BIN_TYPE &&
	    			md5tag.count == 16 && md5tag.data != NULL) {
	    *pkgidp = md5tag.data;
	}
    }

    sig = rpmFreeSignature(sig);
    if (ifd) {
	(void) Fclose(ifd);
	ifd = NULL;
    }
    if (fd) {
	(void) Fclose(fd);
	fd = NULL;
    }
    if (sigtarget) {
	(void) unlink(sigtarget);
	sigtarget = _free(sigtarget);
    }

    if (rc == RPMRC_OK)
	rpmlog(RPMLOG_NOTICE, _("Wrote: %s\n"), fileName);
    else
	(void) unlink(fileName);

    return rc;
}

static const rpmTag copyTags[] = {
    RPMTAG_CHANGELOGTIME,
    RPMTAG_CHANGELOGNAME,
    RPMTAG_CHANGELOGTEXT,
    0
};

/*
 * Add extra provides to package.
 */
static void addPackageProvides(Header h)
{
    const char *arch, *name;
    char *evr, *isaprov;
    rpmsenseFlags pflags = RPMSENSE_EQUAL;

    /* <name> = <evr> provide */
    name = headerGetString(h, RPMTAG_NAME);
    arch = headerGetString(h, RPMTAG_ARCH);
    evr = headerGetAsString(h, RPMTAG_EVR);
    headerPutString(h, RPMTAG_PROVIDENAME, name);
    headerPutString(h, RPMTAG_PROVIDEVERSION, evr);
    headerPutUint32(h, RPMTAG_PROVIDEFLAGS, &pflags, 1);

    /*
     * <name>(<isa>) = <evr> provide
     * FIXME: noarch needs special casing for now as BuildArch: noarch doesn't
     * cause reading in the noarch macros :-/ 
     */
    isaprov = rpmExpand(name, "%{?_isa}", NULL);
    if (!rstreq(arch, "noarch") && !rstreq(name, isaprov)) {
	headerPutString(h, RPMTAG_PROVIDENAME, isaprov);
	headerPutString(h, RPMTAG_PROVIDEVERSION, evr);
	headerPutUint32(h, RPMTAG_PROVIDEFLAGS, &pflags, 1);
    }
    free(isaprov);
    free(evr);
}

rpmRC checkPackages(char *pkgcheck)
{
    int fail = rpmExpandNumeric("%{?_nonzero_exit_pkgcheck_terminate_build}");
    int xx;
    
    rpmlog(RPMLOG_NOTICE, _("Executing \"%s\":\n"), pkgcheck);
    xx = system(pkgcheck);
    if (WEXITSTATUS(xx) == -1 || WEXITSTATUS(xx) == 127) {
	rpmlog(RPMLOG_ERR, _("Execution of \"%s\" failed.\n"), pkgcheck);
	if (fail) return 127;
    }
    if (WEXITSTATUS(xx) != 0) {
	rpmlog(RPMLOG_ERR, _("Package check \"%s\" failed.\n"), pkgcheck);
	if (fail) return RPMRC_FAIL;
    }
    
    return RPMRC_OK;
}

rpmRC packageBinaries(rpmSpec spec)
{
    struct cpioSourceArchive_s csabuf;
    CSA_t csa = &csabuf;
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
	
	if (spec->cookie) {
	    headerPutString(pkg->header, RPMTAG_COOKIE, spec->cookie);
	}

	/* Copy changelog from src rpm */
	headerCopyTags(spec->packages->header, pkg->header, copyTags);
	
	headerPutString(pkg->header, RPMTAG_RPMVERSION, VERSION);
	headerPutString(pkg->header, RPMTAG_BUILDHOST, buildHost());
	headerPutUint32(pkg->header, RPMTAG_BUILDTIME, getBuildTime(), 1);

	addPackageProvides(pkg->header);

    {	char * optflags = rpmExpand("%{optflags}", NULL);
	headerPutString(pkg->header, RPMTAG_OPTFLAGS, optflags);
	optflags = _free(optflags);
    }

	if (spec->sourcePkgId != NULL) {
	    headerPutBin(pkg->header, RPMTAG_SOURCEPKGID, spec->sourcePkgId,16);
	}
	
	{   char *binFormat = rpmGetPath("%{_rpmfilename}", NULL);
	    char *binRpm, *binDir;
	    binRpm = headerFormat(pkg->header, binFormat, &errorString);
	    binFormat = _free(binFormat);
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
		dn = _free(dn);
	    }
	    binRpm = _free(binRpm);
	}

	memset(csa, 0, sizeof(*csa));
	csa->cpioArchiveSize = 0;
	csa->cpioFdIn = fdNew(RPMDBG_M("init (packageBinaries)"));
	csa->cpioList = rpmfiLink(pkg->cpioList, RPMDBG_M("packageBinaries"));

	rc = writeRPM(&pkg->header, NULL, fn, csa, spec->passPhrase, NULL);
	csa->cpioList = rpmfiFree(csa->cpioList);
	csa->cpioFdIn = fdFree(csa->cpioFdIn, 
			       RPMDBG_M("init (packageBinaries)"));
	if (rc == RPMRC_OK) {
	    /* Do check each written package if enabled */
	    char *pkgcheck = rpmExpand("%{?_build_pkgcheck} ", fn, NULL);
	    if (pkgcheck[0] != ' ') {
		rc = checkPackages(pkgcheck);
	    }
	    pkgcheck = _free(pkgcheck);
	    rstrcat(&pkglist, fn);
	    rstrcat(&pkglist, " ");
	}
	fn = _free(fn);
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
	pkgcheck_set = _free(pkgcheck_set);
	pkglist = _free(pkglist);
    }
    
    return RPMRC_OK;
}

rpmRC packageSources(rpmSpec spec)
{
    struct cpioSourceArchive_s csabuf;
    CSA_t csa = &csabuf;
    rpmRC rc;

    /* Add some cruft */
    headerPutString(spec->sourceHeader, RPMTAG_RPMVERSION, VERSION);
    headerPutString(spec->sourceHeader, RPMTAG_BUILDHOST, buildHost());
    headerPutUint32(spec->sourceHeader, RPMTAG_BUILDTIME, getBuildTime(), 1);

    spec->cookie = _free(spec->cookie);
    
    /* XXX this should be %_srpmdir */
    {	char *fn = rpmGetPath("%{_srcrpmdir}/", spec->sourceRpmName,NULL);
	char *pkgcheck = rpmExpand("%{?_build_pkgcheck_srpm} ", fn, NULL);

	memset(csa, 0, sizeof(*csa));
	csa->cpioArchiveSize = 0;
	csa->cpioFdIn = fdNew(RPMDBG_M("init (packageSources)"));
	csa->cpioList = rpmfiLink(spec->sourceCpioList, 
				  RPMDBG_M("packageSources"));

	spec->sourcePkgId = NULL;
	rc = writeRPM(&spec->sourceHeader, &spec->sourcePkgId, fn, 
		csa, spec->passPhrase, &(spec->cookie));

	/* Do check SRPM package if enabled */
	if (rc == RPMRC_OK && pkgcheck[0] != ' ') {
	    rc = checkPackages(pkgcheck);
	}

	csa->cpioList = rpmfiFree(csa->cpioList);
	csa->cpioFdIn = fdFree(csa->cpioFdIn, 
			       RPMDBG_M("init (packageSources)"));
	pkgcheck = _free(pkgcheck);
	fn = _free(fn);
    }
    return rc;
}
