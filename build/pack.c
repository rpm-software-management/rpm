/** \ingroup rpmbuild
 * \file build/pack.c
 *  Assemble components of an RPM package.
 */

#include "system.h"

#include <rpmio_internal.h>
#include <rpmbuild.h>
#include "buildio.h"

#include "misc.h"
#include "signature.h"
#include "rpmlead.h"
#include "debug.h"

extern int _noDirTokens;

/*@access StringBuf @*/	/* compared with NULL */
/*@access TFI_t @*/	/* compared with NULL */
/*@access Header @*/	/* compared with NULL */
/*@access FD_t @*/	/* compared with NULL */

/**
 */
static inline int genSourceRpmName(Spec spec)
{
    if (spec->sourceRpmName == NULL) {
	const char *name, *version, *release;
	char fileName[BUFSIZ];

	(void) headerNVR(spec->packages->header, &name, &version, &release);
	sprintf(fileName, "%s-%s-%s.%ssrc.rpm", name, version, release,
	    spec->noSource ? "no" : "");
	spec->sourceRpmName = xstrdup(fileName);
    }

    return 0;
}

/**
 * @todo Create transaction set *much* earlier.
 */
static int cpio_doio(FD_t fdo, /*@unused@*/ Header h, CSA_t * csa,
		const char * fmodeMacro)
{
    const char * rootDir = "/";
    rpmdb rpmdb = NULL;
    rpmTransactionSet ts = rpmtransCreateSet(rpmdb, rootDir);
    TFI_t fi = csa->cpioList;
    const char *fmode = rpmExpand(fmodeMacro, NULL);
    const char *failedFile = NULL;
    FD_t cfd;
    int rc;

    if (!(fmode && fmode[0] == 'w'))
	fmode = xstrdup("w9.gzdio");
    (void) Fflush(fdo);
    cfd = Fdopen(fdDup(Fileno(fdo)), fmode);

    rc = fsmSetup(fi->fsm, FSM_PKGBUILD, ts, fi, cfd,
		&csa->cpioArchiveSize, &failedFile);
    (void) Fclose(cfd);
    (void) fsmTeardown(fi->fsm);

    if (rc) {
	rpmError(RPMERR_CPIO, _("create archive failed on file %s: %s\n"),
		failedFile, cpioStrerror(rc));
      rc = 1;
    }

    failedFile = _free(failedFile);
    fmode = _free(fmode);
    rpmtransFree(ts);

    return rc;
}

/**
 */
static int cpio_copy(FD_t fdo, CSA_t *csa)
{
    char buf[BUFSIZ];
    size_t nb;

    while((nb = Fread(buf, sizeof(buf[0]), sizeof(buf), csa->cpioFdIn)) > 0) {
	if (Fwrite(buf, sizeof(buf[0]), nb, fdo) != nb) {
	    rpmError(RPMERR_CPIO, _("cpio_copy write failed: %s\n"),
			Fstrerror(fdo));
	    return 1;
	}
	csa->cpioArchiveSize += nb;
    }
    if (Ferror(csa->cpioFdIn)) {
	rpmError(RPMERR_CPIO, _("cpio_copy read failed: %s\n"),
		Fstrerror(csa->cpioFdIn));
	return 1;
    }
    return 0;
}

/**
 */
static /*@only@*/ /*@null@*/ StringBuf addFileToTagAux(Spec spec,
		const char * file, /*@only@*/ StringBuf sb)
{
    char buf[BUFSIZ];
    const char * fn = buf;
    FILE * f;
    FD_t fd;

    /* XXX use rpmGenPath(rootdir, "%{_buildir}/%{_buildsubdir}/", file) */
    fn = rpmGetPath("%{_builddir}/", spec->buildSubdir, "/", file, NULL);

    fd = Fopen(fn, "r.ufdio");
    if (fn != buf) fn = _free(fn);
    if (fd == NULL || Ferror(fd)) {
	freeStringBuf(sb);
	return NULL;
    }
    f = fdGetFp(fd);
    while (fgets(buf, sizeof(buf), f)) {
	/* XXX display fn in error msg */
	if (expandMacros(spec, spec->macros, buf, sizeof(buf))) {
	    rpmError(RPMERR_BADSPEC, _("line: %s\n"), buf);
	    freeStringBuf(sb);
	    sb = NULL;
	    break;
	}
	appendStringBuf(sb, buf);
    }
    (void) Fclose(fd);

    return sb;
}

