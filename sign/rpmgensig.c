/** \ingroup rpmcli
 * \file lib/rpmchecksig.c
 * Verify the signature of a package.
 */

#include "system.h"

#include <errno.h>
#include <sys/wait.h>
#include <popt.h>

#include <rpm/rpmlib.h>			/* RPMSIGTAG & related */
#include <rpm/rpmmacro.h>
#include <rpm/rpmpgp.h>
#include <rpm/rpmsign.h>
#include <rpm/rpmfileutil.h>	/* rpmMkTemp() */
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>

#include "lib/rpmlead.h"
#include "lib/signature.h"

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
 */
static int copyFile(FD_t *sfdp, const char *sfnp,
		FD_t *tfdp, const char *tfnp)
{
    unsigned char buf[BUFSIZ];
    ssize_t count;
    int rc = 1;

    if (manageFile(sfdp, sfnp, O_RDONLY))
	goto exit;
    if (manageFile(tfdp, tfnp, O_WRONLY|O_CREAT|O_TRUNC))
	goto exit;

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
    if (*sfdp)	(void) closeFile(sfdp);
    if (*tfdp)	(void) closeFile(tfdp);
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

static int runGPG(sigTarget sigt, const char *sigfile, const char * passPhrase)
{
    int pid = 0, status;
    int inpipe[2];
    int inpipe2[2];
    FILE * fpipe = NULL;
    unsigned char buf[BUFSIZ];
    ssize_t count;
    ssize_t wantCount;
    rpm_loff_t size;
    int rc = 1; /* assume failure */

    inpipe[0] = inpipe[1] = 0;
    if (pipe(inpipe) < 0) {
	rpmlog(RPMLOG_ERR, _("Couldn't create pipe for signing: %m"));
	goto exit;
    }

    inpipe2[0] = inpipe2[1] = 0;
    if (pipe(inpipe2) < 0) {
	rpmlog(RPMLOG_ERR, _("Couldn't create pipe for signing: %m"));
	goto exit;
    }

    addMacro(NULL, "__plaintext_filename", NULL, "-", -1);
    addMacro(NULL, "__signature_filename", NULL, sigfile, -1);

    if (!(pid = fork())) {
	char *const *av;
	char *cmd = NULL;
	const char *gpg_path = rpmExpand("%{?_gpg_path}", NULL);

	(void) dup2(inpipe[0], 3);
	(void) close(inpipe[1]);

	(void) dup2(inpipe2[0], STDIN_FILENO);
	(void) close(inpipe2[1]);

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

    (void) close(inpipe[0]);
    inpipe[0] = 0;
    (void) close(inpipe2[0]);
    inpipe2[0] = 0;

    fpipe = fdopen(inpipe[1], "w");
    if (!fpipe) {
	rpmlog(RPMLOG_ERR, _("fdopen failed\n"));
	goto exit;
    }
    inpipe[1] = 0;

    if (fprintf(fpipe, "%s\n", (passPhrase ? passPhrase : "")) < 0) {
	rpmlog(RPMLOG_ERR, _("Could not write to pipe\n"));
	goto exit;
    }
    (void) fclose(fpipe);
    fpipe = NULL;

    fpipe = fdopen(inpipe2[1], "w");
    if (!fpipe) {
	rpmlog(RPMLOG_ERR, _("fdopen failed\n"));
	goto exit;
    }
    inpipe2[1] = 0;

    if (Fseek(sigt->fd, sigt->start, SEEK_SET) < 0) {
	rpmlog(RPMLOG_ERR, _("Could not seek in file %s: %s\n"),
		sigt->fileName, Fstrerror(sigt->fd));
	goto exit;
    }

    size = sigt->size;
    wantCount = size < sizeof(buf) ? size : sizeof(buf);
    while ((count = Fread(buf, sizeof(buf[0]), wantCount, sigt->fd)) > 0) {
	fwrite(buf, sizeof(buf[0]), count, fpipe);
	if (ferror(fpipe)) {
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
    fclose(fpipe);
    fpipe = NULL;

    (void) waitpid(pid, &status, 0);
    pid = 0;
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	rpmlog(RPMLOG_ERR, _("gpg exec failed (%d)\n"), WEXITSTATUS(status));
    } else {
	rc = 0;
    }

exit:
    if (fpipe)
	fclose(fpipe);

    if (inpipe[0])
	close(inpipe[0]);

    if (inpipe[1])
	close(inpipe[1]);

    if (inpipe2[0])
	close(inpipe[0]);

    if (inpipe2[1])
	close(inpipe[1]);

    if (pid)
	waitpid(pid, &status, 0);


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
static int makeGPGSignature(Header sigh, int ishdr, sigTarget sigt,
			    const char * passPhrase)
{
    char * sigfile = rstrscat(NULL, sigt->fileName, ".sig", NULL);
    struct stat st;
    uint8_t * pkt = NULL;
    size_t pktlen = 0;
    int rc = 1; /* assume failure */

    if (runGPG(sigt, sigfile, passPhrase))
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

static int rpmGenSignature(Header sigh, sigTarget sigt1, sigTarget sigt2,
			    const char * passPhrase)
{
    int ret;

    ret = makeGPGSignature(sigh, 0, sigt1, passPhrase);
    if (ret)
	goto exit;

    ret = makeGPGSignature(sigh, 1, sigt2, passPhrase);
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

static int replaceSignature(Header sigh, sigTarget sigt1, sigTarget sigt2,
			    const char *passPhrase)
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
    if (rpmGenSignature(sigh, sigt1, sigt2, passPhrase) == 0) {
	/* Lets see what we got and whether its the same signature as before */
	rpmTagVal sigtag = headerIsEntry(sigh, RPMSIGTAG_DSA) ?
					RPMSIGTAG_DSA : RPMSIGTAG_RSA;

	rc = sameSignature(sigtag, sigh, oldsigh);

    }

    headerFree(oldsigh);
    return rc;
}

/** \ingroup rpmcli
 * Create/modify elements in signature header.
 * @param rpm		path to package
 * @param deleting	adding or deleting signature?
 * @param passPhrase	passPhrase (ignored when deleting)
 * @return		0 on success, -1 on error
 */
static int rpmSign(const char *rpm, int deleting, const char *passPhrase)
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

    /* Dump the immutable region (if present). */
    if (headerGet(sigh, RPMTAG_HEADERSIGNATURES, &utd, HEADERGET_DEFAULT)) {
	struct rpmtd_s copytd;
	Header nh = headerNew();
	Header oh = headerCopyLoad(utd.data);
	HeaderIterator hi = headerInitIterator(oh);
	while (headerNext(hi, &copytd)) {
	    if (copytd.data)
		headerPut(nh, &copytd, HEADERPUT_DEFAULT);
	    rpmtdFreeData(&copytd);
	}
	headerFreeIterator(hi);
	headerFree(oh);
	rpmtdFreeData(&utd);

	headerFree(sigh);
	sigh = headerLink(nh);
	headerFree(nh);
    }
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

	res = replaceSignature(sigh, &sigt1, &sigt2, passPhrase);
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

int rpmPkgSign(const char *path,
		const struct rpmSignArgs * args, const char *passPhrase)
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

    rc = rpmSign(path, 0, passPhrase);

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
    return rpmSign(path, 1, NULL);
}
