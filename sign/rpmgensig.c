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
#include "lib/rpmsignfiles.h"

#include "debug.h"

#if !defined(__GLIBC__) && !defined(__APPLE__)
char ** environ = NULL;
#endif

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
	switch(flags & O_ACCMODE) {
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
 * Return 0 on success, 1 on failure.
 */
static int putSignature(Header sigh, int ishdr, uint8_t *pkt, size_t pktlen)
{
    pgpDigParams sigp = NULL;
    rpmTagVal sigtag;
    struct rpmtd_s sigtd;
    int rc = 1; /* assume failure */
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

    /* Looks sane, insert into header */
    rpmtdReset(&sigtd);
    sigtd.count = pktlen;
    sigtd.data = pkt;
    sigtd.type = RPM_BIN_TYPE;
    sigtd.tag = sigtag;

    /* Argh, reversed return codes */
    rc = (headerPut(sigh, &sigtd, HEADERPUT_DEFAULT) == 0);

exit:
    pgpDigParamsFree(sigp);
    return rc;
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

    addMacro(NULL, "__plaintext_filename", NULL, namedPipeName, -1);
    addMacro(NULL, "__signature_filename", NULL, sigfile, -1);

    if (!(pid = fork())) {
	char *const *av;
	char *cmd = NULL;
	const char *gpg_path = rpmExpand("%{?_gpg_path}", NULL);

	if (gpg_path && *gpg_path != '\0')
	    (void) setenv("GNUPGHOME", gpg_path, 1);
	(void) setenv("LC_ALL", "C", 1);

	unsetenv("MALLOC_CHECK_");
	cmd = rpmExpand("%{?__gpg_sign_cmd}", NULL);
	rc = poptParseArgvString(cmd, NULL, (const char ***)&av);
	if (!rc)
	    rc = execve(av[0], av+1, environ);

	rpmlog(RPMLOG_ERR, _("Could not exec %s: %s\n"), "gpg",
			strerror(errno));
	_exit(EXIT_FAILURE);
    }

    delMacro(NULL, "__plaintext_filename");
    delMacro(NULL, "__signature_filename");

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
 * @return		0 on success, 1 on failure
 */
static int makeGPGSignature(Header sigh, int ishdr, sigTarget sigt)
{
    char * sigfile = rstrscat(NULL, sigt->fileName, ".sig", NULL);
    struct stat st;
    uint8_t * pkt = NULL;
    size_t pktlen = 0;
    int rc = 1; /* assume failure */

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

	rc = 0;
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
    rc = putSignature(sigh, ishdr, pkt, pktlen);
exit:
    (void) unlink(sigfile);
    free(sigfile);
    free(pkt);

    return rc;
}

static int rpmGenSignature(Header sigh, sigTarget sigt1, sigTarget sigt2)
{
    int ret;

    ret = makeGPGSignature(sigh, 0, sigt1);
    if (ret)
	goto exit;

    ret = makeGPGSignature(sigh, 1, sigt2);
    if (ret)
	goto exit;
exit:
    return ret;
}

/**
 * Retrieve signature from header tag
 * @param sigh		signature header
 * @param sigtag	signature tag
 * @return		parsed pgp dig or NULL
 */
static pgpDigParams getSig(Header sigh, rpmTagVal sigtag)
{
    struct rpmtd_s pkt;
    pgpDigParams sig = NULL;

    if (headerGet(sigh, sigtag, &pkt, HEADERGET_DEFAULT) && pkt.data != NULL) {
	pgpPrtParams(pkt.data, pkt.count, PGPTAG_SIGNATURE, &sig);
	rpmtdFreeData(&pkt);
    }
    return sig;
}

static void deleteSigs(Header sigh)
{
    headerDel(sigh, RPMSIGTAG_GPG);
    headerDel(sigh, RPMSIGTAG_PGP);
    headerDel(sigh, RPMSIGTAG_DSA);
    headerDel(sigh, RPMSIGTAG_RSA);
    headerDel(sigh, RPMSIGTAG_PGP5);
}

static int sameSignature(rpmTagVal sigtag, Header h1, Header h2)
{
    pgpDigParams sig1 = getSig(h1, sigtag);
    pgpDigParams sig2 = getSig(h2, sigtag);;

    int rc = pgpDigParamsCmp(sig1, sig2);

    pgpDigParamsFree(sig1);
    pgpDigParamsFree(sig2);
    return (rc == 0);
}

static int replaceSignature(Header sigh, sigTarget sigt1, sigTarget sigt2)
{
    /* Grab a copy of the header so we can compare the result */
    Header oldsigh = headerCopy(sigh);
    int rc = -1;
    
    /* Nuke all signature tags */
    deleteSigs(sigh);

    /*
     * rpmGenSignature() internals parse the actual signing result and 
     * adds appropriate tags for DSA/RSA.
     */
    if (rpmGenSignature(sigh, sigt1, sigt2) == 0) {
	/* Lets see what we got and whether its the same signature as before */
	rpmTagVal sigtag = headerIsEntry(sigh, RPMSIGTAG_DSA) ?
					RPMSIGTAG_DSA : RPMSIGTAG_RSA;

	rc = sameSignature(sigtag, sigh, oldsigh);

    }

    headerFree(oldsigh);
    return rc;
}

static void unloadImmutableRegion(Header *hdrp, rpmTagVal tag)
{
    struct rpmtd_s copytd, td;
    rpmtd utd = &td;
    Header nh;
    Header oh;
    HeaderIterator hi;

    if (headerGet(*hdrp, tag, utd, HEADERGET_DEFAULT)) {
	nh = headerNew();
	oh = headerCopyLoad(utd->data);
	hi = headerInitIterator(oh);

	while (headerNext(hi, &copytd)) {
	    if (copytd.data)
		headerPut(nh, &copytd, HEADERPUT_DEFAULT);
	    rpmtdFreeData(&copytd);
	}

	headerFreeIterator(hi);
	headerFree(oh);
	rpmtdFreeData(utd);
	headerFree(*hdrp);
	*hdrp = headerLink(nh);
	headerFree(nh);
    }
}

static rpmRC replaceSigDigests(FD_t fd, const char *rpm, Header *sigp,
			       off_t sigStart, off_t sigTargetSize,
			       char *SHA1, uint8_t *MD5)
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
    addMacro(NULL, "__gpg_reserved_space", NULL, 0, RMIL_GLOBAL);

    /* Replace old digests in sigh */
    rc = rpmGenerateSignature(SHA1, MD5, sigTargetSize, archiveSize, fd);
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
    rc = rpmReadSignature(fd, sigp, RPMSIGTYPE_HEADERSIG, NULL);
    if (rc != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, _("rpmReadSignature failed\n"));
	goto exit;
    }

exit:
    return rc;
}

