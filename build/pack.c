#include "system.h"

#include "rpmbuild.h"
#include "buildio.h"

#include "lib/misc.h"
#include "lib/signature.h"
#include "lib/rpmlead.h"

#define RPM_MAJOR_NUMBER 3

static int processScriptFiles(Spec spec, Package pkg);
static StringBuf addFileToTagAux(Spec spec, const char *file, StringBuf sb);
static int addFileToTag(Spec spec, char *file, Header h, int tag);
static int addFileToArrayTag(Spec spec, char *file, Header h, int tag);

static int cpio_doio(FD_t fdo, CSA_t *csa, const char * fmode);
static int cpio_copy(FD_t fdo, CSA_t *csa);

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

    {	int capability = 0;
	headerAddEntry(spec->sourceHeader, RPMTAG_CAPABILITY, RPM_INT32_TYPE,
			&capability, 1);
    }

    genSourceRpmName(spec);

    FREE(spec->cookie);
    
    /* XXX this should be %_srpmdir */
    {	const char *fn = rpmGetPath("%{_srcrpmdir}/", spec->sourceRpmName,NULL);

	memset(csa, 0, sizeof(*csa));
	csa->cpioArchiveSize = 0;
	csa->cpioFdIn = fdNew("init (packageSources)");
	csa->cpioList = spec->sourceCpioList;
	csa->cpioCount = spec->sourceCpioCount;

	rc = writeRPM(spec->sourceHeader, fn, RPMLEAD_SOURCE,
		csa, spec->passPhrase, &(spec->cookie));
	csa->cpioFdIn = fdFree(csa->cpioFdIn, "init (packageSources)");
	xfree(fn);
    }
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
    char *name;
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

    {	int capability = 0;
	headerAddEntry(pkg->header, RPMTAG_CAPABILITY, RPM_INT32_TYPE,
			&capability, 1);
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
		headerGetEntry(pkg->header, RPMTAG_NAME, NULL,
			   (void **)&name, NULL);
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

	rc = writeRPM(pkg->header, fn, RPMLEAD_BINARY,
		    csa, spec->passPhrase, NULL);
	csa->cpioFdIn = fdFree(csa->cpioFdIn, "init (packageBinaries)");
	xfree(fn);
	if (rc)
	    return rc;
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

    if (csa) {
	csa->cpioFdIn = fdi;
    } else {
	Fclose(fdi);
    }

    return 0;
}

