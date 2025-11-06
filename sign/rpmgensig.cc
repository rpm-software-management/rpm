/** \ingroup rpmcli
 * \file lib/rpmchecksig.c
 * Verify the signature of a package.
 */

#include "system.h"

#include <errno.h>
#include <sys/wait.h>
#include <popt.h>
#include <fcntl.h>
#ifdef WITH_FSVERITY
#include <libfsverity.h>
#include "rpmsignverity.hh"
#endif
#ifdef WITH_IMAEVM
#include "rpmsignfiles.hh"
#endif

#include <rpm/rpmbase64.h>
#include <rpm/rpmlib.h>			/* RPMSIGTAG & related */
#include <rpm/rpmmacro.h>
#include <rpm/rpmpgp.h>
#include <rpm/rpmsign.h>
#include <rpm/rpmfileutil.h>	/* rpmMkTemp() */
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include "rpmio_internal.hh"

#include "rpmlead.hh"
#include "signature.hh"
#include "rpmmacro_internal.hh"
#include "rpmvs.hh"

#include "debug.h"

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

static char ** signCmd(const char *sigfile)
{
    int argc = 0;
    char **argv = NULL;
    auto mctx = rpm::macros();
    auto [ ign, name ] = mctx.expand({"__", "%{_openpgp_sign}", "_sign_cmd"});
    const char * const margs[] = { "-", sigfile, NULL };

    auto [ rc, cmd ] = mctx.expand_this(name, (ARGV_const_t)margs, 0);
    if (rc) {
	rpmlog(RPMLOG_ERR, _("Expanding signing macro %s failed\n"),
		name.c_str());
	return NULL;
    }

    if (poptParseArgvString(cmd.c_str(), &argc, (const char ***)&argv) < 0 || argc < 2) {
	rpmlog(RPMLOG_ERR, _("Invalid sign command: %s\n"), cmd.c_str());
	argv = _free(argv);
    }

    return argv;
}

static int runGPG(sigTarget sigt, const char *sigfile)
{
    int pid = 0, status;
    int pipefd[2];
    FILE *fpipe = NULL;
    unsigned char buf[BUFSIZ];
    ssize_t count;
    ssize_t wantCount;
    rpm_loff_t size;
    int rc = 1; /* assume failure */
    char **argv = NULL;

    if ((argv = signCmd(sigfile)) == NULL)
	goto exit_nowait;

    if (pipe(pipefd) < 0) {
        rpmlog(RPMLOG_ERR, _("Could not create pipe for signing: %m\n"));
        goto exit_nowait;
    }

    if (!(pid = fork())) {
	/* GnuPG needs extra setup, try to see if that's what we're running */
	char *out = rpmExpand("%(", argv[0], " --version 2> /dev/null)", NULL);
	int using_gpg = (strstr(out, "GnuPG") != NULL);
	if (using_gpg) {
	    const char *tty = ttyname(STDIN_FILENO);

	    if (!getenv("GPG_TTY") && (!tty || setenv("GPG_TTY", tty, 0)))
		rpmlog(RPMLOG_WARNING, _("Could not set GPG_TTY to stdin: %m\n"));
	}
	free(out);

	dup2(pipefd[0], STDIN_FILENO);
	close(pipefd[1]);

	rc = execve(argv[0], argv, environ);

	rpmlog(RPMLOG_ERR, _("Could not exec %s: %s\n"), argv[0],
		strerror(errno));
	_exit(EXIT_FAILURE);
    }

    close(pipefd[0]);
    fpipe = fdopen(pipefd[1], "w");
    if (!fpipe) {
	rpmlog(RPMLOG_ERR, _("Could not open pipe for writing: %m\n"));
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

exit:

    if (fpipe)
	fclose(fpipe);
    if (pipefd[1])
	close(pipefd[1]);

    pid_t reaped;
    do {
        reaped = waitpid(pid, &status, 0);
    } while (reaped == -1 && errno == EINTR);

    if (reaped == -1) {
	rpmlog(RPMLOG_ERR, _("%s waitpid failed (%s)\n"), argv[0],
		strerror(errno));
    } else if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	rpmlog(RPMLOG_ERR, _("%s exec failed (%d)\n"), argv[0],
		WEXITSTATUS(status));
    } else {
	rc = 0;
    }

exit_nowait:
    free(argv);

    return rc;
}