static rpmRC includeFileSignatures(FD_t fd, const char *rpm,
				   Header *sigp, Header *hdrp,
				   off_t sigStart, off_t headerStart)
{
    FD_t ofd = NULL;
    char *trpm = NULL;
    const char *key;
    char *keypass;
    char *SHA1 = NULL;
    uint8_t *MD5 = NULL;
    size_t sha1len;
    size_t md5len;
    off_t sigTargetSize;
    rpmRC rc = RPMRC_OK;
    struct rpmtd_s osigtd;
    char *o_sha1 = NULL;
    uint8_t o_md5[16];

#ifndef WITH_IMAEVM
    rpmlog(RPMLOG_ERR, _("missing libimaevm\n"));
    return RPMRC_FAIL;
#endif
    unloadImmutableRegion(hdrp, RPMTAG_HEADERIMMUTABLE);

    key = rpmExpand("%{?_file_signing_key}", NULL);

    keypass = rpmExpand("%{_file_signing_key_password}", NULL);
    if (rstreq(keypass, ""))
	keypass = NULL;

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
    fdInitDigest(fd, PGPHASHALGO_MD5, 0);

    /* Write header to rpm and recalculate SHA1 */
    fdInitDigest(fd, PGPHASHALGO_SHA1, 0);
    rc = headerWrite(fd, *hdrp, HEADER_MAGIC_YES);
    if (rc != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, _("headerWrite failed\n"));
	goto exit;
    }
    fdFiniDigest(fd, PGPHASHALGO_SHA1, (void **)&SHA1, &sha1len, 1);

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
    fdFiniDigest(fd, PGPHASHALGO_MD5, (void **)&MD5, &md5len, 0);

    if (headerGet(*sigp, RPMSIGTAG_MD5, &osigtd, HEADERGET_DEFAULT))
	memcpy(o_md5, osigtd.data, 16);

    if (headerGet(*sigp, RPMSIGTAG_SHA1, &osigtd, HEADERGET_DEFAULT))
	o_sha1 = xstrdup(osigtd.data);

    if (memcmp(MD5, o_md5, md5len) == 0 && strcmp(SHA1, o_sha1) == 0)
	rpmlog(RPMLOG_WARNING,
	       _("%s already contains identical file signatures\n"),
	       rpm);
    else
	replaceSigDigests(fd, rpm, sigp, sigStart, sigTargetSize, SHA1, MD5);

