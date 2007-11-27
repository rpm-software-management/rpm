/** \ingroup rpmbuild
 * \file build/pack.c
 *  Assemble components of an RPM package.
 */

#include "system.h"

#include "rpmio/rpmio_internal.h"	/* fdInitDigest, fdFiniDigest */
#include <rpmbuild.h>

#include <rpmps.h>

#include "lib/cpio.h"
#include "lib/fsm.h"
#include "lib/psm.h"

#define	_RPMFI_INTERNAL		/* XXX fi->fsm */
#include <rpmfi.h>
#include <rpmts.h>

#include "build/buildio.h"

#include "lib/legacy.h"	/* XXX providePackageNVR */
#include "lib/signature.h"
#include "lib/rpmlead.h"
#include <rpmfileutil.h>
#include <rpmlog.h>
#include "debug.h"

/**
 */
static inline int genSourceRpmName(rpmSpec spec)
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
static int cpio_doio(FD_t fdo, Header h, CSA_t csa,
		const char * fmodeMacro)
{
    rpmts ts = rpmtsCreate();
    rpmfi fi = csa->cpioList;
    const char *failedFile = NULL;
    FD_t cfd;
    int rc, ec;

    {	const char *fmode = rpmExpand(fmodeMacro, NULL);
	if (!(fmode && fmode[0] == 'w'))
	    fmode = xstrdup("w9.gzdio");
	(void) Fflush(fdo);
	cfd = Fdopen(fdDup(Fileno(fdo)), fmode);
	fmode = _free(fmode);
    }
    if (cfd == NULL)
	return 1;

    rc = fsmSetup(fi->fsm, FSM_PKGBUILD, ts, fi, cfd,
		&csa->cpioArchiveSize, &failedFile);
    (void) Fclose(cfd);
    ec = fsmTeardown(fi->fsm);
    if (!rc) rc = ec;

    if (rc) {
	if (failedFile)
	    rpmlog(RPMLOG_ERR, _("create archive failed on file %s: %s\n"),
		failedFile, cpioStrerror(rc));
	else
	    rpmlog(RPMLOG_ERR, _("create archive failed: %s\n"),
		cpioStrerror(rc));
      rc = 1;
    }

    failedFile = _free(failedFile);
    ts = rpmtsFree(ts);

    return rc;
}

/**
 */
static int cpio_copy(FD_t fdo, CSA_t csa)
{
    char buf[BUFSIZ];
    size_t nb;

    while((nb = Fread(buf, sizeof(buf[0]), sizeof(buf), csa->cpioFdIn)) > 0) {
	if (Fwrite(buf, sizeof(buf[0]), nb, fdo) != nb) {
	    rpmlog(RPMLOG_ERR, _("cpio_copy write failed: %s\n"),
			Fstrerror(fdo));
	    return 1;
	}
	csa->cpioArchiveSize += nb;
    }
    if (Ferror(csa->cpioFdIn)) {
	rpmlog(RPMLOG_ERR, _("cpio_copy read failed: %s\n"),
		Fstrerror(csa->cpioFdIn));
	return 1;
    }
    return 0;
}

/**
 */
static StringBuf addFileToTagAux(rpmSpec spec,
		const char * file, StringBuf sb)
{
    char buf[BUFSIZ];
    const char * fn = buf;
    FILE * f;
    FD_t fd;

    fn = rpmGetPath("%{_builddir}/%{?buildsubdir:%{buildsubdir}/}", file, NULL);

    fd = Fopen(fn, "r.ufdio");
    if (fn != buf) fn = _free(fn);
    if (fd == NULL || Ferror(fd)) {
	sb = freeStringBuf(sb);
	return NULL;
    }
    if ((f = fdGetFILE(fd)) != NULL)
    while (fgets(buf, sizeof(buf), f)) {
	/* XXX display fn in error msg */
	if (expandMacros(spec, spec->macros, buf, sizeof(buf))) {
	    rpmlog(RPMLOG_ERR, _("line: %s\n"), buf);
	    sb = freeStringBuf(sb);
	    break;
	}
	appendStringBuf(sb, buf);
    }
    (void) Fclose(fd);

    return sb;
}