/**
 */
static int addFileToTag(Spec spec, const char * file, Header h, int tag)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    StringBuf sb = newStringBuf();
    char *s;

    if (hge(h, tag, NULL, (void **)&s, NULL)) {
	appendLineStringBuf(sb, s);
	(void) headerRemoveEntry(h, tag);
    }

    if ((sb = addFileToTagAux(spec, file, sb)) == NULL)
	return 1;
    
    (void) headerAddEntry(h, tag, RPM_STRING_TYPE, getStringBuf(sb), 1);

    freeStringBuf(sb);
    return 0;
}

/**
 */
static int addFileToArrayTag(Spec spec, const char *file, Header h, int tag)
{
    StringBuf sb = newStringBuf();
    char *s;

    if ((sb = addFileToTagAux(spec, file, sb)) == NULL)
	return 1;

    s = getStringBuf(sb);
    (void) headerAddOrAppendEntry(h, tag, RPM_STRING_ARRAY_TYPE, &s, 1);

    freeStringBuf(sb);
    return 0;
}

/**
 */
static int processScriptFiles(Spec spec, Package pkg)
{
    struct TriggerFileEntry *p;
    
    if (pkg->preInFile) {
	if (addFileToTag(spec, pkg->preInFile, pkg->header, RPMTAG_PREIN)) {
	    rpmError(RPMERR_BADFILENAME,
		     _("Could not open PreIn file: %s\n"), pkg->preInFile);
	    return RPMERR_BADFILENAME;
	}
    }
    if (pkg->preUnFile) {
	if (addFileToTag(spec, pkg->preUnFile, pkg->header, RPMTAG_PREUN)) {
	    rpmError(RPMERR_BADFILENAME,
		     _("Could not open PreUn file: %s\n"), pkg->preUnFile);
	    return RPMERR_BADFILENAME;
	}
    }
    if (pkg->postInFile) {
	if (addFileToTag(spec, pkg->postInFile, pkg->header, RPMTAG_POSTIN)) {
	    rpmError(RPMERR_BADFILENAME,
		     _("Could not open PostIn file: %s\n"), pkg->postInFile);
	    return RPMERR_BADFILENAME;
	}
    }
    if (pkg->postUnFile) {
	if (addFileToTag(spec, pkg->postUnFile, pkg->header, RPMTAG_POSTUN)) {
	    rpmError(RPMERR_BADFILENAME,
		     _("Could not open PostUn file: %s\n"), pkg->postUnFile);
	    return RPMERR_BADFILENAME;
	}
    }
    if (pkg->verifyFile) {
	if (addFileToTag(spec, pkg->verifyFile, pkg->header,
			 RPMTAG_VERIFYSCRIPT)) {
	    rpmError(RPMERR_BADFILENAME,
		     _("Could not open VerifyScript file: %s\n"), pkg->verifyFile);
	    return RPMERR_BADFILENAME;
	}
    }

    for (p = pkg->triggerFiles; p != NULL; p = p->next) {
	(void) headerAddOrAppendEntry(pkg->header, RPMTAG_TRIGGERSCRIPTPROG,
			       RPM_STRING_ARRAY_TYPE, &(p->prog), 1);
	if (p->script) {
	    (void) headerAddOrAppendEntry(pkg->header, RPMTAG_TRIGGERSCRIPTS,
				   RPM_STRING_ARRAY_TYPE, &(p->script), 1);
	} else if (p->fileName) {
	    if (addFileToArrayTag(spec, p->fileName, pkg->header,
				  RPMTAG_TRIGGERSCRIPTS)) {
		rpmError(RPMERR_BADFILENAME,
			 _("Could not open Trigger script file: %s\n"),
			 p->fileName);
		return RPMERR_BADFILENAME;
	    }
	} else {
	    /* This is dumb.  When the header supports NULL string */
	    /* this will go away.                                  */
	    char *bull = "";
	    (void) headerAddOrAppendEntry(pkg->header, RPMTAG_TRIGGERSCRIPTS,
				   RPM_STRING_ARRAY_TYPE, &bull, 1);
	}
    }

    return 0;
}