/* Generate an OpenPGP signature(s) for a target */
static int makeGPGSignature(sigTarget sigt, uint8_t **pktp, size_t *lenp)
{
    char * sigfile = rstrscat(NULL, sigt->fileName, ".sig", NULL);
    struct stat st;
    uint8_t * pkt = NULL;
    size_t pktlen = 0;
    int rc = -1;

    if (runGPG(sigt, sigfile))
	goto exit;

    if (stat(sigfile, &st)) {
	/* External command failed to write signature */
	rpmlog(RPMLOG_ERR, _("failed to write signature: %s\n"), sigfile);
	goto exit;
    }

    pktlen = st.st_size;
    rpmlog(RPMLOG_DEBUG, "OpenPGP sig size: %zd\n", pktlen);
    pkt = (uint8_t *)xmalloc(pktlen);

    {	FD_t fd;
	size_t nb = 0;

	fd = Fopen(sigfile, "r.ufdio");
	if (fd != NULL && !Ferror(fd)) {
	    nb = Fread(pkt, sizeof(*pkt), pktlen, fd);
	    (void) Fclose(fd);
	}
	if (nb != pktlen) {
	    rpmlog(RPMLOG_ERR, _("unable to read the signature: %s\n"),
			sigfile);
	    pkt = _free(pkt);
	    goto exit;
	}
    }

    rpmlog(RPMLOG_DEBUG, "Got %zd bytes of OpenPGP sig\n", pktlen);

    *pktp = pkt;
    *lenp = pktlen;
    rc = 0;

exit:
    (void) unlink(sigfile);
    free(sigfile);

    return rc;
}

static void deleteSigs(Header sigh)
{
    headerDel(sigh, RPMSIGTAG_GPG);
    headerDel(sigh, RPMSIGTAG_PGP);
    headerDel(sigh, RPMSIGTAG_DSA);
    headerDel(sigh, RPMSIGTAG_RSA);
    headerDel(sigh, RPMSIGTAG_PGP5);
    headerDel(sigh, RPMSIGTAG_OPENPGP);
}

static void deleteFileSigs(Header sigh)
{
    headerDel(sigh, RPMSIGTAG_FILESIGNATURELENGTH);
    headerDel(sigh, RPMSIGTAG_FILESIGNATURES);
    headerDel(sigh, RPMSIGTAG_VERITYSIGNATURES);
    headerDel(sigh, RPMSIGTAG_VERITYSIGNATUREALGO);
}

static pgpDigParams tdParams(rpmtd td)
{
    pgpDigParams sig = NULL;
    if (td->tag == RPMTAG_OPENPGP) {
	uint8_t *pkt = NULL;
	size_t pktlen = 0;
	if (rpmBase64Decode(rpmtdGetString(td), (void **)&pkt, &pktlen) == 0) {
	    pgpPrtParams(pkt, pktlen, PGPTAG_SIGNATURE, &sig);
	    free(pkt);
	}
    } else {
	pgpPrtParams((uint8_t *)td->data, td->count, PGPTAG_SIGNATURE, &sig);
    }
    return sig;
}

static int haveSignature(rpmtd sigtd, Header sigh)
{
    struct rpmtd_s oldtd;
    int rc = 0; /* assume no */

    if (!headerGet(sigh, sigtd->tag, &oldtd, HEADERGET_DEFAULT))
	return rc;

    pgpDigParams newsig = tdParams(sigtd);
    while (rpmtdNext(&oldtd) >= 0 && rc == 0) {
	pgpDigParams oldsig = tdParams(&oldtd);
	if (pgpDigParamsCmp(newsig, oldsig) == 0)
	    rc = 1;
	pgpDigParamsFree(oldsig);
    }
    pgpDigParamsFree(newsig);
    rpmtdFreeData(&oldtd);

    return rc;
}

static int haveLegacySig(Header sigh, int ishdr)
{
    return headerIsEntry(sigh, ishdr ? RPMSIGTAG_RSA : RPMSIGTAG_PGP) ||
	   headerIsEntry(sigh, ishdr ? RPMSIGTAG_DSA : RPMSIGTAG_GPG);
}