/**
 */
static int addFileToTag(rpmSpec spec, const char * file, Header h, int tag)
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

    sb = freeStringBuf(sb);
    return 0;
}

/**
 */
static int addFileToArrayTag(rpmSpec spec, const char *file, Header h, int tag)
{
    StringBuf sb = newStringBuf();
    char *s;

    if ((sb = addFileToTagAux(spec, file, sb)) == NULL)
	return 1;

    s = getStringBuf(sb);
    (void) headerAddOrAppendEntry(h, tag, RPM_STRING_ARRAY_TYPE, &s, 1);

    sb = freeStringBuf(sb);
    return 0;
}

/**
 */
static int processScriptFiles(rpmSpec spec, Package pkg)
{
    struct TriggerFileEntry *p;
    
    if (pkg->preInFile) {
	if (addFileToTag(spec, pkg->preInFile, pkg->header, RPMTAG_PREIN)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PreIn file: %s\n"), pkg->preInFile);
	    return RPMLOG_ERR;
	}
    }
    if (pkg->preUnFile) {
	if (addFileToTag(spec, pkg->preUnFile, pkg->header, RPMTAG_PREUN)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PreUn file: %s\n"), pkg->preUnFile);
	    return RPMLOG_ERR;
	}
    }
    if (pkg->preTransFile) {
	if (addFileToTag(spec, pkg->preTransFile, pkg->header, RPMTAG_PRETRANS)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PreIn file: %s\n"), pkg->preTransFile);
	    return RPMLOG_ERR;
	}
    }
    if (pkg->postInFile) {
	if (addFileToTag(spec, pkg->postInFile, pkg->header, RPMTAG_POSTIN)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PostIn file: %s\n"), pkg->postInFile);
	    return RPMLOG_ERR;
	}
    }
    if (pkg->postUnFile) {
	if (addFileToTag(spec, pkg->postUnFile, pkg->header, RPMTAG_POSTUN)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PostUn file: %s\n"), pkg->postUnFile);
	    return RPMLOG_ERR;
	}
    }
    if (pkg->postTransFile) {
	if (addFileToTag(spec, pkg->postTransFile, pkg->header, RPMTAG_POSTTRANS)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open PostUn file: %s\n"), pkg->postTransFile);
	    return RPMLOG_ERR;
	}
    }
    if (pkg->verifyFile) {
	if (addFileToTag(spec, pkg->verifyFile, pkg->header,
			 RPMTAG_VERIFYSCRIPT)) {
	    rpmlog(RPMLOG_ERR,
		     _("Could not open VerifyScript file: %s\n"), pkg->verifyFile);
	    return RPMLOG_ERR;
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
		rpmlog(RPMLOG_ERR,
			 _("Could not open Trigger script file: %s\n"),
			 p->fileName);
		return RPMLOG_ERR;
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

int readRPM(const char *fileName, rpmSpec *specp, struct rpmlead *lead,
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
	return RPMLOG_ERR;
    }

    /* Get copy of lead */
    if ((rc = Fread(lead, sizeof(char), sizeof(*lead), fdi)) != sizeof(*lead)) {
	rpmlog(RPMLOG_ERR, _("readRPM: read %s: %s\n"),
		(fileName ? fileName : "<stdin>"),
		Fstrerror(fdi));
	return RPMLOG_ERR;
    }

    /* XXX FIXME: EPIPE on <stdin> */
    if (Fseek(fdi, 0, SEEK_SET) == -1) {
	rpmlog(RPMLOG_ERR, _("%s: Fseek failed: %s\n"),
			(fileName ? fileName : "<stdin>"), Fstrerror(fdi));
	return RPMLOG_ERR;
    }

    /* Reallocate build data structures */
    spec = newSpec();
    spec->packages = newPackage(spec);

    /* XXX the header just allocated will be allocated again */
    spec->packages->header = headerFree(spec->packages->header);

    /* Read the rpm lead, signatures, and header */
    {	rpmts ts = rpmtsCreate();

	/* XXX W2DO? pass fileName? */
	     /* LCL: segfault */
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
	return RPMLOG_ERR;
    case RPMRC_FAIL:
    default:
	rpmlog(RPMLOG_ERR, _("readRPM: reading header from %s\n"),
		(fileName ? fileName : "<stdin>"));
	return RPMLOG_ERR;
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

    return 0;
}

#ifdef	DYING
static unsigned char header_magic[8] = {
        0x8e, 0xad, 0xe8, 0x01, 0x00, 0x00, 0x00, 0x00
};
#endif

#define	RPMPKGVERSION_MIN	30004
#define	RPMPKGVERSION_MAX	40003
static int rpmpkg_version = -1;

static int rpmLeadVersion(void)
{
    int rpmlead_version;

    /* Intitialize packaging version from macro configuration. */
    if (rpmpkg_version < 0) {
	rpmpkg_version = rpmExpandNumeric("%{_package_version}");
	if (rpmpkg_version < RPMPKGVERSION_MIN)
	    rpmpkg_version = RPMPKGVERSION_MIN;
	if (rpmpkg_version > RPMPKGVERSION_MAX)
	    rpmpkg_version = RPMPKGVERSION_MAX;
    }

    rpmlead_version = rpmpkg_version / 10000;
    if (_noDirTokens || (rpmlead_version < 3 || rpmlead_version > 4))
	rpmlead_version = 3;
    return rpmlead_version;
}

int writeRPM(Header *hdrp, unsigned char ** pkgidp, const char *fileName,
		int type, CSA_t csa, char *passPhrase, const char **cookie)
{
    FD_t fd = NULL;
    FD_t ifd = NULL;
    int32_t count, sigtag;
    const char * sigtarget = NULL;;
    const char * rpmio_flags = NULL;
    const char * SHA1 = NULL;
    char *s;
    char buf[BUFSIZ];
    Header h;
    Header sig = NULL;
    int rc = 0;

    /* Transfer header reference form *hdrp to h. */
    h = headerLink(*hdrp);
    *hdrp = headerFree(*hdrp);

    if (pkgidp)
	*pkgidp = NULL;

    /* Binary packages now have explicit Provides: name = version-release. */
    if (type == RPMLEAD_BINARY)
	providePackageNVR(h);

    /* Save payload information */
    switch(type) {
    case RPMLEAD_SOURCE:
	rpmio_flags = rpmExpand("%{?_source_payload}", NULL);
	break;
    case RPMLEAD_BINARY:
	rpmio_flags = rpmExpand("%{?_binary_payload}", NULL);
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
	sprintf(buf, "%s %d", buildHost(), (int) (*getBuildTime()));
	*cookie = xstrdup(buf);
	(void) headerAddEntry(h, RPMTAG_COOKIE, RPM_STRING_TYPE, *cookie, 1);
    }
    
    /* Reallocate the header into one contiguous region. */
    h = headerReload(h, RPMTAG_HEADERIMMUTABLE);
    if (h == NULL) {	/* XXX can't happen */
	rc = RPMLOG_ERR;
	rpmlog(RPMLOG_ERR, _("Unable to create immutable header region.\n"));
	goto exit;
    }
    /* Re-reference reallocated header. */
    *hdrp = headerLink(h);

    /*
     * Write the header+archive into a temp file so that the size of
     * archive (after compression) can be added to the header.
     */
    if (rpmMkTempFile(NULL, &sigtarget, &fd)) {
	rc = RPMLOG_ERR;
	rpmlog(RPMLOG_ERR, _("Unable to open temp file.\n"));
	goto exit;
    }

    fdInitDigest(fd, PGPHASHALGO_SHA1, 0);
    if (headerWrite(fd, h, HEADER_MAGIC_YES)) {
	rc = RPMLOG_ERR;
	rpmlog(RPMLOG_ERR, _("Unable to write temp header\n"));
    } else { /* Write the archive and get the size */
	(void) Fflush(fd);
	fdFiniDigest(fd, PGPHASHALGO_SHA1, (void **)&SHA1, NULL, 1);
	if (csa->cpioList != NULL) {
	    rc = cpio_doio(fd, h, csa, rpmio_flags);
	} else if (Fileno(csa->cpioFdIn) >= 0) {
	    rc = cpio_copy(fd, csa);
	} else {
	    rc = RPMLOG_ERR;
	    rpmlog(RPMLOG_ERR, _("Bad CSA data\n"));
	}
    }
    rpmio_flags = _free(rpmio_flags);

    if (rc)
	goto exit;

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

    if ((sigtag = rpmLookupSignatureType(RPMLOOKUPSIG_QUERY)) > 0) {
	rpmlog(RPMLOG_NOTICE, _("Generating signature: %d\n"), sigtag);
	(void) rpmAddSignature(sig, sigtarget, sigtag, passPhrase);
    }
    
    if (SHA1) {
	(void) headerAddEntry(sig, RPMSIGTAG_SHA1, RPM_STRING_TYPE, SHA1, 1);
	SHA1 = _free(SHA1);
    }

    {	int32_t payloadSize = csa->cpioArchiveSize;
	(void) headerAddEntry(sig, RPMSIGTAG_PAYLOADSIZE, RPM_INT32_TYPE,
			&payloadSize, 1);
    }

    /* Reallocate the signature into one contiguous region. */
    sig = headerReload(sig, RPMTAG_HEADERSIGNATURES);
    if (sig == NULL) {	/* XXX can't happen */
	rc = RPMLOG_ERR;
	rpmlog(RPMLOG_ERR, _("Unable to reload signature header.\n"));
	goto exit;
    }

    /* Open the output file */
    fd = Fopen(fileName, "w.ufdio");
    if (fd == NULL || Ferror(fd)) {
	rc = RPMLOG_ERR;
	rpmlog(RPMLOG_ERR, _("Could not open %s: %s\n"),
		fileName, Fstrerror(fd));
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
	lead.major = rpmLeadVersion();
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

	if (writeLead(fd, &lead) != RPMRC_OK) {
	    rc = RPMLOG_ERR;
	    rpmlog(RPMLOG_ERR, _("Unable to write package: %s\n"),
		 Fstrerror(fd));
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
	rc = RPMLOG_ERR;
	rpmlog(RPMLOG_ERR, _("Unable to open sigtarget %s: %s\n"),
		sigtarget, Fstrerror(ifd));
	goto exit;
    }

    /* Add signatures to header, and write header into the package. */
    /* XXX header+payload digests/signatures might be checked again here. */
    {	Header nh = headerRead(ifd, HEADER_MAGIC_YES);

	if (nh == NULL) {
	    rc = RPMLOG_ERR;
	    rpmlog(RPMLOG_ERR, _("Unable to read header from %s: %s\n"),
			sigtarget, Fstrerror(ifd));
	    goto exit;
	}

#ifdef	NOTYET
	(void) headerMergeLegacySigs(nh, sig);
#endif

	rc = headerWrite(fd, nh, HEADER_MAGIC_YES);
	nh = headerFree(nh);

	if (rc) {
	    rc = RPMLOG_ERR;
	    rpmlog(RPMLOG_ERR, _("Unable to write header to %s: %s\n"),
			fileName, Fstrerror(fd));
	    goto exit;
	}
    }
	
    /* Write the payload into the package. */
    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), ifd)) > 0) {
	if (count == -1) {
	    rc = RPMLOG_ERR;
	    rpmlog(RPMLOG_ERR, _("Unable to read payload from %s: %s\n"),
		     sigtarget, Fstrerror(ifd));
	    goto exit;
	}
	if (Fwrite(buf, sizeof(buf[0]), count, fd) != count) {
	    rc = RPMLOG_ERR;
	    rpmlog(RPMLOG_ERR, _("Unable to write payload to %s: %s\n"),
		     fileName, Fstrerror(fd));
	    goto exit;
	}
    }
    rc = 0;

exit:
    SHA1 = _free(SHA1);
    h = headerFree(h);

    /* XXX Fish the pkgid out of the signature header. */
    if (sig != NULL && pkgidp != NULL) {
	int32_t tagType;
	unsigned char * MD5 = NULL;
	int32_t c;
	int xx;
	xx = headerGetEntry(sig, RPMSIGTAG_MD5, &tagType, (void **)&MD5, &c);
	if (tagType == RPM_BIN_TYPE && MD5 != NULL && c == 16)
	    *pkgidp = MD5;
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
	(void) Unlink(sigtarget);
	sigtarget = _free(sigtarget);
    }

    if (rc == 0)
	rpmlog(RPMLOG_NOTICE, _("Wrote: %s\n"), fileName);
    else
	(void) Unlink(fileName);

    return rc;
}