int readRPM(const char *fileName, Spec *specp, struct rpmlead *lead, Header *sigs,
	    CSA_t *csa)
{
    FD_t fdi;
    Spec spec;
    rpmRC rc;

    if (fileName != NULL) {
	fdi = Fopen(fileName, "r.ufdio");
	if (fdi == NULL || Ferror(fdi)) {
	    rpmError(RPMERR_BADMAGIC, _("readRPM: open %s: %s\n"), fileName,
		Fstrerror(fdi));
	    return RPMERR_BADMAGIC;
	}
    } else {
	fdi = fdDup(STDIN_FILENO);
    }

    /* Get copy of lead */
    if ((rc = Fread(lead, sizeof(char), sizeof(*lead), fdi)) != sizeof(*lead)) {
	rpmError(RPMERR_BADMAGIC, _("readRPM: read %s: %s\n"), fileName,
	    Fstrerror(fdi));
	return RPMERR_BADMAGIC;
    }

    (void)Fseek(fdi, 0, SEEK_SET);	/* XXX FIXME: EPIPE */

    /* Reallocate build data structures */
    spec = newSpec();
    spec->packages = newPackage(spec);

    /* XXX the header just allocated will be allocated again */
    if (spec->packages->header != NULL) {
	headerFree(spec->packages->header);
	spec->packages->header = NULL;
    }

   /* Read the rpm lead and header */
    rc = rpmReadPackageInfo(fdi, sigs, &spec->packages->header);
    switch (rc) {
    case RPMRC_BADMAGIC:
	rpmError(RPMERR_BADMAGIC, _("readRPM: %s is not an RPM package\n"),
		fileName);
	return RPMERR_BADMAGIC;
    case RPMRC_OK:
	break;
    case RPMRC_FAIL:
    case RPMRC_BADSIZE:
    case RPMRC_SHORTREAD:
    default:
	rpmError(RPMERR_BADMAGIC, _("readRPM: reading header from %s\n"),
		fileName);
	return RPMERR_BADMAGIC;
	/*@notreached@*/ break;
    }

    if (specp)
	*specp = spec;
    else
	freeSpec(spec);

    if (csa)
	csa->cpioFdIn = fdi;
    else
	(void) Fclose(fdi);

    return 0;
}

