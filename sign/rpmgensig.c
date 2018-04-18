/** \ingroup rpmcli
 * \file lib/rpmchecksig.c
 * Verify the signature of a package.
 */

#include "system.h"

#include <errno.h>
#include <sys/wait.h>
#include <popt.h>
#include <libgen.h>

#include <rpm/rpmlib.h>			/* RPMSIGTAG & related */
#include <rpm/rpmmacro.h>
#include <rpm/rpmpgp.h>
#include <rpm/rpmsign.h>
#include <rpm/rpmfileutil.h>	/* rpmMkTemp() */
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include <rpmio/rpmio_internal.h>

#include "lib/rpmlead.h"
#include "lib/signature.h"
#include "sign/rpmsignfiles.h"

#include "debug.h"

typedef struct sigTarget_s {
    FD_t fd;
    const char *fileName;
    off_t start;
    rpm_loff_t size;
} *sigTarget;

/*
 * There is no function for creating unique temporary fifos so create
 * unique temporary directory and then create fifo in it.
 */
static char *mkTempFifo(void)
{
    char *tmppath = NULL, *tmpdir = NULL, *fifofn = NULL;
    mode_t mode;

    tmppath = rpmExpand("%{_tmppath}", NULL);
    if (rpmioMkpath(tmppath, 0755, (uid_t) -1, (gid_t) -1))
	goto exit;


    tmpdir = rpmGetPath(tmppath, "/rpm-tmp.XXXXXX", NULL);
    mode = umask(0077);
    tmpdir = mkdtemp(tmpdir);
    umask(mode);
    if (tmpdir == NULL) {
	rpmlog(RPMLOG_ERR, _("error creating temp directory %s: %m\n"),
	    tmpdir);
	tmpdir = _free(tmpdir);
	goto exit;
    }

    fifofn = rpmGetPath(tmpdir, "/fifo", NULL);
    if (mkfifo(fifofn, 0600) == -1) {
	rpmlog(RPMLOG_ERR, _("error creating fifo %s: %m\n"), fifofn);
	fifofn = _free(fifofn);
    }

exit:
    if (fifofn == NULL && tmpdir != NULL)
	unlink(tmpdir);

    free(tmppath);
    free(tmpdir);

    return fifofn;
}

/* Delete fifo and then temporary directory in which it was located */
static int rpmRmTempFifo(const char *fn)
{
    int rc = 0;
    char *dfn = NULL, *dir = NULL;

    if ((rc = unlink(fn)) != 0) {
	rpmlog(RPMLOG_ERR, _("error delete fifo %s: %m\n"), fn);
	return rc;
    }

    dfn = xstrdup(fn);
    dir = dirname(dfn);

    if ((rc = rmdir(dir)) != 0)
	rpmlog(RPMLOG_ERR, _("error delete directory %s: %m\n"), dir);
    free(dfn);

    return rc;
}

static int closeFile(FD_t *fdp)
{
    if (fdp == NULL || *fdp == NULL)
	return 1;

    /* close and reset *fdp to NULL */
    (void) Fclose(*fdp);
    *fdp = NULL;
    return 0;
}

/**
 */
static int manageFile(FD_t *fdp, const char *fn, int flags)
{
    FD_t fd;
    const char *fmode;

    if (fdp == NULL || fn == NULL)	/* programmer error */
	return 1;

    /* open a file and set *fdp */
    if (*fdp == NULL && fn != NULL) {
	switch (flags & O_ACCMODE) {
	    case O_WRONLY:
		fmode = "w.ufdio";
		break;
	    case O_RDONLY:
		fmode = "r.ufdio";
		break;
	    default:
	    case O_RDWR:
		fmode = "r+.ufdio";
		break;
	}
	fd = Fopen(fn, fmode);
	if (fd == NULL || Ferror(fd)) {
	    rpmlog(RPMLOG_ERR, _("%s: open failed: %s\n"), fn,
		Fstrerror(fd));
	    return 1;
	}
	*fdp = fd;
	return 0;
    }

    /* no operation */
    if (*fdp != NULL && fn != NULL)
	return 0;

    /* XXX never reached */
    return 1;
}