exit:
if (ofd)    (void) closeFile(&ofd);
    return rc;
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
    rpmlead lead = NULL;
    char *trpm = NULL;
    Header sigh = NULL;
    Header h = NULL;
    char * msg = NULL;
    int res = -1; /* assume failure */
    rpmRC rc;
    struct rpmtd_s utd;
    off_t headerStart;
    off_t sigStart;
    struct sigTarget_s sigt1;
    struct sigTarget_s sigt2;
    unsigned int origSigSize;
    int insSig = 0;

    fprintf(stdout, "%s:\n", rpm);

    if (manageFile(&fd, rpm, O_RDWR))
	goto exit;

    if ((rc = rpmLeadRead(fd, &lead, NULL, &msg)) != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, "%s: %s\n", rpm, msg);
	free(msg);
	goto exit;
    }

    sigStart = Ftell(fd);
    rc = rpmReadSignature(fd, &sigh, RPMSIGTYPE_HEADERSIG, &msg);
    if (rc != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, _("%s: rpmReadSignature failed: %s"), rpm,
		    (msg && *msg ? msg : "\n"));
	msg = _free(msg);
	goto exit;
    }

    headerStart = Ftell(fd);
    if (rpmReadHeader(NULL, fd, &h, &msg) != RPMRC_OK) {
	rpmlog(RPMLOG_ERR, _("%s: headerRead failed: %s\n"), rpm, msg);
	msg = _free(msg);
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
	sigt1.fd = fd;
	sigt1.start = headerStart;
	sigt1.fileName = rpm;
	sigt1.size = fdSize(fd) - headerStart;

	/* Signature target containing only header */
	sigt2 = sigt1;
	sigt2.size = headerSizeof(h, HEADER_MAGIC_YES);

	res = replaceSignature(sigh, &sigt1, &sigt2);
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
	rc = rpmLeadWrite(ofd, lead);
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

    rpmFreeSignature(sigh);
    headerFree(h);
    rpmLeadFree(lead);

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
	    addMacro(NULL, "_gpg_digest_algo", NULL, algo, RMIL_GLOBAL);
	    free(algo);
	}
	if (args->keyid) {
	    addMacro(NULL, "_gpg_name", NULL, args->keyid, RMIL_GLOBAL);
	}
    }

    rc = rpmSign(path, 0, args->signfiles);

    if (args) {
	if (args->hashalgo) {
	    delMacro(NULL, "_gpg_digest_algo");
	}
	if (args->keyid) {
	    delMacro(NULL, "_gpg_name");
	}
    }

    return rc;
}

int rpmPkgDelSign(const char *path)
{
    return rpmSign(path, 1, 0);
}