int writeRPM(Header *hdrp, const char *fileName, int type,
		    CSA_t *csa, char *passPhrase, const char **cookie)
{
    FD_t fd = NULL;
    FD_t ifd = NULL;
    int rc, count, sigtype;
    const char *sigtarget;
    const char * rpmio_flags = NULL;
    char *s;
    char buf[BUFSIZ];
    Header h = *hdrp;
    Header sig = NULL;

    if (Fileno(csa->cpioFdIn) < 0) {
	csa->cpioArchiveSize = 0;
	/* Add a bogus archive size to the Header */
	(void) headerAddEntry(h, RPMTAG_ARCHIVESIZE, RPM_INT32_TYPE,
		&csa->cpioArchiveSize, 1);
    }

#ifdef	DYING
    /* Choose how filenames are represented. */
    if (_noDirTokens)
	expandFilelist(h);
    else {
	compressFilelist(h);
	/* Binary packages with dirNames cannot be installed by legacy rpm. */
	if (type == RPMLEAD_BINARY)
	    rpmlibNeedsFeature(h, "CompressedFileNames", "3.0.4-1");
    }
#endif

    /* Binary packages now have explicit Provides: name = version-release. */
    if (type == RPMLEAD_BINARY)
	providePackageNVR(h);

    /* Save payload information */
    switch(type) {
    case RPMLEAD_SOURCE:
	rpmio_flags = rpmExpand("%{?_source_payload:%{_source_payload}}", NULL);
	break;
    case RPMLEAD_BINARY:
	rpmio_flags = rpmExpand("%{?_binary_payload:%{_binary_payload}}", NULL);
	break;
    }
    if (!(rpmio_flags && *rpmio_flags)) {
	rpmio_flags = _free(rpmio_flags);
	rpmio_flags = xstrdup("w9.gzdio");
    }
    s = strchr(rpmio_flags, '.');
    if (s) {
	(void) headerAddEntry(h, RPMTAG_PAYLOADFORMAT, RPM_STRING_TYPE, "cpio", 1);
	if (s[1] == 'g' && s[2] == 'z')
	    (void) headerAddEntry(h, RPMTAG_PAYLOADCOMPRESSOR, RPM_STRING_TYPE,
		"gzip", 1);
	if (s[1] == 'b' && s[2] == 'z') {
	    (void) headerAddEntry(h, RPMTAG_PAYLOADCOMPRESSOR, RPM_STRING_TYPE,
		"bzip2", 1);
	    /* Add prereq on rpm version that understands bzip2 payloads */
	    (void) rpmlibNeedsFeature(h, "PayloadIsBzip2", "3.0.5-1");
	}
	strcpy(buf, rpmio_flags);
	buf[s - rpmio_flags] = '\0';
	(void) headerAddEntry(h, RPMTAG_PAYLOADFLAGS, RPM_STRING_TYPE, buf+1, 1);
    }

    /* Create and add the cookie */
    if (cookie) {
	sprintf(buf, "%s %d", buildHost(), (int) time(NULL));
	*cookie = xstrdup(buf);
	(void) headerAddEntry(h, RPMTAG_COOKIE, RPM_STRING_TYPE, *cookie, 1);
    }
    
    /* Reallocate the header into one contiguous region. */
    *hdrp = h = headerReload(h, RPMTAG_HEADERIMMUTABLE);

    /*
     * Write the header+archive into a temp file so that the size of
     * archive (after compression) can be added to the header.
     */
    if (makeTempFile(NULL, &sigtarget, &fd)) {
	rpmError(RPMERR_CREATE, _("Unable to open temp file.\n"));
	return RPMERR_CREATE;
    }

    if (headerWrite(fd, h, HEADER_MAGIC_YES)) {
	rc = RPMERR_NOSPACE;
    } else { /* Write the archive and get the size */
	if (csa->cpioList != NULL) {
	    rc = cpio_doio(fd, h, csa, rpmio_flags);
	} else if (Fileno(csa->cpioFdIn) >= 0) {
	    rc = cpio_copy(fd, csa);
	} else {
	    rpmError(RPMERR_CREATE, _("Bad CSA data\n"));
	    rc = RPMERR_BADARG;
	}
    }
    rpmio_flags = _free(rpmio_flags);

    if (rc)
	goto exit;

    /*
     * Set the actual archive size, and rewrite the header.
     * This used to be done using headerModifyEntry(), but now that headers
     * have regions, the value is scribbled directly into the header data
     * area. Some new scheme for adding the final archive size will have
     * to be devised if headerGetEntry() ever changes to return a pointer
     * to memory not in the region. <shrug>
     */
    if (Fileno(csa->cpioFdIn) < 0) {
	HGE_t hge = (HGE_t)headerGetEntryMinMemory;
	int_32 * archiveSize;
	if (hge(h, RPMTAG_ARCHIVESIZE, NULL, (void *)&archiveSize, NULL))
	    *archiveSize = csa->cpioArchiveSize;
    }

    (void)Fseek(fd, 0, SEEK_SET);

    if (headerWrite(fd, h, HEADER_MAGIC_YES))
	rc = RPMERR_NOSPACE;

    (void) Fclose(fd);
    fd = NULL;
    (void) Unlink(fileName);

    if (rc)
	goto exit;

    /* Generate the signature */
    (void) fflush(stdout);
    sig = rpmNewSignature();
    (void) rpmAddSignature(sig, sigtarget, RPMSIGTAG_SIZE, passPhrase);
    (void) rpmAddSignature(sig, sigtarget, RPMSIGTAG_MD5, passPhrase);
    if ((sigtype = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY)) > 0) {
	rpmMessage(RPMMESS_NORMAL, _("Generating signature: %d\n"), sigtype);
	(void) rpmAddSignature(sig, sigtarget, sigtype, passPhrase);
    }

    /* Reallocate the signature into one contiguous region. */
    sig = headerReload(sig, RPMTAG_HEADERSIGNATURES);

    /* Open the output file */
    fd = Fopen(fileName, "w.ufdio");
    if (fd == NULL || Ferror(fd)) {
	rpmError(RPMERR_CREATE, _("Could not open %s: %s\n"),
		fileName, Fstrerror(fd));
	rc = RPMERR_CREATE;
	goto exit;
    }

    /* Write the lead section into the package. */
    {	int archnum = -1;
	int osnum = -1;
	struct rpmlead lead;

	if (Fileno(csa->cpioFdIn) < 0) {
#ifndef	DYING
	    rpmGetArchInfo(NULL, &archnum);
	    rpmGetOsInfo(NULL, &osnum);
#endif
	} else if (csa->lead != NULL) {
	    archnum = csa->lead->archnum;
	    osnum = csa->lead->osnum;
	}

	memset(&lead, 0, sizeof(lead));
	/* XXX Set package version conditioned on noDirTokens. */
	lead.major = (_noDirTokens ? 3 : 4);
	lead.minor = 0;
	lead.type = type;
	lead.archnum = archnum;
	lead.osnum = osnum;
	lead.signature_type = RPMSIGTYPE_HEADERSIG;

	{   const char *name, *version, *release;
	    (void) headerNVR(h, &name, &version, &release);
	    sprintf(buf, "%s-%s-%s", name, version, release);
	    strncpy(lead.name, buf, sizeof(lead.name));
	}

	if (writeLead(fd, &lead)) {
	    rpmError(RPMERR_NOSPACE, _("Unable to write package: %s\n"),
		 Fstrerror(fd));
	    rc = RPMERR_NOSPACE;
	    goto exit;
	}
    }

    /* Write the signature section into the package. */
    rc = rpmWriteSignature(fd, sig);
    if (rc)
	goto exit;

    /* Append the header and archive */
    ifd = Fopen(sigtarget, "r.ufdio");
    if (ifd == NULL || Ferror(ifd)) {
	rpmError(RPMERR_READ, _("Unable to open sigtarget %s: %s\n"),
		sigtarget, Fstrerror(ifd));
	rc = RPMERR_READ;
	goto exit;
    }

    /* Add signatures to header, and write header into the package. */
    {	Header nh = headerRead(ifd, HEADER_MAGIC_YES);

	if (nh == NULL) {
	    rpmError(RPMERR_READ, _("Unable to read header from %s: %s\n"),
			sigtarget, Fstrerror(ifd));
	    rc = RPMERR_READ;
	    goto exit;
	}

#ifdef	NOTYET
	(void) headerMergeLegacySigs(nh, sig);
#endif

	rc = headerWrite(fd, nh, HEADER_MAGIC_YES);
	headerFree(nh);

	if (rc) {
	    rpmError(RPMERR_NOSPACE, _("Unable to write header to %s: %s\n"),
			fileName, Fstrerror(fd));
	    rc = RPMERR_NOSPACE;
	    goto exit;
	}
    }
	
    /* Write the payload into the package. */
    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), ifd)) > 0) {
	if (count == -1) {
	    rpmError(RPMERR_READ, _("Unable to read payload from %s: %s\n"),
		     sigtarget, Fstrerror(ifd));
	    rc = RPMERR_READ;
	    goto exit;
	}
	if (Fwrite(buf, sizeof(buf[0]), count, fd) != count) {
	    rpmError(RPMERR_NOSPACE, _("Unable to write payload to %s: %s\n"),
		     fileName, Fstrerror(fd));
	    rc = RPMERR_NOSPACE;
	    goto exit;
	}
    }
    rc = 0;