int writeRPM(Header h, const char *fileName, int type,
		    CSA_t *csa, char *passPhrase, char **cookie)
{
    FD_t fd, ifd;
    int rc, count, sigtype;
    int archnum, osnum;
    const char *sigtarget;
    char buf[BUFSIZ];
    Header sig;
    struct rpmlead lead;
#ifdef	DYING
    int fdno;
#endif

    if (Fileno(csa->cpioFdIn) < 0) {
	csa->cpioArchiveSize = 0;
	/* Add a bogus archive size to the Header */
	headerAddEntry(h, RPMTAG_ARCHIVESIZE, RPM_INT32_TYPE,
		&csa->cpioArchiveSize, 1);
    }

    compressFilelist(h);

    /* Create and add the cookie */
    if (cookie) {
	sprintf(buf, "%s %d", buildHost(), (int) time(NULL));
	*cookie = xstrdup(buf);
	headerAddEntry(h, RPMTAG_COOKIE, RPM_STRING_TYPE, *cookie, 1);
    }
    
    /* Write the header */
    if (makeTempFile(NULL, &sigtarget, &fd)) {
	rpmError(RPMERR_CREATE, _("Unable to open temp file"));
	return RPMERR_CREATE;
    }

#ifdef	DYING
    fd = fdLink(fd, "persist");	/* XXX keep fd from being freed */
    fdno = Fileno(fd);		/* XXX HACK HACK HACK to keep fdno open */
#endif

    if (headerWrite(fd, h, HEADER_MAGIC_YES)) {
	rc = RPMERR_NOSPACE;
    } else { /* Write the archive and get the size */
	if (csa->cpioList != NULL) {
	    rc = cpio_doio(fd, csa, "w9.gzdio");
	} else if (Fileno(csa->cpioFdIn) >= 0) {
	    rc = cpio_copy(fd, csa);
	} else {
	    rpmError(RPMERR_CREATE, _("Bad CSA data"));
	    rc = RPMERR_BADARG;
	}
    }

#ifdef DYING
    fdSetFdno(fd, fdno);	/* XXX HACK HACK HACK to keep fdno open */
#endif

    if (rc != 0) {
	Fclose(fd);
	unlink(sigtarget);
	xfree(sigtarget);
	return rc;
    }

    /* Now set the real archive size in the Header */
    if (Fileno(csa->cpioFdIn) < 0) {
	headerModifyEntry(h, RPMTAG_ARCHIVESIZE,
		RPM_INT32_TYPE, &csa->cpioArchiveSize, 1);
    }

    (void)Fseek(fd, 0,  SEEK_SET);

    if (headerWrite(fd, h, HEADER_MAGIC_YES))
	rc = RPMERR_NOSPACE;

    Fclose(fd);
    unlink(fileName);

    if (rc) {
	unlink(sigtarget);
	xfree(sigtarget);
	return rc;
    }

    /* Open the output file */
    fd = Fopen(fileName, "w.ufdio");
    if (fd == NULL || Ferror(fd)) {
	rpmError(RPMERR_CREATE, _("Could not open %s: %s\n"),
		fileName, Fstrerror(fd));
	unlink(sigtarget);
	xfree(sigtarget);
	return RPMERR_CREATE;
    }

    /* Now write the lead */
    {	const char *name, *version, *release;
	headerNVR(h, &name, &version, &release);
	sprintf(buf, "%s-%s-%s", name, version, release);
    }

    if (Fileno(csa->cpioFdIn) < 0) {
	rpmGetArchInfo(NULL, &archnum);
	rpmGetOsInfo(NULL, &osnum);
    } else if (csa->lead != NULL) {	/* XXX FIXME: exorcize lead/arch/os */
	archnum = csa->lead->archnum;
	osnum = csa->lead->osnum;
    } else {
	archnum = -1;
	osnum = -1;
    }

    memset(&lead, 0, sizeof(lead));
    lead.major = RPM_MAJOR_NUMBER;
    lead.minor = 0;
    lead.type = type;
    lead.archnum = archnum;
    lead.osnum = osnum;
    lead.signature_type = RPMSIG_HEADERSIG;  /* New-style signature */
    strncpy(lead.name, buf, sizeof(lead.name));
    if (writeLead(fd, &lead)) {
	rpmError(RPMERR_NOSPACE, _("Unable to write package: %s"),
		 Fstrerror(fd));
	Fclose(fd);
	unlink(sigtarget);
	xfree(sigtarget);
	unlink(fileName);
	return rc;
    }

    /* Generate the signature */
    fflush(stdout);
    sig = rpmNewSignature();
    rpmAddSignature(sig, sigtarget, RPMSIGTAG_SIZE, passPhrase);
    rpmAddSignature(sig, sigtarget, RPMSIGTAG_MD5, passPhrase);
    if ((sigtype = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY)) > 0) {
	rpmMessage(RPMMESS_NORMAL, _("Generating signature: %d\n"), sigtype);
	rpmAddSignature(sig, sigtarget, sigtype, passPhrase);
    }
    if ((rc = rpmWriteSignature(fd, sig))) {
	Fclose(fd);
	unlink(sigtarget);
	xfree(sigtarget);
	unlink(fileName);
	rpmFreeSignature(sig);
	return rc;
    }
    rpmFreeSignature(sig);
	
    /* Append the header and archive */
    ifd = Fopen(sigtarget, "r.ufdio");
    if (ifd == NULL || Ferror(ifd)) {
	rpmError(RPMERR_READERROR, _("Unable to open sigtarget %s: %s"),
		sigtarget, Fstrerror(ifd));
	Fclose(fd);
	Unlink(sigtarget);
	xfree(sigtarget);
	Unlink(fileName);
	return RPMERR_READERROR;
    }
    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), ifd)) > 0) {
	if (count == -1) {
	    rpmError(RPMERR_READERROR, _("Unable to read sigtarget %s: %s"),
		     sigtarget, Fstrerror(ifd));
	    Fclose(ifd);
	    Fclose(fd);
	    unlink(sigtarget);
	    xfree(sigtarget);
	    unlink(fileName);
	    return RPMERR_READERROR;
	}
	if (Fwrite(buf, sizeof(buf[0]), count, fd) < 0) {
	    rpmError(RPMERR_NOSPACE, _("Unable to write package %s: %s"),
		     fileName, Fstrerror(fd));
	    Fclose(ifd);
	    Fclose(fd);
	    unlink(sigtarget);
	    xfree(sigtarget);
	    unlink(fileName);
	    return RPMERR_NOSPACE;
	}
    }
    Fclose(ifd);
    Fclose(fd);
    unlink(sigtarget);
    xfree(sigtarget);

    rpmMessage(RPMMESS_NORMAL, _("Wrote: %s\n"), fileName);

    return 0;
}

static int cpio_doio(FD_t fdo, CSA_t * csa, const char * fmode)
{
    FD_t cfd;
    int rc;
    const char *failedFile = NULL;

    (void) Fflush(fdo);
#ifndef	DYING
    cfd = Fdopen(fdDup(Fileno(fdo)), fmode);
#else
    cfd = Fdopen(fdo, fmode);
#endif
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
    while (fgets(buf, sizeof(buf), fpio->ffileno(fd))) {
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

static int addFileToTag(Spec spec, char *file, Header h, int tag)
{
    StringBuf sb;
    char *s;

    sb = newStringBuf();
    if (headerGetEntry(h, tag, NULL, (void **)&s, NULL)) {
	appendLineStringBuf(sb, s);
	headerRemoveEntry(h, tag);
    }

    if ((sb = addFileToTagAux(spec, file, sb)) == NULL) {
	return 1;
    }
    
    headerAddEntry(h, tag, RPM_STRING_TYPE, getStringBuf(sb), 1);

    freeStringBuf(sb);
    return 0;
}

static int addFileToArrayTag(Spec spec, char *file, Header h, int tag)
{
    StringBuf sb;
    char *s;

    sb = newStringBuf();
    if ((sb = addFileToTagAux(spec, file, sb)) == NULL) {
	return 1;
    }

    s = getStringBuf(sb);
    headerAddOrAppendEntry(h, tag, RPM_STRING_ARRAY_TYPE, &s, 1);

    freeStringBuf(sb);
    return 0;
}

static int processScriptFiles(Spec spec, Package pkg)
{
    struct TriggerFileEntry *p;
    char *bull = "";
    
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

    p = pkg->triggerFiles;
    while (p) {
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
	    headerAddOrAppendEntry(pkg->header, RPMTAG_TRIGGERSCRIPTS,
				   RPM_STRING_ARRAY_TYPE, &bull, 1);
	}
	
	p = p->next;
    }

    return 0;
}
