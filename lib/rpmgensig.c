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

#include "rpmio/digest.h"
#include "lib/rpmlead.h"
#include "lib/signature.h"

#include "debug.h"

#if !defined(__GLIBC__) && !defined(__APPLE__)
char ** environ = NULL;
#endif

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

    if (fdp == NULL || fn == NULL)	/* programmer error */
	return 1;

    /* open a file and set *fdp */
    if (*fdp == NULL && fn != NULL) {
	fd = Fopen(fn, (flags & O_ACCMODE) == O_WRONLY ? "w.ufdio" : "r.ufdio");
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
 * NSS doesn't support everything GPG does. Basic tests to see if the 
 * generated signature is something we can use.
 */
static int validatePGPSig(pgpDigParams sigp)
{
    pgpPubkeyAlgo pa = sigp->pubkey_algo;
    /* TODO: query from the implementation instead of hardwiring here */
    if (pa != PGPPUBKEYALGO_DSA && pa != PGPPUBKEYALGO_RSA) {
	rpmlog(RPMLOG_ERR, _("Unsupported PGP pubkey algorithm %d\n"),
		sigp->pubkey_algo);
	return 1;
    }

    if (rpmDigestLength(sigp->hash_algo) == 0) {
	rpmlog(RPMLOG_ERR, _("Unsupported PGP hash algorithm %d\n"),
	       sigp->hash_algo);
	return 1;
    }

    return 0;
}

static int runGPG(const char *file, const char *sigfile, const char * passPhrase)
{
    int pid, status;
    int inpipe[2];
    FILE * fpipe;
    int rc = 1; /* assume failure */

    inpipe[0] = inpipe[1] = 0;
    if (pipe(inpipe) < 0) {
	rpmlog(RPMLOG_ERR, _("Couldn't create pipe for signing: %m"));
	goto exit;
    }

    addMacro(NULL, "__plaintext_filename", NULL, file, -1);
    addMacro(NULL, "__signature_filename", NULL, sigfile, -1);

    if (!(pid = fork())) {
	char *const *av;
	char *cmd = NULL;
	const char *gpg_path = rpmExpand("%{?_gpg_path}", NULL);

	(void) dup2(inpipe[0], 3);
	(void) close(inpipe[1]);

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

    fpipe = fdopen(inpipe[1], "w");
    (void) close(inpipe[0]);
    if (fpipe) {
	fprintf(fpipe, "%s\n", (passPhrase ? passPhrase : ""));
	(void) fclose(fpipe);
    }

    (void) waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	rpmlog(RPMLOG_ERR, _("gpg exec failed (%d)\n"), WEXITSTATUS(status));
    } else {
	rc = 0;
    }
exit:
    return rc;
}

/**
 * Generate GPG signature(s) for a header+payload file.
 * @param file		header+payload file name
 * @retval *sigTagp	signature tag
 * @retval *pktp	signature packet(s)
 * @retval *pktlenp	signature packet(s) length
 * @param passPhrase	private key pass phrase
 * @return		0 on success, 1 on failure
 */
static int makeGPGSignature(const char * file, rpmSigTag * sigTagp,
		uint8_t ** pktp, size_t * pktlenp,
		const char * passPhrase)
{
    char * sigfile = rstrscat(NULL, file, ".sig", NULL);
    struct stat st;
    pgpDig dig = NULL;
    pgpDigParams sigp = NULL;
    int rc = 1; /* assume failure */

    if (runGPG(file, sigfile, passPhrase))
	goto exit;

    if (stat(sigfile, &st)) {
	/* GPG failed to write signature */
	rpmlog(RPMLOG_ERR, _("gpg failed to write signature\n"));
	goto exit;
    }

    *pktlenp = st.st_size;
    rpmlog(RPMLOG_DEBUG, "GPG sig size: %zd\n", *pktlenp);
    *pktp = xmalloc(*pktlenp);

    {	FD_t fd;

	rc = 0;
	fd = Fopen(sigfile, "r.ufdio");
	if (fd != NULL && !Ferror(fd)) {
	    rc = Fread(*pktp, sizeof(**pktp), *pktlenp, fd);
	    (void) Fclose(fd);
	}
	if (rc != *pktlenp) {
	    *pktp = _free(*pktp);
	    rpmlog(RPMLOG_ERR, _("unable to read the signature\n"));
	    goto exit;
	}
    }

    rpmlog(RPMLOG_DEBUG, "Got %zd bytes of GPG sig\n", *pktlenp);

    /* Parse the signature, change signature tag as appropriate. */
    dig = pgpNewDig();

    (void) pgpPrtPkts(*pktp, *pktlenp, dig, 0);
    sigp = &dig->signature;

    switch (*sigTagp) {
    case RPMSIGTAG_GPG:
	/* XXX check MD5 hash too? */
	if (sigp->pubkey_algo == PGPPUBKEYALGO_RSA)
	    *sigTagp = RPMSIGTAG_PGP;
	break;
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
	if (sigp->pubkey_algo == PGPPUBKEYALGO_DSA)
	    *sigTagp = RPMSIGTAG_GPG;
	break;
    case RPMSIGTAG_DSA:
	/* XXX check MD5 hash too? */
	if (sigp->pubkey_algo == PGPPUBKEYALGO_RSA)
	    *sigTagp = RPMSIGTAG_RSA;
	break;
    case RPMSIGTAG_RSA:
	if (sigp->pubkey_algo == PGPPUBKEYALGO_DSA)
	    *sigTagp = RPMSIGTAG_DSA;
	break;
    default:
	break;
    }

    rc = validatePGPSig(sigp);
    dig = pgpFreeDig(dig);

exit:
    (void) unlink(sigfile);
    free(sigfile);

    return rc;
}

/**
 * Generate header only signature(s) from a header+payload file.
 * @param sigh		signature header
 * @param file		header+payload file name
 * @param sigTag	type of signature(s) to add
 * @param passPhrase	private key pass phrase
 * @return		0 on success, -1 on failure
 */
static int makeHDRSignature(Header sigh, const char * file, rpmSigTag sigTag,
		const char * passPhrase)
{
    Header h = NULL;
    FD_t fd = NULL;
    uint8_t * pkt = NULL;
    size_t pktlen;
    char * fn = NULL;
    int ret = -1;	/* assume failure. */

    fd = Fopen(file, "r.fdio");
    if (fd == NULL || Ferror(fd))
	goto exit;
    h = headerRead(fd, HEADER_MAGIC_YES);
    if (h == NULL)
	goto exit;
    (void) Fclose(fd);

    fd = rpmMkTempFile(NULL, &fn);
    if (fd == NULL || Ferror(fd))
	goto exit;
    if (headerWrite(fd, h, HEADER_MAGIC_YES))
	goto exit;

    if (makeGPGSignature(fn, &sigTag, &pkt, &pktlen, passPhrase) != 0)
	goto exit;
    if (sighdrPut(sigh, sigTag, RPM_BIN_TYPE, pkt, pktlen) == 0)
	goto exit;

    ret = 0;

exit:
    if (fn) {
	(void) unlink(fn);
	free(fn);
    }
    free(pkt);
    h = headerFree(h);
    if (fd != NULL) (void) Fclose(fd);
    return ret;
}

static int rpmGenSignature(Header sigh, const char * file,
		const char * passPhrase)
{
    uint8_t * pkt = NULL;
    size_t pktlen;
    rpmSigTag sigTag = RPMSIGTAG_GPG;
    rpmSigTag hdrtag;
    int ret = -1;	/* assume failure. */

    if (makeGPGSignature(file, &sigTag, &pkt, &pktlen, passPhrase) != 0)
	goto exit;
    if (sighdrPut(sigh, sigTag, RPM_BIN_TYPE, pkt, pktlen) == 0)
	goto exit;

    /* XXX Piggyback a header-only DSA/RSA signature as well. */
    hdrtag = (sigTag == RPMSIGTAG_GPG) ?  RPMSIGTAG_DSA : RPMSIGTAG_RSA;
    ret = makeHDRSignature(sigh, file, hdrtag, passPhrase);

exit:
    free(pkt);
    return ret;
}

/**
 * Retrieve signature from header tag
 * @param sigh		signature header
 * @param sigtag	signature tag
 * @return		parsed pgp dig or NULL
 */
static pgpDig getSig(Header sigh, rpmSigTag sigtag)
{
    struct rpmtd_s pkt;
    pgpDig dig = NULL;

    if (headerGet(sigh, sigtag, &pkt, HEADERGET_DEFAULT) && pkt.data != NULL) {
	dig = pgpNewDig();

	if (pgpPrtPkts(pkt.data, pkt.count, dig, 0) != 0) {
	    dig = pgpFreeDig(dig);
	}
	rpmtdFreeData(&pkt);
    }
    return dig;
}

static void deleteSigs(Header sigh)
{
    headerDel(sigh, RPMSIGTAG_GPG);
    headerDel(sigh, RPMSIGTAG_PGP);
    headerDel(sigh, RPMSIGTAG_DSA);
    headerDel(sigh, RPMSIGTAG_RSA);
    headerDel(sigh, RPMSIGTAG_PGP5);
}

static int sameSignature(rpmSigTag sigtag, Header h1, Header h2)
{
    pgpDig dig1 = getSig(h1, sigtag);
    pgpDig dig2 = getSig(h2, sigtag);
    int rc = 0; /* assume different, eg if either signature doesn't exist */

    /* XXX This part really belongs to rpmpgp.[ch] */
    if (dig1 && dig2) {
	pgpDigParams sig1 = &dig1->signature;
	pgpDigParams sig2 = &dig2->signature;

	/* XXX Should we compare something else too? */
	if (sig1->hash_algo != sig2->hash_algo)
	    goto exit;
	if (sig1->pubkey_algo != sig2->pubkey_algo)
	    goto exit;
	if (sig1->version != sig2->version)
	    goto exit;
	if (sig1->sigtype != sig2->sigtype)
	    goto exit;
	if (memcmp(sig1->signid, sig2->signid, sizeof(sig1->signid)) != 0)
	    goto exit;

	/* Parameters match, assume same signature */
	rc = 1;
    }

exit:
    pgpFreeDig(dig1);
    pgpFreeDig(dig2);
    return rc;
}

static int replaceSignature(Header sigh, const char *sigtarget,
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
    if (rpmGenSignature(sigh, sigtarget, passPhrase) == 0) {
	/* Lets see what we got and whether its the same signature as before */
	rpmSigTag sigtag = headerIsEntry(sigh, RPMSIGTAG_DSA) ?
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
    rpmlead lead;
    char *sigtarget = NULL, *trpm = NULL;
    Header sigh = NULL;
    char * msg;
    int res = -1; /* assume failure */
    int xx;
    
    {
    	rpmRC rc;
	struct rpmtd_s utd;

	fprintf(stdout, "%s:\n", rpm);

	if (manageFile(&fd, rpm, O_RDONLY))
	    goto exit;

	lead = rpmLeadNew();

	if ((rc = rpmLeadRead(fd, lead)) == RPMRC_OK) {
	    const char *lmsg = NULL;
	    rc = rpmLeadCheck(lead, &lmsg);
	    if (rc != RPMRC_OK) 
		rpmlog(RPMLOG_ERR, "%s: %s\n", rpm, lmsg);
	}

	if (rc != RPMRC_OK) {
	    lead = rpmLeadFree(lead);
	    goto exit;
	}

	msg = NULL;
	rc = rpmReadSignature(fd, &sigh, RPMSIGTYPE_HEADERSIG, &msg);
	switch (rc) {
	default:
	    rpmlog(RPMLOG_ERR, _("%s: rpmReadSignature failed: %s"), rpm,
			(msg && *msg ? msg : "\n"));
	    msg = _free(msg);
	    goto exit;
	    break;
	case RPMRC_OK:
	    if (sigh == NULL) {
		rpmlog(RPMLOG_ERR, _("%s: No signature available\n"), rpm);
		goto exit;
	    }
	    break;
	}
	msg = _free(msg);

	/* ASSERT: ofd == NULL && sigtarget == NULL */
	ofd = rpmMkTempFile(NULL, &sigtarget);
	if (ofd == NULL || Ferror(ofd)) {
	    rpmlog(RPMLOG_ERR, _("rpmMkTemp failed\n"));
	    goto exit;
	}
	/* Write the header and archive to a temp file */
	if (copyFile(&fd, rpm, &ofd, sigtarget))
	    goto exit;
	/* Both fd and ofd are now closed. sigtarget contains tempfile name. */
	/* ASSERT: fd == NULL && ofd == NULL */

	/* Dump the immutable region (if present). */
	if (headerGet(sigh, RPMTAG_HEADERSIGNATURES, &utd, HEADERGET_DEFAULT)) {
	    struct rpmtd_s copytd;
	    Header nh = headerNew();
	    Header oh = headerCopyLoad(utd.data);
	    HeaderIterator hi = headerInitIterator(oh);
	    while (headerNext(hi, &copytd)) {
		if (copytd.data)
		    xx = headerPut(nh, &copytd, HEADERPUT_DEFAULT);
		rpmtdFreeData(&copytd);
	    }
	    hi = headerFreeIterator(hi);
	    oh = headerFree(oh);

	    sigh = headerFree(sigh);
	    sigh = headerLink(nh);
	    nh = headerFree(nh);
	}

	/* Eliminate broken digest values. */
	xx = headerDel(sigh, RPMSIGTAG_BADSHA1_1);
	xx = headerDel(sigh, RPMSIGTAG_BADSHA1_2);

	/* Toss and recalculate header+payload size and digests. */
	{
	    rpmSigTag const sigs[] = { 	RPMSIGTAG_SIZE, 
					RPMSIGTAG_MD5,
				  	RPMSIGTAG_SHA1,
				     };
	    int nsigs = sizeof(sigs) / sizeof(rpmSigTag);
	    for (int i = 0; i < nsigs; i++) {
		(void) headerDel(sigh, sigs[i]);
		if (rpmGenDigest(sigh, sigtarget, sigs[i]))
		    goto exit;
	    }
	}

	if (deleting) {	/* Nuke all the signature tags. */
	    deleteSigs(sigh);
	} else {
	    res = replaceSignature(sigh, sigtarget, passPhrase);
	    if (res != 0) {
		if (res == 1) {
		    rpmlog(RPMLOG_WARNING,
			"%s already contains identical signature, skipping\n",
			rpm);
		    /* Identical signature is not an error */
		    res = 0;
		}
		goto exit;
	    }
	}

	/* Reallocate the signature into one contiguous region. */
	sigh = headerReload(sigh, RPMTAG_HEADERSIGNATURES);
	if (sigh == NULL)	/* XXX can't happen */
	    goto exit;

	rasprintf(&trpm, "%s.XXXXXX", rpm);
	ofd = rpmMkTemp(trpm);
	if (ofd == NULL || Ferror(ofd)) {
	    rpmlog(RPMLOG_ERR, _("rpmMkTemp failed\n"));
	    goto exit;
	}

	/* Write the lead/signature of the output rpm */
	rc = rpmLeadWrite(ofd, lead);
	lead = rpmLeadFree(lead);
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

	/* Append the header and archive from the temp file */
	/* ASSERT: fd == NULL && ofd != NULL */
	if (copyFile(&fd, sigtarget, &ofd, trpm))
	    goto exit;
	/* Both fd and ofd are now closed. */
	/* ASSERT: fd == NULL && ofd == NULL */

	/* Move final target into place, restore file permissions. */
	{
	    struct stat st;
	    xx = stat(rpm, &st);
	    xx = unlink(rpm);
	    xx = rename(trpm, rpm);
	    xx = chmod(rpm, st.st_mode);
	}
    }

    res = 0;

exit:
    if (fd)	(void) closeFile(&fd);
    if (ofd)	(void) closeFile(&ofd);

    sigh = rpmFreeSignature(sigh);

    /* Clean up intermediate target */
    if (sigtarget) {
	xx = unlink(sigtarget);
	sigtarget = _free(sigtarget);
    }
    if (trpm) {
	(void) unlink(trpm);
	free(trpm);
    }

    return res;
}

int rpmcliSign(ARGV_const_t argv, int deleting, const char *passPhrase)
{
    int res = 0;
    for (ARGV_const_t arg = argv; arg && *arg; arg++) {
	res += rpmSign(*arg, deleting, passPhrase);
    }
    return res;
}