/**
 * Copy header+payload, calculating digest(s) on the fly.
 * @param sfdp source file
 * @param sfnp source path
 * @param tfdp destination file
 * @param tfnp destination path
 */
static int copyFile(FD_t *sfdp, const char *sfnp,
		FD_t *tfdp, const char *tfnp)
{
    unsigned char buf[BUFSIZ];
    ssize_t count;
    int rc = 1;

    while ((count = Fread(buf, sizeof(buf[0]), sizeof(buf), *sfdp)) > 0)
    {
	if (Fwrite(buf, sizeof(buf[0]), count, *tfdp) != count) {
	    rpmlog(RPMLOG_ERR, _("%s: Fwrite failed: %s\n"), tfnp,
		Fstrerror(*tfdp));
	    goto exit;
	}
    }
    if (count < 0) {
	rpmlog(RPMLOG_ERR, _("%s: Fread failed: %s\n"), sfnp, Fstrerror(*sfdp));
	goto exit;
    }
    if (Fflush(*tfdp) != 0) {
	rpmlog(RPMLOG_ERR, _("%s: Fflush failed: %s\n"), tfnp,
	    Fstrerror(*tfdp));
    }

    rc = 0;

exit:
    return rc;
}

/*
 * Validate generated signature and insert to header if it looks sane.
 * NSS doesn't support everything GPG does. Basic tests to see if the 
 * generated signature is something we can use.
 * Return generated signature tag data on success, NULL on failure.
 */
static rpmtd makeSigTag(Header sigh, int ishdr, uint8_t *pkt, size_t pktlen)
{
    pgpDigParams sigp = NULL;
    rpmTagVal sigtag;
    rpmtd sigtd = NULL;
    unsigned int hash_algo;
    unsigned int pubkey_algo;

    if (pgpPrtParams(pkt, pktlen, PGPTAG_SIGNATURE, &sigp)) {
	rpmlog(RPMLOG_ERR, _("Unsupported PGP signature\n"));
	goto exit;
    }

    hash_algo = pgpDigParamsAlgo(sigp, PGPVAL_HASHALGO);
    if (rpmDigestLength(hash_algo) == 0) {
	rpmlog(RPMLOG_ERR, _("Unsupported PGP hash algorithm %u\n"), hash_algo);
	goto exit;
    }

    pubkey_algo = pgpDigParamsAlgo(sigp, PGPVAL_PUBKEYALGO);
    switch (pubkey_algo) {
    case PGPPUBKEYALGO_DSA:
	sigtag = ishdr ? RPMSIGTAG_DSA : RPMSIGTAG_GPG;
	break;
    case PGPPUBKEYALGO_RSA:
	sigtag = ishdr ? RPMSIGTAG_RSA : RPMSIGTAG_PGP;
	break;
    default:
	rpmlog(RPMLOG_ERR, _("Unsupported PGP pubkey algorithm %u\n"),
		pubkey_algo);
	goto exit;
	break;
    }

    /* Looks sane, create the tag data */
    sigtd = rpmtdNew();
    sigtd->count = pktlen;
    sigtd->data = memcpy(xmalloc(pktlen), pkt, pktlen);;
    sigtd->type = RPM_BIN_TYPE;
    sigtd->tag = sigtag;
    sigtd->flags |= RPMTD_ALLOCED;

exit:
    pgpDigParamsFree(sigp);
    return sigtd;
}

