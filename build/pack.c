#include "system.h"

#include <signal.h>

#include "rpmbuild.h"
#include "buildio.h"

#include "lib/signature.h"
#include "lib/rpmlead.h"

#define RPM_MAJOR_NUMBER 3

static int processScriptFiles(Spec spec, Package pkg);
static StringBuf addFileToTagAux(Spec spec, char *file, StringBuf sb);
static int addFileToTag(Spec spec, char *file, Header h, int tag);
static int addFileToArrayTag(Spec spec, char *file, Header h, int tag);

static int cpio_gzip(int fdo, CSA_t *csa);
static int cpio_copy(int fdo, CSA_t *csa);

static int genSourceRpmName(Spec spec)
{
    char *name, *version, *release;
    char fileName[BUFSIZ];

    if (spec->sourceRpmName) {
	return 0;
    }
    
    headerGetEntry(spec->packages->header, RPMTAG_NAME,
		   NULL, (void **)&name, NULL);
    headerGetEntry(spec->packages->header, RPMTAG_VERSION,
		   NULL, (void **)&version, NULL);
    headerGetEntry(spec->packages->header, RPMTAG_RELEASE,
		   NULL, (void **)&release, NULL);
    sprintf(fileName, "%s-%s-%s.%ssrc.rpm", name, version, release,
	    spec->noSource ? "no" : "");
    spec->sourceRpmName = strdup(fileName);

    return 0;
}

int packageSources(Spec spec)
{
    CSA_t csabuf, *csa = &csabuf;
    char fileName[BUFSIZ];
    HeaderIterator iter;
    int_32 tag, count;
    char **ptr;

    /* Add some cruft */
    headerAddEntry(spec->sourceHeader, RPMTAG_RPMVERSION,
		   RPM_STRING_TYPE, VERSION, 1);
    headerAddEntry(spec->sourceHeader, RPMTAG_BUILDHOST,
		   RPM_STRING_TYPE, buildHost(), 1);
    headerAddEntry(spec->sourceHeader, RPMTAG_BUILDTIME,
		   RPM_INT32_TYPE, getBuildTime(), 1);

    genSourceRpmName(spec);
    sprintf(fileName, "%s/%s", rpmGetVar(RPMVAR_SRPMDIR), spec->sourceRpmName);

    /* Add the build restrictions */
    iter = headerInitIterator(spec->buildRestrictions);
    while (headerNextIterator(iter, &tag, NULL, (void **)&ptr, &count)) {
	headerAddEntry(spec->sourceHeader, tag,
		       RPM_STRING_ARRAY_TYPE, ptr, count);
	FREE(ptr);
    }
    headerFreeIterator(iter);
    if (spec->buildArchitectureCount) {
	headerAddEntry(spec->sourceHeader, RPMTAG_BUILDARCHS,
		       RPM_STRING_ARRAY_TYPE,
		       spec->buildArchitectures, spec->buildArchitectureCount);
    }

    FREE(spec->cookie);
    
    memset(csa, 0, sizeof(*csa));
    csa->cpioArchiveSize = 0;
    csa->cpioFdIn = -1;
    csa->cpioList = spec->sourceCpioList;
    csa->cpioCount = spec->sourceCpioCount;

    return writeRPM(spec->sourceHeader, fileName, RPMLEAD_SOURCE,
		    csa, spec->passPhrase, &(spec->cookie));
}

static int copyTags[] = {
    RPMTAG_CHANGELOGTIME,
    RPMTAG_CHANGELOGNAME,
    RPMTAG_CHANGELOGTEXT,
    0
};

int packageBinaries(Spec spec)
{
    CSA_t csabuf, *csa = &csabuf;
    int rc;
    char *binFormat, *binRpm, *errorString;
    char *name, fileName[BUFSIZ];
    Package pkg;

    pkg = spec->packages;
    while (pkg) {
	if (!pkg->fileList) {
	    pkg = pkg->next;
	    continue;
	}

	if ((rc = processScriptFiles(spec, pkg))) {
	    return rc;
	}
	
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

	genSourceRpmName(spec);
	headerAddEntry(pkg->header, RPMTAG_SOURCERPM, RPM_STRING_TYPE,
		       spec->sourceRpmName, 1);
	
	binFormat = rpmGetVar(RPMVAR_RPMFILENAME);
	binRpm = headerSprintf(pkg->header, binFormat, rpmTagTable,
			       rpmHeaderFormats, &errorString);
	if (!binRpm) {
	    headerGetEntry(pkg->header, RPMTAG_NAME, NULL,
			   (void **)&name, NULL);
	    rpmError(RPMERR_BADFILENAME, "Could not generate output "
		     "filename for package %s: %s\n", name, errorString);
	    return RPMERR_BADFILENAME;
	}
	sprintf(fileName, "%s/%s", rpmGetVar(RPMVAR_RPMDIR), binRpm);
	FREE(binRpm);

	memset(csa, 0, sizeof(*csa));
	csa->cpioArchiveSize = 0;
	csa->cpioFdIn = -1;
	csa->cpioList = pkg->cpioList;
	csa->cpioCount = pkg->cpioCount;

	if ((rc = writeRPM(pkg->header, fileName, RPMLEAD_BINARY,
			    csa, spec->passPhrase, NULL))) {
	    return rc;
	}
	
	pkg = pkg->next;
    }
    
    return 0;
}