static int putSignature(Header sigh, uint8_t *pkt, size_t pktlen,
			int multisig, int ishdr, rpmSignFlags flags)
{
    int rc = -1;
    unsigned int hash_algo = 0;
    int ver = 0;
    pgpDigParams sigp = NULL;

    if (pgpPrtParams(pkt, pktlen, PGPTAG_SIGNATURE, &sigp)) {
	rpmlog(RPMLOG_ERR, _("Unsupported OpenPGP signature\n"));
	goto exit;
    }

    hash_algo = pgpDigParamsAlgo(sigp, PGPVAL_HASHALGO);
    if (rpmDigestLength(hash_algo) == 0) {
	rpmlog(RPMLOG_ERR, _("Unsupported OpenPGP hash algorithm %u\n"),
		hash_algo);
	goto exit;
    }

    ver = pgpDigParamsVersion(sigp);
    if (ver < 4) {
	rpmlog(RPMLOG_WARNING, _("Deprecated OpenPGP signature version %d\n"),
	    ver);
    }

    if (multisig) {
	char *b64 = rpmBase64Encode(pkt, pktlen, 0);
	char **arr = (char **)xmalloc(1 * sizeof(*arr));
	arr[0] = b64;

	rpmtd_s mtd = {
	    .tag = RPMSIGTAG_OPENPGP,
	    .type = RPM_STRING_ARRAY_TYPE,
	    .count = 1,
	    .data = arr,
	    .flags = RPMTD_ALLOCED|RPMTD_PTR_ALLOCED,
	    .ix = -1,
	    .size = 0,
	};
	if (haveSignature(&mtd, sigh)) {
	    rc = 1;
	} else {
	    rc = (headerPut(sigh, &mtd, HEADERPUT_APPEND) == 0) ? -1 : 0;
	}
	rpmtdFreeData(&mtd);
    } else {
	unsigned int pubkey_algo = pgpDigParamsAlgo(sigp, PGPVAL_PUBKEYALGO);
	uint32_t sigtag = 0;
	switch (pubkey_algo) {
	case PGPPUBKEYALGO_DSA:
	case PGPPUBKEYALGO_ECDSA:
	case PGPPUBKEYALGO_EDDSA:
	    sigtag = ishdr ? RPMSIGTAG_DSA : RPMSIGTAG_GPG;
	    break;
	case PGPPUBKEYALGO_RSA:
	    sigtag = ishdr ? RPMSIGTAG_RSA : RPMSIGTAG_PGP;
	    break;
	default:
	    break;
	}

	if (sigtag && ver <= 4) {
	    struct rpmtd_s sigtd = {
		.tag = sigtag,
		.type = RPM_BIN_TYPE,
		.count = (uint32_t)pktlen,
		.data = pkt,
		.flags = 0,
		.ix = -1,
		.size = 0,
	    };

	    if (haveSignature(&sigtd, sigh)) {
		rc = 1;
	    } else if (haveLegacySig(sigh, ishdr)) {
		rc = 2;
	    } else {
		rc = (headerPut(sigh, &sigtd, HEADERPUT_DEFAULT) == 0) ? -1 : 0;
	    }

	    /* Legacy signatures are best-effort in v6 mode */
	    if ((flags & RPMSIGN_FLAG_RPMV6) && rc > 0) {
		rc = 0;
		goto exit;
	    }
	} else {
	    /* If we did a v6 signature, we can ignore the error here */
	    if (flags & RPMSIGN_FLAG_RPMV6) {
		rc = 0;
		goto exit;
	    }

	    if (sigtag == 0) {
		rpmlog(RPMLOG_ERR,
		    _("Unsupported OpenPGP pubkey algorithm %u for rpm v3/v4 signatures\n"),
		    pubkey_algo);
		goto exit;
	    }
	    if (ver > 4) {
		rpmlog(RPMLOG_ERR,
		    ("Unsupported OpenPGP version %u for rpm v3/v4 signatures\n"),
		    ver);
		goto exit;
	    }
	}
    }

exit:
    pgpDigParamsFree(sigp);
    return rc;
}