static int runGPG(sigTarget sigt, const char *sigfile)
{
    int pid = 0, status;
    FD_t fnamedPipe = NULL;
    char *namedPipeName = NULL;
    unsigned char buf[BUFSIZ];
    ssize_t count;
    ssize_t wantCount;
    rpm_loff_t size;
    int rc = 1; /* assume failure */

    namedPipeName = mkTempFifo();

    rpmPushMacro(NULL, "__plaintext_filename", NULL, namedPipeName, -1);
    rpmPushMacro(NULL, "__signature_filename", NULL, sigfile, -1);

    if (!(pid = fork())) {
	char *const *av;
	char *cmd = NULL;
	const char *gpg_path = rpmExpand("%{?_gpg_path}", NULL);

	if (gpg_path && *gpg_path != '\0')
	    (void) setenv("GNUPGHOME", gpg_path, 1);

	unsetenv("MALLOC_CHECK_");
	cmd = rpmExpand("%{?__gpg_sign_cmd}", NULL);
	rc = poptParseArgvString(cmd, NULL, (const char ***)&av);
	if (!rc)
	    rc = execve(av[0], av+1, environ);

	rpmlog(RPMLOG_ERR, _("Could not exec %s: %s\n"), "gpg",
			strerror(errno));
	_exit(EXIT_FAILURE);
    }

    rpmPopMacro(NULL, "__plaintext_filename");
    rpmPopMacro(NULL, "__signature_filename");

    fnamedPipe = Fopen(namedPipeName, "w");
    if (!fnamedPipe) {
	rpmlog(RPMLOG_ERR, _("Fopen failed\n"));
	goto exit;
    }

    if (Fseek(sigt->fd, sigt->start, SEEK_SET) < 0) {
	rpmlog(RPMLOG_ERR, _("Could not seek in file %s: %s\n"),
		sigt->fileName, Fstrerror(sigt->fd));
	goto exit;
    }

    size = sigt->size;
    wantCount = size < sizeof(buf) ? size : sizeof(buf);
    while ((count = Fread(buf, sizeof(buf[0]), wantCount, sigt->fd)) > 0) {
	Fwrite(buf, sizeof(buf[0]), count, fnamedPipe);
	if (Ferror(fnamedPipe)) {
	    rpmlog(RPMLOG_ERR, _("Could not write to pipe\n"));
	    goto exit;
	}
	size -= count;
	wantCount = size < sizeof(buf) ? size : sizeof(buf);
    }
    if (count < 0) {
	rpmlog(RPMLOG_ERR, _("Could not read from file %s: %s\n"),
		sigt->fileName, Fstrerror(sigt->fd));
	goto exit;
    }
    Fclose(fnamedPipe);
    fnamedPipe = NULL;

    (void) waitpid(pid, &status, 0);
    pid = 0;
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	rpmlog(RPMLOG_ERR, _("gpg exec failed (%d)\n"), WEXITSTATUS(status));
    } else {
	rc = 0;
    }

exit:

    if (fnamedPipe)
	Fclose(fnamedPipe);

    if (pid)
	waitpid(pid, &status, 0);

    if (namedPipeName) {
	rpmRmTempFifo(namedPipeName);
	free(namedPipeName);
    }

    return rc;
}

/**
 * Generate GPG signature(s) for a header+payload file.
 * @param sigh		signature header
 * @param ishdr		header-only signature?
 * @param sigt		signature target
 * @param passPhrase	private key pass phrase
 * @return		generated sigtag on success, 0 on failure
 */
static rpmtd makeGPGSignature(Header sigh, int ishdr, sigTarget sigt)
{
    char * sigfile = rstrscat(NULL, sigt->fileName, ".sig", NULL);
    struct stat st;
    uint8_t * pkt = NULL;
    size_t pktlen = 0;
    rpmtd sigtd = NULL;

    if (runGPG(sigt, sigfile))
	goto exit;

    if (stat(sigfile, &st)) {
	/* GPG failed to write signature */
	rpmlog(RPMLOG_ERR, _("gpg failed to write signature\n"));
	goto exit;
    }

    pktlen = st.st_size;
    rpmlog(RPMLOG_DEBUG, "GPG sig size: %zd\n", pktlen);
    pkt = xmalloc(pktlen);

    {	FD_t fd;

	int rc = 0;
	fd = Fopen(sigfile, "r.ufdio");
	if (fd != NULL && !Ferror(fd)) {
	    rc = Fread(pkt, sizeof(*pkt), pktlen, fd);
	    (void) Fclose(fd);
	}
	if (rc != pktlen) {
	    rpmlog(RPMLOG_ERR, _("unable to read the signature\n"));
	    goto exit;
	}
    }

    rpmlog(RPMLOG_DEBUG, "Got %zd bytes of GPG sig\n", pktlen);

    /* Parse the signature, change signature tag as appropriate. */
    sigtd = makeSigTag(sigh, ishdr, pkt, pktlen);
exit:
    (void) unlink(sigfile);
    free(sigfile);
    free(pkt);

    return sigtd;
}

