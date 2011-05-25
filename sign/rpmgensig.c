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
 * Validate generated signature and insert to header if it looks sane.
 * NSS doesn't support everything GPG does. Basic tests to see if the 
 * generated signature is something we can use.
 * Return 0 on success, 1 on failure.
 */
static int putSignature(Header sigh, int ishdr, uint8_t *pkt, size_t pktlen)
{
    pgpDig dig = pgpNewDig();
    pgpDigParams sigp = &dig->signature;
    rpmTagVal sigtag;
    struct rpmtd_s sigtd;
    int rc = 1; /* assume failure */

    if (pgpPrtPkts(pkt, pktlen, dig, 0) != 0)
	goto exit;

    if (rpmDigestLength(sigp->hash_algo) == 0) {
	rpmlog(RPMLOG_ERR, _("Unsupported PGP hash algorithm %d\n"),
	       sigp->hash_algo);
	goto exit;
    }

    switch (sigp->pubkey_algo) {
    case PGPPUBKEYALGO_DSA:
	sigtag = ishdr ? RPMSIGTAG_DSA : RPMSIGTAG_GPG;
	break;
    case PGPPUBKEYALGO_RSA:
	sigtag = ishdr ? RPMSIGTAG_RSA : RPMSIGTAG_PGP;
	break;
    default:
	rpmlog(RPMLOG_ERR, _("Unsupported PGP pubkey algorithm %d\n"),
		sigp->pubkey_algo);
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
    dig = pgpFreeDig(dig);
    return rc;
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
 * @param sigh		signature header
 * @param ishdr		header-only signature?
 * @param file		header+payload file name
 * @param passPhrase	private key pass phrase
 * @return		0 on success, 1 on failure
 */
static int makeGPGSignature(Header sigh, int ishdr,
			    const char * file, const char * passPhrase)
{
    char * sigfile = rstrscat(NULL, file, ".sig", NULL);
    struct stat st;
    uint8_t * pkt = NULL;
    size_t pktlen = 0;
    int rc = 1; /* assume failure */

    if (runGPG(file, sigfile, passPhrase))
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

/**
 * Generate header only signature(s) from a header+payload file.
 * @param sigh		signature header
 * @param file		header+payload file name
 * @param passPhrase	private key pass phrase
 * @return		0 on success, -1 on failure
 */
static int makeHDRSignature(Header sigh, const char * file,
		const char * passPhrase)
{
    Header h = NULL;
    FD_t fd = NULL;
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

    ret = makeGPGSignature(sigh, 1, fn, passPhrase);

exit:
    if (fn) {
	(void) unlink(fn);
	free(fn);
    }
    h = headerFree(h);
    if (fd != NULL) (void) Fclose(fd);
    return ret;
}

static int rpmGenSignature(Header sigh, const char * file,
		const char * passPhrase)
{
    int ret = -1;	/* assume failure. */

    if (makeGPGSignature(sigh, 0, file, passPhrase) == 0) {
	/* XXX Piggyback a header-only DSA/RSA signature as well. */
	ret = makeHDRSignature(sigh, file, passPhrase);
    }

    return ret;
}

/**
 * Retrieve signature from header tag
 * @param sigh		signature header
 * @param sigtag	signature tag
 * @return		parsed pgp dig or NULL
 */
static pgpDig getSig(Header sigh, rpmTagVal sigtag)
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

static int sameSignature(rpmTagVal sigtag, Header h1, Header h2)
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
    rpmlead lead;
    char *sigtarget = NULL, *trpm = NULL;
    Header sigh = NULL;
    char * msg;
    int res = -1; /* assume failure */
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

    ofd = rpmMkTempFile(NULL, &sigtarget);
    if (ofd == NULL || Ferror(ofd)) {
	rpmlog(RPMLOG_ERR, _("rpmMkTemp failed\n"));
	goto exit;
    }
    /* Write the header and archive to a temp file */
    if (copyFile(&fd, rpm, &ofd, sigtarget))
	goto exit;
    /* Both fd and ofd are now closed. sigtarget contains tempfile name. */

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
	hi = headerFreeIterator(hi);
	oh = headerFree(oh);

	sigh = headerFree(sigh);
	sigh = headerLink(nh);
	nh = headerFree(nh);
    }

    /* Eliminate broken digest values. */
    headerDel(sigh, RPMSIGTAG_BADSHA1_1);
    headerDel(sigh, RPMSIGTAG_BADSHA1_2);

    /* Toss and recalculate header+payload size and digests. */
    {
	rpmTagVal const sigs[] = { 	RPMSIGTAG_SIZE, 
				    RPMSIGTAG_MD5,
				    RPMSIGTAG_SHA1,
				 };
	int nsigs = sizeof(sigs) / sizeof(rpmTagVal);
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
		   _("%s already contains identical signature, skipping\n"),
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
    if (copyFile(&fd, sigtarget, &ofd, trpm) == 0) {
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

exit:
    if (fd)	(void) closeFile(&fd);
    if (ofd)	(void) closeFile(&ofd);

    sigh = rpmFreeSignature(sigh);

    /* Clean up intermediate target */
    if (sigtarget) {
	unlink(sigtarget);
	sigtarget = _free(sigtarget);
    }
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