static int addSignature(Header sigh, rpmSignFlags flags,
			sigTarget sigt_v3, sigTarget sigt_v4)
{
    int rc = -1;
    uint8_t *pkt = NULL;
    size_t pktlen = 0;
    
    /* Make a header signature */
    if (makeGPGSignature(sigt_v4, &pkt, &pktlen))
	goto exit;

    /* Add a v6 signature if requested */
    if (flags & RPMSIGN_FLAG_RPMV6)
	if ((rc = putSignature(sigh, pkt, pktlen, 1, 1, flags)))
	    goto exit;

    /* Add a v4 signature if requested */
    if (flags & RPMSIGN_FLAG_RPMV4) {
	if ((rc = putSignature(sigh, pkt, pktlen, 0, 1, flags)))
	    goto exit;

	/* Only consider v3 signature if also adding v4 */
	if (flags & RPMSIGN_FLAG_RPMV3) {
	    pkt = _free(pkt);

	    /* Assume the same signature test holds for v3 signature too */
	    if (makeGPGSignature(sigt_v3, &pkt, &pktlen))
		goto exit;

	    if ((rc = putSignature(sigh, pkt, pktlen, 0, 0, flags)))
		goto exit;
	}
    }


    rc = 0;
exit:
    free(pkt);
    return rc;
}

static void unloadImmutableRegion(Header *hdrp, rpmTagVal tag)
{
    struct rpmtd_s td;
    Header oh = NULL;

    if (headerGet(*hdrp, tag, &td, HEADERGET_DEFAULT)) {
	oh = headerImport(td.data, td.count, HEADERIMPORT_COPY);
	rpmtdFreeData(&td);
    } else {
	/* XXX should we warn if the immutable region is corrupt/missing? */
	oh = headerLink(*hdrp);
    }

    if (oh) {
	/* Perform a copy to eliminate crud from buggy signing tools etc */
	Header nh = headerCopy(oh);
	headerFree(*hdrp);
	*hdrp = headerLink(nh);
	headerFree(nh);
	headerFree(oh);
    }
}

static rpmRC includeFileSignatures(Header *sigp, Header *hdrp)
{
#ifdef WITH_IMAEVM
    rpmRC rc;
    char *key = rpmExpand("%{?_file_signing_key}", NULL);
    char *keypass = rpmExpand("%{?_file_signing_key_password}", NULL);

    if (rstreq(keypass, "")) {
	free(keypass);
	keypass = NULL;
    }

    rc = rpmSignFiles(*sigp, *hdrp, key, keypass);

    free(keypass);
    free(key);
    return rc;
#else
    rpmlog(RPMLOG_ERR, _("file signing support not built in\n"));
    return RPMRC_FAIL;
#endif
}

static rpmRC includeVeritySignatures(FD_t fd, Header *sigp, Header *hdrp)
{
#ifdef WITH_FSVERITY
    rpmRC rc = RPMRC_OK;
    char *key = rpmExpand("%{?_file_signing_key}", NULL);
    char *keypass = rpmExpand("%{?_file_signing_key_password}", NULL);
    char *cert = rpmExpand("%{?_file_signing_cert}", NULL);
    char *algorithm = rpmExpand("%{?_verity_algorithm}", NULL);
    uint16_t algo = 0;

    if (rstreq(keypass, "")) {
	free(keypass);
	keypass = NULL;
    }

    if (algorithm && strlen(algorithm) > 0) {
	    algo = libfsverity_find_hash_alg_by_name(algorithm);
	    rpmlog(RPMLOG_DEBUG, _("Searching for algorithm %s got %i\n"),
		   algorithm, algo);
	    if (!algo) {
		    rpmlog(RPMLOG_ERR, _("Unsupported fsverity algorithm %s\n"),
			   algorithm);
		    rc = RPMRC_FAIL;
		    goto out;
	    }
    }
    if (key && cert) {
	    rc = rpmSignVerity(fd, *sigp, *hdrp, key, keypass, cert, algo);
    } else {
	rpmlog(RPMLOG_ERR, _("fsverity signatures requires a key and a cert\n"));
	rc = RPMRC_FAIL;
    }

 out:
    free(keypass);
    free(key);
    free(cert);
    return rc;
#else
    rpmlog(RPMLOG_ERR, _("fsverity signing support not built in\n"));
    return RPMRC_FAIL;
#endif
}

static int msgCb(struct rpmsinfo_s *sinfo, void *cbdata)
{
    char **msg = (char **)cbdata;
    if (sinfo->rc && sinfo->rc != RPMRC_NOTFOUND && *msg == NULL)
	*msg = rpmsinfoMsg(sinfo);
    return 1;
}