static void deleteSigs(Header sigh)
{
    headerDel(sigh, RPMSIGTAG_GPG);
    headerDel(sigh, RPMSIGTAG_PGP);
    headerDel(sigh, RPMSIGTAG_DSA);
    headerDel(sigh, RPMSIGTAG_RSA);
    headerDel(sigh, RPMSIGTAG_PGP5);
}

static int haveSignature(rpmtd sigtd, Header h)
{
    pgpDigParams sig1 = NULL;
    pgpDigParams sig2 = NULL;
    struct rpmtd_s oldtd;
    int rc = 0; /* assume no */

    if (!headerGet(h, rpmtdTag(sigtd), &oldtd, HEADERGET_DEFAULT))
	return rc;

    pgpPrtParams(sigtd->data, sigtd->count, PGPTAG_SIGNATURE, &sig1);
    while (rpmtdNext(&oldtd) >= 0 && rc == 0) {
	pgpPrtParams(oldtd.data, oldtd.count, PGPTAG_SIGNATURE, &sig2);
	if (pgpDigParamsCmp(sig1, sig2) == 0)
	    rc = 1;
	pgpDigParamsFree(sig2);
    }
    pgpDigParamsFree(sig1);
    rpmtdFreeData(&oldtd);

    return rc;
}

static int replaceSignature(Header sigh, sigTarget sigt_v3, sigTarget sigt_v4)
{
    int rc = -1;
    rpmtd sigtd = NULL;
    
    /* Make the cheaper v4 signature first */
    if ((sigtd = makeGPGSignature(sigh, 1, sigt_v4)) == NULL)
	goto exit;

    /* See if we already have a signature by the same key and parameters */
    if (haveSignature(sigtd, sigh)) {
	rc = 1;
	goto exit;
    }
    /* Nuke all signature tags */
    deleteSigs(sigh);

    if (headerPut(sigh, sigtd, HEADERPUT_DEFAULT) == 0)
	goto exit;
    rpmtdFree(sigtd);

    /* Assume the same signature test holds for v3 signature too */
    if ((sigtd = makeGPGSignature(sigh, 0, sigt_v3)) == NULL)
	goto exit;

    if (headerPut(sigh, sigtd, HEADERPUT_DEFAULT) == 0)
	goto exit;

    rc = 0;
exit:
    rpmtdFree(sigtd);
    return rc;
}

static void unloadImmutableRegion(Header *hdrp, rpmTagVal tag)
{
    struct rpmtd_s td;
    rpmtd utd = &td;
    Header nh;
    Header oh;

    if (headerGet(*hdrp, tag, utd, HEADERGET_DEFAULT)) {
	oh = headerCopyLoad(utd->data);
	nh = headerCopy(oh);
	headerFree(oh);
	rpmtdFreeData(utd);
	headerFree(*hdrp);
	*hdrp = headerLink(nh);
	headerFree(nh);
    }
}

#ifdef WITH_IMAEVM
static rpmRC replaceSigDigests(FD_t fd, const char *rpm, Header *sigp,
			       off_t sigStart, off_t sigTargetSize,
			       char *SHA256, char *SHA1, uint8_t *MD5)
{
    off_t archiveSize;
    rpmRC rc = RPMRC_OK;

    if (Fseek(fd, sigStart, SEEK_SET) < 0) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Could not seek in file %s: %s\n"),
		rpm, Fstrerror(fd));
	goto exit;
    }

    /* Get payload size from signature tag */
    archiveSize = headerGetNumber(*sigp, RPMSIGTAG_PAYLOADSIZE);
    if (!archiveSize) {
	archiveSize = headerGetNumber(*sigp, RPMSIGTAG_LONGARCHIVESIZE);
    }

    /* Set reserved space to 0 */
    rpmPushMacro(NULL, "__gpg_reserved_space", NULL, 0, RMIL_GLOBAL);

    /* Replace old digests in sigh */
    rc = rpmGenerateSignature(SHA256, SHA1, MD5, sigTargetSize, archiveSize, fd);
    if (rc != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, _("generateSignature failed\n"));
	goto exit;
    }

    if (Fseek(fd, sigStart, SEEK_SET) < 0) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Could not seek in file %s: %s\n"),
		rpm, Fstrerror(fd));
	goto exit;
    }

    headerFree(*sigp);
    rc = rpmReadSignature(fd, sigp, NULL);
    if (rc != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, _("rpmReadSignature failed\n"));
	goto exit;
    }