exit:
    if (sig) {
	rpmFreeSignature(sig);
	sig = NULL;
    }
    if (ifd) {
	(void) Fclose(ifd);
	ifd = NULL;
    }
    if (fd) {
	(void) Fclose(fd);
	fd = NULL;
    }
    if (sigtarget) {
	(void) Unlink(sigtarget);
	sigtarget = _free(sigtarget);
    }

    if (rc == 0)
	rpmMessage(RPMMESS_NORMAL, _("Wrote: %s\n"), fileName);
    else
	(void) Unlink(fileName);

    return rc;
}

static int_32 copyTags[] = {
    RPMTAG_CHANGELOGTIME,
    RPMTAG_CHANGELOGNAME,
    RPMTAG_CHANGELOGTEXT,
    0
};

int packageBinaries(Spec spec)
{
    CSA_t csabuf, *csa = &csabuf;
    int rc;
    const char *errorString;
    Package pkg;

    for (pkg = spec->packages; pkg != NULL; pkg = pkg->next) {
	const char *fn;

	if (pkg->fileList == NULL)
	    continue;

	if ((rc = processScriptFiles(spec, pkg)))
	    return rc;
	
	if (spec->cookie) {
	    (void) headerAddEntry(pkg->header, RPMTAG_COOKIE,
			   RPM_STRING_TYPE, spec->cookie, 1);
	}

	/* Copy changelog from src rpm */
	headerCopyTags(spec->packages->header, pkg->header, copyTags);
	
	(void) headerAddEntry(pkg->header, RPMTAG_RPMVERSION,
		       RPM_STRING_TYPE, VERSION, 1);
	(void) headerAddEntry(pkg->header, RPMTAG_BUILDHOST,
		       RPM_STRING_TYPE, buildHost(), 1);
	(void) headerAddEntry(pkg->header, RPMTAG_BUILDTIME,
		       RPM_INT32_TYPE, getBuildTime(), 1);

	providePackageNVR(pkg->header);

    {	const char * optflags = rpmExpand("%{optflags}", NULL);
	(void) headerAddEntry(pkg->header, RPMTAG_OPTFLAGS, RPM_STRING_TYPE,
			optflags, 1);
	optflags = _free(optflags);
    }

	(void) genSourceRpmName(spec);
	(void) headerAddEntry(pkg->header, RPMTAG_SOURCERPM, RPM_STRING_TYPE,
		       spec->sourceRpmName, 1);
	
	{   const char *binFormat = rpmGetPath("%{_rpmfilename}", NULL);
	    char *binRpm, *binDir;
	    binRpm = headerSprintf(pkg->header, binFormat, rpmTagTable,
			       rpmHeaderFormats, &errorString);
	    binFormat = _free(binFormat);
	    if (binRpm == NULL) {
		const char *name;
		(void) headerNVR(pkg->header, &name, NULL, NULL);
		rpmError(RPMERR_BADFILENAME, _("Could not generate output "
		     "filename for package %s: %s\n"), name, errorString);
		return RPMERR_BADFILENAME;
	    }
	    fn = rpmGetPath("%{_rpmdir}/", binRpm, NULL);
	    if ((binDir = strchr(binRpm, '/')) != NULL) {
		struct stat st;
		const char *dn;
		*binDir = '\0';
		dn = rpmGetPath("%{_rpmdir}/", binRpm, NULL);
		if (Stat(dn, &st) < 0) {
		    switch(errno) {
		    case  ENOENT:
			if (Mkdir(dn, 0755) == 0)
			    break;
			/*@fallthrough@*/
		    default:
			rpmError(RPMERR_BADFILENAME,_("cannot create %s: %s\n"),
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
	csa->cpioFdIn = fdNew("init (packageBinaries)");
	csa->cpioList = pkg->cpioList;

	rc = writeRPM(&pkg->header, fn, RPMLEAD_BINARY,
		    csa, spec->passPhrase, NULL);
	csa->cpioFdIn = fdFree(csa->cpioFdIn, "init (packageBinaries)");
	fn = _free(fn);
	if (rc)
	    return rc;
    }
    
    return 0;
}

int packageSources(Spec spec)
{
    CSA_t csabuf, *csa = &csabuf;
    int rc;

    /* Add some cruft */
    (void) headerAddEntry(spec->sourceHeader, RPMTAG_RPMVERSION,
		   RPM_STRING_TYPE, VERSION, 1);
    (void) headerAddEntry(spec->sourceHeader, RPMTAG_BUILDHOST,
		   RPM_STRING_TYPE, buildHost(), 1);
    (void) headerAddEntry(spec->sourceHeader, RPMTAG_BUILDTIME,
		   RPM_INT32_TYPE, getBuildTime(), 1);

    (void) genSourceRpmName(spec);

    spec->cookie = _free(spec->cookie);
    
    /* XXX this should be %_srpmdir */
    {	const char *fn = rpmGetPath("%{_srcrpmdir}/", spec->sourceRpmName,NULL);

	memset(csa, 0, sizeof(*csa));
	csa->cpioArchiveSize = 0;
	csa->cpioFdIn = fdNew("init (packageSources)");
	csa->cpioList = spec->sourceCpioList;

	rc = writeRPM(&spec->sourceHeader, fn, RPMLEAD_SOURCE,
		csa, spec->passPhrase, &(spec->cookie));
	csa->cpioFdIn = fdFree(csa->cpioFdIn, "init (packageSources)");
	fn = _free(fn);
    }
    return rc;
}
