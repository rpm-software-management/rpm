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

extern int _noDirTokens;

static inline int genSourceRpmName(Spec spec)
{
    if (spec->sourceRpmName == NULL) {
	const char *name, *version, *release;
	char fileName[BUFSIZ];

	headerNVR(spec->packages->header, &name, &version, &release);
	sprintf(fileName, "%s-%s-%s.%ssrc.rpm", name, version, release,
	    spec->noSource ? "no" : "");
	spec->sourceRpmName = xstrdup(fileName);
    }

    return 0;
}

static int cpio_doio(FD_t fdo, CSA_t * csa, const char * fmodeMacro)
{
    FD_t cfd;
    int rc;
    const char *failedFile = NULL;
    const char *fmode = rpmExpand(fmodeMacro, NULL);

    if (!(fmode && fmode[0] == 'w'))
	fmode = xstrdup("w9.gzdio");
    (void) Fflush(fdo);
    cfd = Fdopen(fdDup(Fileno(fdo)), fmode);
    rc = cpioBuildArchive(cfd, csa->cpioList, csa->cpioCount, NULL, NULL,
			  &csa->cpioArchiveSize, &failedFile);
    if (rc) {
	rpmError(RPMERR_CPIO, _("create archive failed on file %s: %s"),
		failedFile, cpioStrerror(rc));
      rc = 1;
    }

    Fclose(cfd);
    if (failedFile)
	xfree(failedFile);
    xfree(fmode);

    return rc;
}

static int cpio_copy(FD_t fdo, CSA_t *csa)
{
    char buf[BUFSIZ];
    size_t nb;

    while((nb = Fread(buf, sizeof(buf[0]), sizeof(buf), csa->cpioFdIn)) > 0) {
	if (Fwrite(buf, sizeof(buf[0]), nb, fdo) != nb) {
	    rpmError(RPMERR_CPIO, _("cpio_copy write failed: %s"),
			Fstrerror(fdo));
	    return 1;
	}
	csa->cpioArchiveSize += nb;
    }
    if (Ferror(csa->cpioFdIn)) {
	rpmError(RPMERR_CPIO, _("cpio_copy read failed: %s"),
		Fstrerror(csa->cpioFdIn));
	return 1;
    }
    return 0;
}

static StringBuf addFileToTagAux(Spec spec, const char *file, StringBuf sb)
{
    char buf[BUFSIZ];
    const char *fn = buf;
    FD_t fd;

    /* XXX use rpmGenPath(rootdir, "%{_buildir}/%{_buildsubdir}/", file) */
    fn = rpmGetPath("%{_builddir}/", spec->buildSubdir, "/", file, NULL);

    fd = Fopen(fn, "r.ufdio");
    if (fn != buf) xfree(fn);
    if (fd == NULL || Ferror(fd)) {
	freeStringBuf(sb);
	return NULL;
    }
    while (fgets(buf, sizeof(buf), (FILE *)fdGetFp(fd))) {
	/* XXX display fn in error msg */
	if (expandMacros(spec, spec->macros, buf, sizeof(buf))) {
	    rpmError(RPMERR_BADSPEC, _("line: %s"), buf);
	    return NULL;
	}
	appendStringBuf(sb, buf);
    }
    Fclose(fd);

    return sb;
}

static int addFileToTag(Spec spec, const char *file, Header h, int tag)
{
    StringBuf sb = newStringBuf();
    char *s;

    if (headerGetEntry(h, tag, NULL, (void **)&s, NULL)) {
	appendLineStringBuf(sb, s);
	headerRemoveEntry(h, tag);
    }

    if ((sb = addFileToTagAux(spec, file, sb)) == NULL)
	return 1;
    
    headerAddEntry(h, tag, RPM_STRING_TYPE, getStringBuf(sb), 1);

    freeStringBuf(sb);
    return 0;
}

static int addFileToArrayTag(Spec spec, char *file, Header h, int tag)
{
    StringBuf sb = newStringBuf();
    char *s;

    if ((sb = addFileToTagAux(spec, file, sb)) == NULL)
	return 1;

    s = getStringBuf(sb);
    headerAddOrAppendEntry(h, tag, RPM_STRING_ARRAY_TYPE, &s, 1);

    freeStringBuf(sb);
    return 0;
}