static int32_t copyTags[] = {
    RPMTAG_CHANGELOGTIME,
    RPMTAG_CHANGELOGNAME,
    RPMTAG_CHANGELOGTEXT,
    0
};

int packageBinaries(rpmSpec spec)
{
    struct cpioSourceArchive_s csabuf;
    CSA_t csa = &csabuf;
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
	if (spec->sourcePkgId != NULL) {
	(void) headerAddEntry(pkg->header, RPMTAG_SOURCEPKGID, RPM_BIN_TYPE,
		       spec->sourcePkgId, 16);
	}
	
	{   const char *binFormat = rpmGetPath("%{_rpmfilename}", NULL);
	    char *binRpm, *binDir;
	    binRpm = headerSprintf(pkg->header, binFormat, rpmTagTable,
			       rpmHeaderFormats, &errorString);
	    binFormat = _free(binFormat);
	    if (binRpm == NULL) {
		const char *name;
		(void) headerNVR(pkg->header, &name, NULL, NULL);
		rpmlog(RPMLOG_ERR, _("Could not generate output "
		     "filename for package %s: %s\n"), name, errorString);
		return RPMLOG_ERR;
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
	/* LCL: function typedefs */
	csa->cpioFdIn = fdNew("init (packageBinaries)");
	csa->cpioList = rpmfiLink(pkg->cpioList, "packageBinaries");

	rc = writeRPM(&pkg->header, NULL, fn, RPMLEAD_BINARY,
		    csa, spec->passPhrase, NULL);

	csa->cpioList = rpmfiFree(csa->cpioList);
	csa->cpioFdIn = fdFree(csa->cpioFdIn, "init (packageBinaries)");
	fn = _free(fn);
	if (rc)
	    return rc;
    }
    
    return 0;
}

int packageSources(rpmSpec spec)
{
    struct cpioSourceArchive_s csabuf;
    CSA_t csa = &csabuf;
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
	/* LCL: function typedefs */
	csa->cpioFdIn = fdNew("init (packageSources)");
	csa->cpioList = rpmfiLink(spec->sourceCpioList, "packageSources");

	spec->sourcePkgId = NULL;
	rc = writeRPM(&spec->sourceHeader, &spec->sourcePkgId, fn, RPMLEAD_SOURCE,
		csa, spec->passPhrase, &(spec->cookie));

	csa->cpioList = rpmfiFree(csa->cpioList);
	csa->cpioFdIn = fdFree(csa->cpioFdIn, "init (packageSources)");
	fn = _free(fn);
    }
    return rc;
}