/* Require valid digests on entire package for signing. */
static rpmRC checkPkg(FD_t fd, char **msg)
{
    rpmRC rc;
    struct rpmvs_s *vs = rpmvsCreate(RPMSIG_DIGEST_TYPE, 0, NULL);
    off_t offset = Ftell(fd);

    Fseek(fd, 0, SEEK_SET);
    rc = rpmpkgRead(vs, fd, NULL, NULL, msg);
    if (!rc) {
	rc = rpmvsVerify(vs, RPMSIG_DIGEST_TYPE, msgCb, msg) ?
			RPMRC_FAIL : RPMRC_OK;
    }
    Fseek(fd, offset, SEEK_SET);
    Fseek(fd, offset, SEEK_SET);

    rpmvsFree(vs);
    return rc;
}

/** \ingroup rpmcli
 * Create/modify elements in signature header.
 * @param rpm		path to package
 * @param deleting	adding or deleting signature?
 * @param flags
 * @return		0 on success, -1 on error
 */
static int rpmSign(const char *rpm, int deleting, int flags)
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
    off_t fileSize;
    off_t headerStart;
    off_t sigStart;
    struct sigTarget_s sigt_v3;
    struct sigTarget_s sigt_v4;
    unsigned int origSigSize;
    int insSig = 0;
    int rpmformat = 0;
    rpmTagVal reserveTag = 0;

    if (manageFile(&fd, rpm, O_RDWR))
	goto exit;

    /* Ensure package is intact before attempting to sign */
    if ((rc = checkPkg(fd, &msg))) {
	rpmlog(RPMLOG_ERR, "not signing corrupt package %s: %s\n", rpm, msg);
	goto exit;
    }

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

    rpmformat = headerGetNumber(h, RPMTAG_RPMFORMAT);

    if (rpmformat < 4) {
	rpmlog(RPMLOG_ERR, _("Cannot sign RPM v3 packages: %s\n"), rpm);
	goto exit;
    }

    if (rpmformat >= 6) {
	flags |= RPMSIGN_FLAG_RPMV6;
	reserveTag = RPMSIGTAG_RESERVED;
	/* v3 signatures are not welcome in v6 packages */
	if (flags & RPMSIGN_FLAG_RPMV3) {
	    rpmlog(RPMLOG_WARNING,
		_("not generating v3 signature for v6 package: %s\n"), rpm);
	    flags &= ~RPMSIGN_FLAG_RPMV3;
	}
    } else {
	flags |= RPMSIGN_FLAG_RPMV4;
	reserveTag = RPMSIGTAG_RESERVEDSPACE;
	/* Always add V3 signatures if no payload digest present */
	if (!(headerIsEntry(h, RPMTAG_PAYLOADSHA256) ||
	      headerIsEntry(h, RPMTAG_PAYLOADSHA256ALT))) {
	    flags |= RPMSIGN_FLAG_RPMV3;
	}
    }

    if (headerIsSource(h)) {
	rpmlog(RPMLOG_DEBUG,
	    _("File signatures not applicable to src.rpm: %s\n"), rpm);
	flags &= ~(RPMSIGN_FLAG_IMA | RPMSIGN_FLAG_FSVERITY);
    }

    /* Adjust for the region index entry + data getting stripped: 32 bytes */
    origSigSize = headerSizeof(sigh, HEADER_MAGIC_YES) - 32;
    unloadImmutableRegion(&sigh, RPMTAG_HEADERSIGNATURES);

    if (flags & RPMSIGN_FLAG_IMA) {
	if (includeFileSignatures(&sigh, &h))
	    goto exit;
    }

    if (flags & RPMSIGN_FLAG_FSVERITY) {
	if (includeVeritySignatures(fd, &sigh, &h))
	    goto exit;
    }

    if (deleting == 2) {	/* Nuke IMA + fsverity file signature tags. */
	deleteFileSigs(sigh);
    } else if (deleting) {	/* Nuke all the signature tags. */
	deleteSigs(sigh);
    } else {
	fileSize = fdSize(fd);
	if (fileSize < 0) {
	    rpmlog(RPMLOG_ERR, _("Could not get a file size of %s\n"), rpm);
	    goto exit;
	}
	/* Signature target containing header + payload */
	sigt_v3.fd = fd;
	sigt_v3.start = headerStart;
	sigt_v3.fileName = rpm;
	sigt_v3.size = fileSize - headerStart;

	/* Signature target containing only header */
	sigt_v4 = sigt_v3;
	sigt_v4.size = headerSizeof(h, HEADER_MAGIC_YES);

	if (flags & RPMSIGN_FLAG_RESIGN)
	    deleteSigs(sigh);

	res = addSignature(sigh, flags, &sigt_v3, &sigt_v4);
	if (res != 0) {
	    if (res == 1) {
		rpmlog(RPMLOG_WARNING,
		   _("%s already contains an identical signature, skipping\n"),
		   rpm);
		/* Identical signature is not an error */
		res = 0;
	    }
	    if (res == 2) {
		rpmlog(RPMLOG_ERR,
		   _("%s already contains a legacy signature\n"),
		   rpm);
		/* Existing signature is an error */
		res = -1;
	    }
	    goto exit;
	}
	res = -1;
    }

    /* Adjust reserved size for added/removed signatures */
    if (headerGet(sigh, reserveTag, &utd, HEADERGET_MINMEM)) {
	unsigned newSize = headerSizeof(sigh, HEADER_MAGIC_YES);
	int diff = origSigSize - newSize;

	/* The header doesn't support zero-sized data */
	if ((diff < 0 && abs(diff) < static_cast<int>(utd.count)) || diff > 0) {
	    utd.count += diff;
	    uint8_t *zeros = (uint8_t *)xcalloc(utd.count, sizeof(*zeros));
	    utd.data = zeros;
	    headerMod(sigh, &utd);
	    free(zeros);
	    insSig = 1;
	}
    }

    /* Reallocate the signature into one contiguous region. */
    sigh = headerReload(sigh, RPMTAG_HEADERSIGNATURES);
    if (sigh == NULL)	/* XXX can't happen */
	goto exit;

    if (Fseek(fd, insSig ? sigStart : 0, SEEK_SET) < 0) {
	rpmlog(RPMLOG_ERR, _("Could not seek in file %s: %s\n"),
		rpm, Fstrerror(fd));
	goto exit;
    }

    if (insSig) {
	/* Insert new signature into original rpm */
	if (rpmWriteSignature(fd, sigh)) {
	    rpmlog(RPMLOG_ERR, _("%s: rpmWriteSignature failed: %s\n"), rpm,
		Fstrerror(fd));
	    goto exit;
	}
	res = 0;
    } else {
	char lbuf[RPMLEAD_SIZE];
	/* Replace orignal rpm with new rpm containing new signature */
	rasprintf(&trpm, "%s.XXXXXX", rpm);
	ofd = rpmMkTemp(trpm);
	if (ofd == NULL || Ferror(ofd)) {
	    rpmlog(RPMLOG_ERR, _("rpmMkTemp failed: %s\n"), trpm);
	    goto exit;
	}

	/* Copy lead from original */
	if (!(Fread(lbuf, 1, RPMLEAD_SIZE, fd) == RPMLEAD_SIZE &&
	      Fwrite(lbuf, 1, RPMLEAD_SIZE, ofd) == RPMLEAD_SIZE))
	{
	    rpmlog(RPMLOG_ERR, _("%s: failed to copy rpm lead: %s\n"),
				trpm, Fstrerror(ofd));
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

	    /* File must be closed before deletion due to different file locking in some file systems*/
	    if (fd) (void) closeFile(&fd);
	    if (ofd) (void) closeFile(&ofd);

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
	    rpmPushMacro(NULL, "_openpgp_sign_id", NULL, args->keyid, RMIL_GLOBAL);
	}
    }


    rc = rpmSign(path, 0, args ? args->signflags : 0);

    if (args) {
	if (args->hashalgo) {
	    rpmPopMacro(NULL, "_gpg_digest_algo");
	}
	if (args->keyid) {
	    rpmPopMacro(NULL, "_openpgp_sign_id");
	}
    }

    return rc;
}

int rpmPkgDelSign(const char *path, const struct rpmSignArgs * args)
{
    return rpmSign(path, 1, 0);
}

int rpmPkgDelFileSign(const char *path, const struct rpmSignArgs * args)
{
    return rpmSign(path, 2, 0);
}