exit:
    return rc;
}
#endif

static rpmRC includeFileSignatures(FD_t fd, const char *rpm,
				   Header *sigp, Header *hdrp,
				   off_t sigStart, off_t headerStart)
{
#ifdef WITH_IMAEVM
    FD_t ofd = NULL;
    char *trpm = NULL;
    char *key;
    char *keypass;
    char *SHA1 = NULL;
    char *SHA256 = NULL;
    uint8_t *MD5 = NULL;
    off_t sigTargetSize;
    rpmRC rc = RPMRC_OK;
    struct rpmtd_s osigtd;
    char *o_sha1 = NULL;

    unloadImmutableRegion(hdrp, RPMTAG_HEADERIMMUTABLE);

    key = rpmExpand("%{?_file_signing_key}", NULL);

    keypass = rpmExpand("%{?_file_signing_key_password}", NULL);
    if (rstreq(keypass, "")) {
	free(keypass);
	keypass = NULL;
    }

    rc = rpmSignFiles(*hdrp, key, keypass);
    if (rc != RPMRC_OK) {
	goto exit;
    }

    *hdrp = headerReload(*hdrp, RPMTAG_HEADERIMMUTABLE);
    if (*hdrp == NULL) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("headerReload failed\n"));
	goto exit;
    }

    ofd = rpmMkTempFile(NULL, &trpm);
    if (ofd == NULL || Ferror(ofd)) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("rpmMkTemp failed\n"));
	goto exit;
    }

    /* Copy archive to temp file */
    if (copyFile(&fd, rpm, &ofd, trpm)) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("copyFile failed\n"));
	goto exit;
    }

    if (Fseek(fd, headerStart, SEEK_SET) < 0) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Could not seek in file %s: %s\n"),
		rpm, Fstrerror(fd));
	goto exit;
    }

    /* Start MD5 calculation */
    fdInitDigestID(fd, PGPHASHALGO_MD5, RPMSIGTAG_MD5, 0);

    /* Write header to rpm and recalculate digests */
    fdInitDigestID(fd, PGPHASHALGO_SHA1, RPMSIGTAG_SHA1, 0);
    fdInitDigestID(fd, PGPHASHALGO_SHA256, RPMSIGTAG_SHA256, 0);
    rc = headerWrite(fd, *hdrp, HEADER_MAGIC_YES);
    if (rc != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, _("headerWrite failed\n"));
	goto exit;
    }
    fdFiniDigest(fd, RPMSIGTAG_SHA1, (void **)&SHA1, NULL, 1);
    /* Only add SHA256 if it was there to begin with */
    if (headerIsEntry(*sigp, RPMSIGTAG_SHA256))
	fdFiniDigest(fd, RPMSIGTAG_SHA256, (void **)&SHA256, NULL, 1);

    /* Copy archive from temp file */
    if (Fseek(ofd, 0, SEEK_SET) < 0) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("Could not seek in file %s: %s\n"),
		rpm, Fstrerror(fd));
	goto exit;
    }
    if (copyFile(&ofd, trpm, &fd, rpm)) {
	rc = RPMRC_FAIL;
	rpmlog(RPMLOG_ERR, _("copyFile failed\n"));
	goto exit;
    }
    unlink(trpm);

    sigTargetSize = Ftell(fd) - headerStart;
    fdFiniDigest(fd, RPMSIGTAG_MD5, (void **)&MD5, NULL, 0);

    if (headerGet(*sigp, RPMSIGTAG_SHA1, &osigtd, HEADERGET_DEFAULT)) {
	o_sha1 = xstrdup(osigtd.data);
	rpmtdFreeData(&osigtd);
    }

    if (strcmp(SHA1, o_sha1) == 0)
	rpmlog(RPMLOG_WARNING,
	       _("%s already contains identical file signatures\n"),
	       rpm);
    else
	replaceSigDigests(fd, rpm, sigp, sigStart, sigTargetSize, SHA256, SHA1, MD5);