static int processScriptFiles(Spec spec, Package pkg)
{
    struct TriggerFileEntry *p;
    
    if (pkg->preInFile) {
	if (addFileToTag(spec, pkg->preInFile, pkg->header, RPMTAG_PREIN)) {
	    rpmError(RPMERR_BADFILENAME,
		     _("Could not open PreIn file: %s"), pkg->preInFile);
	    return RPMERR_BADFILENAME;
	}
    }
    if (pkg->preUnFile) {
	if (addFileToTag(spec, pkg->preUnFile, pkg->header, RPMTAG_PREUN)) {
	    rpmError(RPMERR_BADFILENAME,
		     _("Could not open PreUn file: %s"), pkg->preUnFile);
	    return RPMERR_BADFILENAME;
	}
    }
    if (pkg->postInFile) {
	if (addFileToTag(spec, pkg->postInFile, pkg->header, RPMTAG_POSTIN)) {
	    rpmError(RPMERR_BADFILENAME,
		     _("Could not open PostIn file: %s"), pkg->postInFile);
	    return RPMERR_BADFILENAME;
	}
    }
    if (pkg->postUnFile) {
	if (addFileToTag(spec, pkg->postUnFile, pkg->header, RPMTAG_POSTUN)) {
	    rpmError(RPMERR_BADFILENAME,
		     _("Could not open PostUn file: %s"), pkg->postUnFile);
	    return RPMERR_BADFILENAME;
	}
    }
    if (pkg->verifyFile) {
	if (addFileToTag(spec, pkg->verifyFile, pkg->header,
			 RPMTAG_VERIFYSCRIPT)) {
	    rpmError(RPMERR_BADFILENAME,
		     _("Could not open VerifyScript file: %s"), pkg->verifyFile);
	    return RPMERR_BADFILENAME;
	}
    }

    for (p = pkg->triggerFiles; p != NULL; p = p->next) {
	headerAddOrAppendEntry(pkg->header, RPMTAG_TRIGGERSCRIPTPROG,
			       RPM_STRING_ARRAY_TYPE, &(p->prog), 1);
	if (p->script) {
	    headerAddOrAppendEntry(pkg->header, RPMTAG_TRIGGERSCRIPTS,
				   RPM_STRING_ARRAY_TYPE, &(p->script), 1);
	} else if (p->fileName) {
	    if (addFileToArrayTag(spec, p->fileName, pkg->header,
				  RPMTAG_TRIGGERSCRIPTS)) {
		rpmError(RPMERR_BADFILENAME,
			 _("Could not open Trigger script file: %s"),
			 p->fileName);
		return RPMERR_BADFILENAME;
	    }
	} else {
	    /* This is dumb.  When the header supports NULL string */
	    /* this will go away.                                  */
	    char *bull = "";
	    headerAddOrAppendEntry(pkg->header, RPMTAG_TRIGGERSCRIPTS,
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
    int rc;

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
    case 1:
	rpmError(RPMERR_BADMAGIC, _("readRPM: %s is not an RPM package\n"),
	    fileName);
	return RPMERR_BADMAGIC;
    case 0:
	break;
    default:
	rpmError(RPMERR_BADMAGIC, _("readRPM: reading header from %s\n"), fileName);
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
	Fclose(fdi);

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
	headerAddEntry(h, RPMTAG_ARCHIVESIZE, RPM_INT32_TYPE,
		&csa->cpioArchiveSize, 1);
    }

    /* Choose how filenames are represented. */
    if (_noDirTokens)
	expandFilelist(h);
    else {
	compressFilelist(h);
	/* Binary packages with dirNames cannot be installed by legacy rpm. */
	if (type == RPMLEAD_BINARY)
	    rpmlibNeedsFeature(h, "CompressedFileNames", "3.0.4-1");
    }

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
	if (rpmio_flags) xfree(rpmio_flags);
	rpmio_flags = xstrdup("w9.gzdio");
    }
    s = strchr(rpmio_flags, '.');
    if (s) {
	headerAddEntry(h, RPMTAG_PAYLOADFORMAT, RPM_STRING_TYPE, "cpio", 1);
	if (s[1] == 'g' && s[2] == 'z')
	    headerAddEntry(h, RPMTAG_PAYLOADCOMPRESSOR, RPM_STRING_TYPE,
		"gzip", 1);
	if (s[1] == 'b' && s[2] == 'z') {
	    headerAddEntry(h, RPMTAG_PAYLOADCOMPRESSOR, RPM_STRING_TYPE,
		"bzip2", 1);
	    /* Add prereq on rpm version that understands bzip2 payloads */
	    rpmlibNeedsFeature(h, "PayloadIsBzip2", "3.0.5-1");
	}
	strcpy(buf, rpmio_flags);
	buf[s - rpmio_flags] = '\0';
	headerAddEntry(h, RPMTAG_PAYLOADFLAGS, RPM_STRING_TYPE, buf+1, 1);
    }

    /* Create and add the cookie */
    if (cookie) {
	sprintf(buf, "%s %d", buildHost(), (int) time(NULL));
	*cookie = xstrdup(buf);
	headerAddEntry(h, RPMTAG_COOKIE, RPM_STRING_TYPE, *cookie, 1);
    }
    
    /* Reallocate the header into one contiguous region. */
    *hdrp = h = headerReload(h, RPMTAG_HEADERIMMUTABLE);

    /*
     * Write the header+archive into a temp file so that the size of
     * archive (after compression) can be added to the header.
     */
    if (makeTempFile(NULL, &sigtarget, &fd)) {
	rpmError(RPMERR_CREATE, _("Unable to open temp file."));
	return RPMERR_CREATE;
    }

    if (headerWrite(fd, h, HEADER_MAGIC_YES)) {
	rc = RPMERR_NOSPACE;
    } else { /* Write the archive and get the size */
	if (csa->cpioList != NULL) {
	    rc = cpio_doio(fd, csa, rpmio_flags);
	} else if (Fileno(csa->cpioFdIn) >= 0) {
	    rc = cpio_copy(fd, csa);
	} else {
	    rpmError(RPMERR_CREATE, _("Bad CSA data"));
	    rc = RPMERR_BADARG;
	}
    }
    if (rpmio_flags) xfree(rpmio_flags);

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
	int_32 * archiveSize;
	if (headerGetEntry(h, RPMTAG_ARCHIVESIZE, NULL, (void *)&archiveSize, NULL))
	    *archiveSize = csa->cpioArchiveSize;
    }

    (void)Fseek(fd, 0, SEEK_SET);

    if (headerWrite(fd, h, HEADER_MAGIC_YES))
	rc = RPMERR_NOSPACE;

    Fclose(fd);
    fd = NULL;
    Unlink(fileName);

    if (rc)
	goto exit;

    /* Generate the signature */
    fflush(stdout);
    sig = rpmNewSignature();
    rpmAddSignature(sig, sigtarget, RPMSIGTAG_SIZE, passPhrase);
    rpmAddSignature(sig, sigtarget, RPMSIGTAG_MD5, passPhrase);
    if ((sigtype = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY)) > 0) {
	rpmMessage(RPMMESS_NORMAL, _("Generating signature: %d\n"), sigtype);
	rpmAddSignature(sig, sigtarget, sigtype, passPhrase);
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
	lead.signature_type = RPMSIG_HEADERSIG;  /* New-style signature */

	{	    const char *name, *version, *release;
	    headerNVR(h, &name, &version, &release);
	    sprintf(buf, "%s-%s-%s", name, version, release);
	    strncpy(lead.name, buf, sizeof(lead.name));
	}

	if (writeLead(fd, &lead)) {
	    rpmError(RPMERR_NOSPACE, _("Unable to write package: %s"),
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
	rpmError(RPMERR_READ, _("Unable to open sigtarget %s: %s"),
		sigtarget, Fstrerror(ifd));
	rc = RPMERR_READ;
	goto exit;
    }

    /* Add signatures to header, and write header into the package. */
    {	Header nh = headerRead(ifd, HEADER_MAGIC_YES);

	if (nh == NULL) {
	    rpmError(RPMERR_READ, _("Unable to read header from %s: %s"),
			sigtarget, Fstrerror(ifd));
	    rc = RPMERR_READ;
	    goto exit;
	}

#ifdef	NOTYET
	headerMergeLegacySigs(nh, sig);
#endif

	rc = headerWrite(fd, nh, HEADER_MAGIC_YES);
	headerFree(nh);

	if (rc) {
	    rpmError(RPMERR_NOSPACE, _("Unable to write header to %s: %s"),
			fileName, Fstrerror(fd));
	    rc = RPMERR_NOSPACE;
	    goto exit;
	}
    }
	
    /* Write the payload into the package. */
    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), ifd)) > 0) {
	if (count == -1) {
	    rpmError(RPMERR_READ, _("Unable to read payload from %s: %s"),
		     sigtarget, Fstrerror(ifd));
	    rc = RPMERR_READ;
	    goto exit;
	}
	if (Fwrite(buf, sizeof(buf[0]), count, fd) < 0) {
	    rpmError(RPMERR_NOSPACE, _("Unable to write payload to %s: %s"),
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
	Fclose(ifd);
	ifd = NULL;
    }
    if (fd) {
	Fclose(fd);
	fd = NULL;
    }
    if (sigtarget) {
	Unlink(sigtarget);
	xfree(sigtarget);
    }

    if (rc == 0)
	rpmMessage(RPMMESS_NORMAL, _("Wrote: %s\n"), fileName);
    else
	Unlink(fileName);

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
	    headerAddEntry(pkg->header, RPMTAG_COOKIE,
			   RPM_STRING_TYPE, spec->cookie, 1);
	}

	/* Copy changelog from src rpm */
	headerCopyTags(spec->packages->header, pkg->header, copyTags);
	
	headerAddEntry(pkg->header, RPMTAG_RPMVERSION,
		       RPM_STRING_TYPE, VERSION, 1);
	headerAddEntry(pkg->header, RPMTAG_BUILDHOST,
		       RPM_STRING_TYPE, buildHost(), 1);
	headerAddEntry(pkg->header, RPMTAG_BUILDTIME,
		       RPM_INT32_TYPE, getBuildTime(), 1);

	providePackageNVR(pkg->header);

    {	const char * optflags = rpmExpand("%{optflags}", NULL);
	headerAddEntry(pkg->header, RPMTAG_OPTFLAGS, RPM_STRING_TYPE,
			optflags, 1);
	xfree(optflags);
    }

	genSourceRpmName(spec);
	headerAddEntry(pkg->header, RPMTAG_SOURCERPM, RPM_STRING_TYPE,
		       spec->sourceRpmName, 1);
	
	{   const char *binFormat = rpmGetPath("%{_rpmfilename}", NULL);
	    char *binRpm, *binDir;
	    binRpm = headerSprintf(pkg->header, binFormat, rpmTagTable,
			       rpmHeaderFormats, &errorString);
	    xfree(binFormat);
	    if (binRpm == NULL) {
		const char *name;
		headerNVR(pkg->header, &name, NULL, NULL);
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
		xfree(dn);
	    }
	    xfree(binRpm);
	}

	memset(csa, 0, sizeof(*csa));
	csa->cpioArchiveSize = 0;
	csa->cpioFdIn = fdNew("init (packageBinaries)");
	csa->cpioList = pkg->cpioList;
	csa->cpioCount = pkg->cpioCount;

	rc = writeRPM(&pkg->header, fn, RPMLEAD_BINARY,
		    csa, spec->passPhrase, NULL);
	csa->cpioFdIn = fdFree(csa->cpioFdIn, "init (packageBinaries)");
	xfree(fn);
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
    headerAddEntry(spec->sourceHeader, RPMTAG_RPMVERSION,
		   RPM_STRING_TYPE, VERSION, 1);
    headerAddEntry(spec->sourceHeader, RPMTAG_BUILDHOST,
		   RPM_STRING_TYPE, buildHost(), 1);
    headerAddEntry(spec->sourceHeader, RPMTAG_BUILDTIME,
		   RPM_INT32_TYPE, getBuildTime(), 1);

    genSourceRpmName(spec);

    FREE(spec->cookie);
    
    /* XXX this should be %_srpmdir */
    {	const char *fn = rpmGetPath("%{_srcrpmdir}/", spec->sourceRpmName,NULL);

	memset(csa, 0, sizeof(*csa));
	csa->cpioArchiveSize = 0;
	csa->cpioFdIn = fdNew("init (packageSources)");
	csa->cpioList = spec->sourceCpioList;
	csa->cpioCount = spec->sourceCpioCount;

	rc = writeRPM(&spec->sourceHeader, fn, RPMLEAD_SOURCE,
		csa, spec->passPhrase, &(spec->cookie));
	csa->cpioFdIn = fdFree(csa->cpioFdIn, "init (packageSources)");
	xfree(fn);
    }
    return rc;
}
