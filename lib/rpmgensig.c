/** \ingroup rpmcli
 * \file lib/rpmchecksig.c
 * Verify the signature of a package.
 */

#include "system.h"

#include <rpm/rpmlib.h>			/* RPMSIGTAG & related */
#include <rpm/rpmpgp.h>
#include <rpm/rpmcli.h>
#include <rpm/rpmfileutil.h>	/* rpmMkTemp() */
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>

#include "rpmio/digest.h"
#include "lib/rpmlead.h"
#include "lib/signature.h"

#include "debug.h"

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
     * rpmAddSignature() internals parse the actual signing result and 
     * use appropriate DSA/RSA tags regardless of what we pass from here.
     * RPMSIGTAG_GPG is only used to signal its an actual signature
     * and not just a digest we're adding, and says nothing
     * about the actual tags that gets created. 
     */
    if (rpmAddSignature(sigh, sigtarget, RPMSIGTAG_GPG, passPhrase) == 0) {
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
		if (rpmAddSignature(sigh, sigtarget, sigs[i], passPhrase))
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