exit:
    free(trpm);
    free(MD5);
    free(SHA1);
    free(SHA256);
    free(o_sha1);
    free(keypass);
    free(key);
    if (ofd)
	(void) closeFile(&ofd);
    return rc;
#else
    rpmlog(RPMLOG_ERR, _("file signing support not built in\n"));
    return RPMRC_FAIL;
#endif
}

/** \ingroup rpmcli
 * Create/modify elements in signature header.
 * @param rpm		path to package
 * @param deleting	adding or deleting signature?
 * @param signfiles	sign files if non-zero
 * @return		0 on success, -1 on error
 */
static int rpmSign(const char *rpm, int deleting, int signfiles)
{
    FD_t fd = NULL;
    FD_t ofd = NULL;
    char *trpm = NULL;
    Header sigh = NULL;
    Header h = NULL;
    char *msg = NULL;
    int res = -1; /* assume failure */
    rpmRC rc;
    struct rpmtd_s utd;
    off_t headerStart;
    off_t sigStart;
    struct sigTarget_s sigt_v3;
    struct sigTarget_s sigt_v4;
    unsigned int origSigSize;
    int insSig = 0;

    fprintf(stdout, "%s:\n", rpm);

    if (manageFile(&fd, rpm, O_RDWR))
	goto exit;

    if ((rc = rpmLeadRead(fd, &msg)) != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, "%s: %s\n", rpm, msg);
	goto exit;
    }

    sigStart = Ftell(fd);
    rc = rpmReadSignature(fd, &sigh, &msg);
    if (rc != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, _("%s: rpmReadSignature failed: %s"), rpm,
		    (msg && *msg ? msg : "\n"));
	goto exit;
    }

    headerStart = Ftell(fd);
    if (rpmReadHeader(NULL, fd, &h, &msg) != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, _("%s: headerRead failed: %s\n"), rpm, msg);
	goto exit;
    }

    if (!headerIsEntry(h, RPMTAG_HEADERIMMUTABLE)) {
	rpmlog(RPMLOG_ERR, _("Cannot sign RPM v3 packages\n"));
	goto exit;
    }

    if (signfiles) {
	includeFileSignatures(fd, rpm, &sigh, &h, sigStart, headerStart);
    }

    unloadImmutableRegion(&sigh, RPMTAG_HEADERSIGNATURES);
    origSigSize = headerSizeof(sigh, HEADER_MAGIC_YES);

    if (deleting) {	/* Nuke all the signature tags. */
	deleteSigs(sigh);
    } else {
	/* Signature target containing header + payload */
	sigt_v3.fd = fd;
	sigt_v3.start = headerStart;
	sigt_v3.fileName = rpm;
	sigt_v3.size = fdSize(fd) - headerStart;

	/* Signature target containing only header */
	sigt_v4 = sigt_v3;
	sigt_v4.size = headerSizeof(h, HEADER_MAGIC_YES);

	res = replaceSignature(sigh, &sigt_v3, &sigt_v4);
	if (res != 0) {
	    if (res == 1) {
		rpmlog(RPMLOG_WARNING,
		   _("%s already contains identical signature, skipping\n"),
		   rpm);
		/* Identical signature is not an error */
		res = 0;
	    }
	    goto exit;
	}
	res = -1;
    }

    /* Try to make new signature smaller to have size of original signature */
    rpmtdReset(&utd);
    if (headerGet(sigh, RPMSIGTAG_RESERVEDSPACE, &utd, HEADERGET_MINMEM)) {
	int diff;
	int count;
	char *reservedSpace = NULL;

	count = utd.count;
	diff = headerSizeof(sigh, HEADER_MAGIC_YES) - origSigSize;

	if (diff < count) {
	    reservedSpace = xcalloc(count - diff, sizeof(char));
	    headerDel(sigh, RPMSIGTAG_RESERVEDSPACE);
	    rpmtdReset(&utd);
	    utd.tag = RPMSIGTAG_RESERVEDSPACE;
	    utd.count = count - diff;
	    utd.type = RPM_BIN_TYPE;
	    utd.data = reservedSpace;
	    headerPut(sigh, &utd, HEADERPUT_DEFAULT);
	    free(reservedSpace);
	    insSig = 1;
	}
    }

    /* Reallocate the signature into one contiguous region. */
    sigh = headerReload(sigh, RPMTAG_HEADERSIGNATURES);
    if (sigh == NULL)	/* XXX can't happen */
	goto exit;

    if (insSig) {
	/* Insert new signature into original rpm */
	if (Fseek(fd, sigStart, SEEK_SET) < 0) {
	    rpmlog(RPMLOG_ERR, _("Could not seek in file %s: %s\n"),
		    rpm, Fstrerror(fd));
	    goto exit;
	}

	if (rpmWriteSignature(fd, sigh)) {
	    rpmlog(RPMLOG_ERR, _("%s: rpmWriteSignature failed: %s\n"), rpm,
		Fstrerror(fd));
	    goto exit;
	}
	res = 0;
    } else {
	/* Replace orignal rpm with new rpm containing new signature */
	rasprintf(&trpm, "%s.XXXXXX", rpm);
	ofd = rpmMkTemp(trpm);
	if (ofd == NULL || Ferror(ofd)) {
	    rpmlog(RPMLOG_ERR, _("rpmMkTemp failed\n"));
	    goto exit;
	}

	/* Write the lead/signature of the output rpm */
	rc = rpmLeadWrite(ofd, h);
	if (rc != RPMRC_OK) {
	    rpmlog(RPMLOG_ERR, _("%s: writeLead failed: %s\n"), trpm,
		Fstrerror(ofd));
	    goto exit;
	}

	if (rpmWriteSignature(ofd, sigh)) {
	    rpmlog(RPMLOG_ERR, _("%s: rpmWriteSignature failed: %s\n"), trpm,
		Fstrerror(ofd));
	    goto exit;
	}

	if (Fseek(fd, headerStart, SEEK_SET) < 0) {
	    rpmlog(RPMLOG_ERR, _("Could not seek in file %s: %s\n"),
		    rpm, Fstrerror(fd));
	    goto exit;
	}
	/* Append the header and archive from the temp file */
	if (copyFile(&fd, rpm, &ofd, trpm) == 0) {
	    struct stat st;

	    /* Move final target into place, restore file permissions. */
	    if (stat(rpm, &st) == 0 && unlink(rpm) == 0 &&
			rename(trpm, rpm) == 0 && chmod(rpm, st.st_mode) == 0) {
		res = 0;
	    } else {
		rpmlog(RPMLOG_ERR, _("replacing %s failed: %s\n"),
		       rpm, strerror(errno));
	    }
	}
    }