int writeRPM(Header header, char *fileName, int type,
		    CSA_t *csa, char *passPhrase, char **cookie)
{
    int fd, ifd, rc, count, arch, os, sigtype;
    char *sigtarget, *name, *version, *release;
    char buf[BUFSIZ];
    Header sig;
    struct rpmlead lead;

    if (csa->cpioFdIn < 0) {
	csa->cpioArchiveSize = 0;
	/* Add a bogus archive size to the Header */
	headerAddEntry(header, RPMTAG_ARCHIVESIZE, RPM_INT32_TYPE,
		&csa->cpioArchiveSize, 1);
    }

    /* Create and add the cookie */
    if (cookie) {
	sprintf(buf, "%s %d", buildHost(), (int) time(NULL));
	*cookie = strdup(buf);
	headerAddEntry(header, RPMTAG_COOKIE, RPM_STRING_TYPE, *cookie, 1);
    }
    
    /* Write the header */
    if (makeTempFile(NULL, &sigtarget, &fd)) {
	rpmError(RPMERR_CREATE, "Unable to open temp file");
	return RPMERR_CREATE;
    }
    headerWrite(fd, header, HEADER_MAGIC_YES);
	     
    /* Write the archive and get the size */
    if (csa->cpioList != NULL) {
	rc = cpio_gzip(fd, csa);
    } else if (csa->cpioFdIn >= 0) {
	rc = cpio_copy(fd, csa);
    } else {
	rpmError(RPMERR_CREATE, "Bad CSA data");
	rc = RPMERR_BADARG;
    }
    if (rc != 0) {
	close(fd);
	unlink(sigtarget);
	free(sigtarget);
	return rc;
    }

    /* Now set the real archive size in the Header */
    if (csa->cpioFdIn < 0) {
	headerModifyEntry(header, RPMTAG_ARCHIVESIZE,
		RPM_INT32_TYPE, &csa->cpioArchiveSize, 1);
    }
    lseek(fd, 0,  SEEK_SET);
    headerWrite(fd, header, HEADER_MAGIC_YES);

    close(fd);

    /* Open the output file */
    unlink(fileName);
    if ((fd = open(fileName, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1) {
	rpmError(RPMERR_CREATE, "Could not open %s\n", fileName);
	unlink(sigtarget);
	free(sigtarget);
	return RPMERR_CREATE;
    }

    /* Now write the lead */
    headerGetEntry(header, RPMTAG_NAME, NULL, (void **)&name, NULL);
    headerGetEntry(header, RPMTAG_VERSION, NULL, (void **)&version, NULL);
    headerGetEntry(header, RPMTAG_RELEASE, NULL, (void **)&release, NULL);
    sprintf(buf, "%s-%s-%s", name, version, release);

    if (csa->cpioFdIn < 0) {
	rpmGetArchInfo(NULL, &arch);
	rpmGetOsInfo(NULL, &os);
    } else if (csa->lead != NULL) {	/* XXX FIXME: exorcize lead/arch/os */
	arch = csa->lead->archnum;
	os = csa->lead->osnum;
    } else {
	arch = -1;
	os = -1;
    }

    memset(&lead, 0, sizeof(lead));
    lead.major = RPM_MAJOR_NUMBER;
    lead.minor = 0;
    lead.type = type;
    lead.archnum = arch;
    lead.osnum = os;
    lead.signature_type = RPMSIG_HEADERSIG;  /* New-style signature */
    strncpy(lead.name, buf, sizeof(lead.name));
    if (writeLead(fd, &lead)) {
	rpmError(RPMERR_NOSPACE, "Unable to write package: %s",
		 strerror(errno));
	close(fd);
	unlink(sigtarget);
	free(sigtarget);
	unlink(fileName);
	return rc;
    }

    /* Generate the signature */
    sigtype = rpmLookupSignatureType();
    fflush(stdout);
    sig = rpmNewSignature();
    rpmAddSignature(sig, sigtarget, RPMSIGTAG_SIZE, passPhrase);
    rpmAddSignature(sig, sigtarget, RPMSIGTAG_MD5, passPhrase);
    if (sigtype > 0) {
	rpmMessage(RPMMESS_NORMAL, "Generating signature: %d\n", sigtype);
	rpmAddSignature(sig, sigtarget, sigtype, passPhrase);
    }
    if ((rc = rpmWriteSignature(fd, sig))) {
	close(fd);
	unlink(sigtarget);
	free(sigtarget);
	unlink(fileName);
	rpmFreeSignature(sig);
	return rc;
    }
    rpmFreeSignature(sig);
	
    /* Append the header and archive */
    ifd = open(sigtarget, O_RDONLY);
    while ((count = read(ifd, buf, sizeof(buf))) > 0) {
	if (count == -1) {
	    rpmError(RPMERR_READERROR, "Unable to read sigtarget: %s",
		     strerror(errno));
	    close(ifd);
	    close(fd);
	    unlink(sigtarget);
	    free(sigtarget);
	    unlink(fileName);
	    return RPMERR_READERROR;
	}
	if (write(fd, buf, count) < 0) {
	    rpmError(RPMERR_NOSPACE, "Unable to write package: %s",
		     strerror(errno));
	    close(ifd);
	    close(fd);
	    unlink(sigtarget);
	    free(sigtarget);
	    unlink(fileName);
	    return RPMERR_NOSPACE;
	}
    }
    close(ifd);
    close(fd);
    unlink(sigtarget);
    free(sigtarget);

    rpmMessage(RPMMESS_NORMAL, "Wrote: %s\n", fileName);

    return 0;
}

static int cpio_gzip(int fdo, CSA_t *csa) {
    CFD_t *cfd = &csa->cpioCfd;
    int rc;
    char *failedFile;

    cfd->cpioIoType = cpioIoTypeGzFd;
    cfd->cpioGzFd = gzdopen(dup(fdo), "w9");
    rc = cpioBuildArchive(cfd, csa->cpioList, csa->cpioCount, NULL, NULL,
			  &csa->cpioArchiveSize, &failedFile);
    gzclose(cfd->cpioGzFd);

    if (rc) {
	if (rc & CPIO_CHECK_ERRNO)
	    rpmError(RPMERR_CPIO, "cpio failed on file %s: %s",
		     failedFile, strerror(errno));
	else
	    rpmError(RPMERR_CPIO, "cpio failed on file %s: %d",
		     failedFile, rc);
	return 1;
    }

    return 0;
}

static int cpio_copy(int fdo, CSA_t *csa) {
    char buf[BUFSIZ];
    size_t nb;

    while((nb = read(csa->cpioFdIn, buf, sizeof(buf))) > 0) {
	if (write(fdo, buf, nb) != nb) {
	    rpmError(RPMERR_CPIO, "cpio_copy write failed: %s",
		strerror(errno));
	    return 1;
	}
	csa->cpioArchiveSize += nb;
    }
    if (nb < 0) {
	rpmError(RPMERR_CPIO, "cpio_copy read failed: %s", strerror(errno));
	return 1;
    }
    return 0;
}

static StringBuf addFileToTagAux(Spec spec, char *file, StringBuf sb)
{
    char buf[BUFSIZ];
    FILE *f;

    strcpy(buf, "%{_builddir}/");
    expandMacros(spec, spec->macros, buf, sizeof(buf));
    strcat(buf, spec->buildSubdir);
    strcat(buf, "/");
    strcat(buf, file);

    if ((f = fopen(buf, "r")) == NULL) {
	freeStringBuf(sb);
	return NULL;
    }
    while (fgets(buf, sizeof(buf), f)) {
	if (expandMacros(spec, spec->macros, buf, sizeof(buf))) {
	    rpmError(RPMERR_BADSPEC, "line: %s", buf);
	    return NULL;
	}
	appendStringBuf(sb, buf);
    }
    fclose(f);

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

    if (! (sb = addFileToTagAux(spec, file, sb))) {
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
    if (! (sb = addFileToTagAux(spec, file, sb))) {
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
		     "Could not open PreIn file: %s", pkg->preInFile);
	    return RPMERR_BADFILENAME;
	}
    }
    if (pkg->preUnFile) {
	if (addFileToTag(spec, pkg->preUnFile, pkg->header, RPMTAG_PREUN)) {
	    rpmError(RPMERR_BADFILENAME,
		     "Could not open PreUn file: %s", pkg->preUnFile);
	    return RPMERR_BADFILENAME;
	}
    }
    if (pkg->postInFile) {
	if (addFileToTag(spec, pkg->postInFile, pkg->header, RPMTAG_POSTIN)) {
	    rpmError(RPMERR_BADFILENAME,
		     "Could not open PostIn file: %s", pkg->postInFile);
	    return RPMERR_BADFILENAME;
	}
    }
    if (pkg->postUnFile) {
	if (addFileToTag(spec, pkg->postUnFile, pkg->header, RPMTAG_POSTUN)) {
	    rpmError(RPMERR_BADFILENAME,
		     "Could not open PostUn file: %s", pkg->postUnFile);
	    return RPMERR_BADFILENAME;
	}
    }
    if (pkg->verifyFile) {
	if (addFileToTag(spec, pkg->verifyFile, pkg->header,
			 RPMTAG_VERIFYSCRIPT)) {
	    rpmError(RPMERR_BADFILENAME,
		     "Could not open VerifyScript file: %s", pkg->verifyFile);
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
			 "Could not open Trigger script file: %s",
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