exit:
    if (fd)	(void) closeFile(&fd);
    if (ofd)	(void) closeFile(&ofd);

    headerFree(sigh);
    headerFree(h);
    free(msg);

    /* Clean up intermediate target */
    if (trpm) {
	(void) unlink(trpm);
	free(trpm);
    }

    return res;
}

int rpmPkgSign(const char *path, const struct rpmSignArgs * args)
{
    int rc;

    if (args) {
	if (args->hashalgo) {
	    char *algo = NULL;
	    rasprintf(&algo, "%d", args->hashalgo);
	    rpmPushMacro(NULL, "_gpg_digest_algo", NULL, algo, RMIL_GLOBAL);
	    free(algo);
	}
	if (args->keyid) {
	    rpmPushMacro(NULL, "_gpg_name", NULL, args->keyid, RMIL_GLOBAL);
	}
    }

    rc = rpmSign(path, 0, args ? args->signfiles : 0);

    if (args) {
	if (args->hashalgo) {
	    rpmPopMacro(NULL, "_gpg_digest_algo");
	}
	if (args->keyid) {
	    rpmPopMacro(NULL, "_gpg_name");
	}
    }

    return rc;
}

int rpmPkgDelSign(const char *path, const struct rpmSignArgs * args)
{
    return rpmSign(path, 1, 0);
}
