/** \ingroup signature
 * \file lib/signature.c
 */

#include "system.h"

#include "rpmio_internal.h"
#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX for rpmGetPath() */

#include "misc.h"	/* XXX for dosetenv() and makeTempFile() */
#include "legacy.h"	/* XXX for mdbinfile() */
#include "rpmlead.h"
#include "signature.h"
#include "debug.h"

/*@access Header@*/		/* XXX compared with NULL */
/*@access FD_t@*/		/* XXX compared with NULL */
/*@access pgpDig@*/

/*@-mustmod@*/ /* FIX: internalState not modified? */
int rpmLookupSignatureType(int action)
{
    static int disabled = 0;
    int rc = 0;

    switch (action) {
    case RPMLOOKUPSIG_DISABLE:
	disabled = -2;
	break;
    case RPMLOOKUPSIG_ENABLE:
	disabled = 0;
	/*@fallthrough@*/
    case RPMLOOKUPSIG_QUERY:
	if (disabled)
	    break;	/* Disabled */
      { const char *name = rpmExpand("%{_signature}", NULL);
	if (!(name && *name != '%'))
	    rc = 0;
	else if (!xstrcasecmp(name, "none"))
	    rc = 0;
	else if (!xstrcasecmp(name, "pgp"))
	    rc = RPMSIGTAG_PGP;
	else if (!xstrcasecmp(name, "pgp5"))	/* XXX legacy */
	    rc = RPMSIGTAG_PGP;
	else if (!xstrcasecmp(name, "gpg"))
	    rc = RPMSIGTAG_GPG;
	else
	    rc = -1;	/* Invalid %_signature spec in macro file */
	name = _free(name);
      }	break;
    }
    return rc;
}
/*@=mustmod@*/

/* rpmDetectPGPVersion() returns the absolute path to the "pgp"  */
/* executable of the requested version, or NULL when none found. */

const char * rpmDetectPGPVersion(pgpVersion * pgpVer)
{
    /* Actually this should support having more then one pgp version. */
    /* At the moment only one version is possible since we only       */
    /* have one %_pgpbin and one %_pgp_path.                          */

    static pgpVersion saved_pgp_version = PGP_UNKNOWN;
    const char *pgpbin = rpmGetPath("%{_pgpbin}", NULL);

    if (saved_pgp_version == PGP_UNKNOWN) {
	char *pgpvbin;
	struct stat st;

	if (!(pgpbin && pgpbin[0] != '%')) {
	  pgpbin = _free(pgpbin);
	  saved_pgp_version = -1;
	  return NULL;
	}
	pgpvbin = (char *)alloca(strlen(pgpbin) + sizeof("v"));
	(void)stpcpy(stpcpy(pgpvbin, pgpbin), "v");

	if (stat(pgpvbin, &st) == 0)
	  saved_pgp_version = PGP_5;
	else if (stat(pgpbin, &st) == 0)
	  saved_pgp_version = PGP_2;
	else
	  saved_pgp_version = PGP_NOTDETECTED;
    }

    if (pgpVer && pgpbin)
	*pgpVer = saved_pgp_version;
    return pgpbin;
}

/**
 * Check package size.
 * @todo rpmio: use fdSize rather than fstat(2) to get file size.
 * @param fd			package file handle
 * @param siglen		signature header size
 * @param pad			signature padding
 * @param datalen		length of header+payload
 * @return 			rpmRC return code
 */
static inline rpmRC checkSize(FD_t fd, int siglen, int pad, int datalen)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    struct stat st;
    rpmRC rc;

    if (fstat(Fileno(fd), &st))
	return RPMRC_FAIL;

    if (!S_ISREG(st.st_mode)) {
	rpmMessage(RPMMESS_DEBUG,
	    _("file is not regular -- skipping size check\n"));
	return RPMRC_OK;
    }

    /*@-sizeoftype@*/
    rc = (((sizeof(struct rpmlead) + siglen + pad + datalen) - st.st_size)
	? RPMRC_BADSIZE : RPMRC_OK);

    rpmMessage((rc == RPMRC_OK ? RPMMESS_DEBUG : RPMMESS_WARNING),
	_("Expected size: %12d = lead(%d)+sigs(%d)+pad(%d)+data(%d)\n"),
		(int)sizeof(struct rpmlead)+siglen+pad+datalen,
		(int)sizeof(struct rpmlead), siglen, pad, datalen);
    /*@=sizeoftype@*/
    rpmMessage((rc == RPMRC_OK ? RPMMESS_DEBUG : RPMMESS_WARNING),
	_("  Actual size: %12d\n"), (int)st.st_size);

    return rc;
}

rpmRC rpmReadSignature(FD_t fd, Header * headerp, sigType sig_type)
{
    byte buf[2048];
    int sigSize, pad;
    int_32 type, count;
    int_32 *archSize;
    Header h = NULL;
    rpmRC rc = RPMRC_FAIL;		/* assume failure */

    if (headerp)
	*headerp = NULL;

    buf[0] = 0;
    switch (sig_type) {
    case RPMSIGTYPE_NONE:
	rpmMessage(RPMMESS_DEBUG, _("No signature\n"));
	rc = RPMRC_OK;
	break;
    case RPMSIGTYPE_PGP262_1024:
	rpmMessage(RPMMESS_DEBUG, _("Old PGP signature\n"));
	/* These are always 256 bytes */
	/*@-type@*/ /* FIX: eliminate timedRead @*/
	if (timedRead(fd, buf, 256) != 256)
	    break;
	/*@=type@*/
	h = headerNew();
	(void) headerAddEntry(h, RPMSIGTAG_PGP, RPM_BIN_TYPE, buf, 152);
	rc = RPMRC_OK;
	break;
    case RPMSIGTYPE_MD5:
    case RPMSIGTYPE_MD5_PGP:
	rpmError(RPMERR_BADSIGTYPE,
	      _("Old (internal-only) signature!  How did you get that!?\n"));
	break;
    case RPMSIGTYPE_HEADERSIG:
    case RPMSIGTYPE_DISABLE:
	/* This is a new style signature */
	h = headerRead(fd, HEADER_MAGIC_YES);
	if (h == NULL)
	    break;

	rc = RPMRC_OK;
	sigSize = headerSizeof(h, HEADER_MAGIC_YES);

	/* XXX Legacy headers have a HEADER_IMAGE tag added. */
	if (headerIsEntry(h, RPMTAG_HEADERIMAGE))
	    sigSize -= (16 + 16);

	pad = (8 - (sigSize % 8)) % 8; /* 8-byte pad */
	if (sig_type == RPMSIGTYPE_HEADERSIG) {
	    if (! headerGetEntry(h, RPMSIGTAG_SIZE, &type,
				(void **)&archSize, &count))
		break;
	    rc = checkSize(fd, sigSize, pad, *archSize);
	}
	/*@-type@*/ /* FIX: eliminate timedRead @*/
	if (pad && timedRead(fd, buf, pad) != pad)
	    rc = RPMRC_SHORTREAD;
	/*@=type@*/
	break;
    default:
	break;
    }

    if (headerp && rc == 0)
	*headerp = h;
    else
	h = headerFree(h);

    return rc;
}

int rpmWriteSignature(FD_t fd, Header h)
{
    static byte buf[8] = "\000\000\000\000\000\000\000\000";
    int sigSize, pad;
    int rc;

    rc = headerWrite(fd, h, HEADER_MAGIC_YES);
    if (rc)
	return rc;

    sigSize = headerSizeof(h, HEADER_MAGIC_YES);
    pad = (8 - (sigSize % 8)) % 8;
    if (pad) {
	if (Fwrite(buf, sizeof(buf[0]), pad, fd) != pad)
	    rc = 1;
    }
    rpmMessage(RPMMESS_DEBUG, _("Signature: size(%d)+pad(%d)\n"), sigSize, pad);
    return rc;
}

Header rpmNewSignature(void)
{
    Header h = headerNew();
    return h;
}

Header rpmFreeSignature(Header h)
{
    return headerFree(h);
}

static int makePGPSignature(const char * file, /*@out@*/ void ** sig,
		/*@out@*/ int_32 * size, /*@null@*/ const char * passPhrase)
	/*@globals rpmGlobalMacroContext,
		fileSystem @*/
	/*@modifies *sig, *size, rpmGlobalMacroContext,
		fileSystem @*/
{
    char * sigfile = alloca(1024);
    int pid, status;
    int inpipe[2];
    struct stat st;

    (void) stpcpy( stpcpy(sigfile, file), ".sig");

    inpipe[0] = inpipe[1] = 0;
    (void) pipe(inpipe);

    if (!(pid = fork())) {
	const char *pgp_path = rpmExpand("%{_pgp_path}", NULL);
	const char *name = rpmExpand("+myname=\"%{_pgp_name}\"", NULL);
	const char *path;
	pgpVersion pgpVer;

	(void) close(STDIN_FILENO);
	(void) dup2(inpipe[0], 3);
	(void) close(inpipe[1]);

	(void) dosetenv("PGPPASSFD", "3", 1);
	if (pgp_path && *pgp_path != '%')
	    (void) dosetenv("PGPPATH", pgp_path, 1);

	/* dosetenv("PGPPASS", passPhrase, 1); */

	if ((path = rpmDetectPGPVersion(&pgpVer)) != NULL) {
	    switch(pgpVer) {
	    case PGP_2:
		(void) execlp(path, "pgp", "+batchmode=on", "+verbose=0", "+armor=off",
		    name, "-sb", file, sigfile, NULL);
		break;
	    case PGP_5:
		(void) execlp(path,"pgps", "+batchmode=on", "+verbose=0", "+armor=off",
		    name, "-b", file, "-o", sigfile, NULL);
		break;
	    case PGP_UNKNOWN:
	    case PGP_NOTDETECTED:
		break;
	    }
	}
	rpmError(RPMERR_EXEC, _("Couldn't exec pgp (%s)\n"),
			(path ? path : NULL));
	_exit(RPMERR_EXEC);
    }

    (void) close(inpipe[0]);
    if (passPhrase)
	(void) write(inpipe[1], passPhrase, strlen(passPhrase));
    (void) write(inpipe[1], "\n", 1);
    (void) close(inpipe[1]);

    (void)waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	rpmError(RPMERR_SIGGEN, _("pgp failed\n"));
	return 1;
    }

    if (stat(sigfile, &st)) {
	/* PGP failed to write signature */
	if (sigfile) (void) unlink(sigfile);  /* Just in case */
	rpmError(RPMERR_SIGGEN, _("pgp failed to write signature\n"));
	return 1;
    }

    *size = st.st_size;
    rpmMessage(RPMMESS_DEBUG, _("PGP sig size: %d\n"), *size);
    *sig = xmalloc(*size);

    {	FD_t fd;
	int rc = 0;
	fd = Fopen(sigfile, "r.fdio");
	if (fd != NULL && !Ferror(fd)) {
	    /*@-type@*/ /* FIX: eliminate timedRead @*/
	    rc = timedRead(fd, *sig, *size);
	    /*@=type@*/
	    if (sigfile) (void) unlink(sigfile);
	    (void) Fclose(fd);
	}
	if (rc != *size) {
	    *sig = _free(*sig);
	    rpmError(RPMERR_SIGGEN, _("unable to read the signature\n"));
	    return 1;
	}
    }

    rpmMessage(RPMMESS_DEBUG, _("Got %d bytes of PGP sig\n"), *size);

    return 0;
}

/* This is an adaptation of the makePGPSignature function to use GPG instead
 * of PGP to create signatures.  I think I've made all the changes necessary,
 * but this could be a good place to start looking if errors in GPG signature
 * creation crop up.
 */
static int makeGPGSignature(const char * file, /*@out@*/ void ** sig,
		/*@out@*/ int_32 * size, /*@null@*/ const char * passPhrase)
	/*@globals rpmGlobalMacroContext,
		fileSystem @*/
	/*@modifies *sig, *size, rpmGlobalMacroContext,
		fileSystem @*/
{
    char * sigfile = alloca(1024);
    int pid, status;
    int inpipe[2];
    FILE * fpipe;
    struct stat st;

    (void) stpcpy( stpcpy(sigfile, file), ".sig");

    inpipe[0] = inpipe[1] = 0;
    (void) pipe(inpipe);

    if (!(pid = fork())) {
	const char *gpg_path = rpmExpand("%{_gpg_path}", NULL);
	const char *name = rpmExpand("%{_gpg_name}", NULL);

	(void) close(STDIN_FILENO);
	(void) dup2(inpipe[0], 3);
	(void) close(inpipe[1]);

	if (gpg_path && *gpg_path != '%')
	    (void) dosetenv("GNUPGHOME", gpg_path, 1);
	(void) execlp("gpg", "gpg",
	       "--batch", "--no-verbose", "--no-armor", "--passphrase-fd", "3",
	       "-u", name, "-sbo", sigfile, file,
	       NULL);
	rpmError(RPMERR_EXEC, _("Couldn't exec gpg\n"));
	_exit(RPMERR_EXEC);
    }

    fpipe = fdopen(inpipe[1], "w");
    (void) close(inpipe[0]);
    if (fpipe) {
	fprintf(fpipe, "%s\n", (passPhrase ? passPhrase : ""));
	(void) fclose(fpipe);
    }

    (void)waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	rpmError(RPMERR_SIGGEN, _("gpg failed\n"));
	return 1;
    }

    if (stat(sigfile, &st)) {
	/* GPG failed to write signature */
	if (sigfile) (void) unlink(sigfile);  /* Just in case */
	rpmError(RPMERR_SIGGEN, _("gpg failed to write signature\n"));
	return 1;
    }

    *size = st.st_size;
    rpmMessage(RPMMESS_DEBUG, _("GPG sig size: %d\n"), *size);
    *sig = xmalloc(*size);

    {	FD_t fd;
	int rc = 0;
	fd = Fopen(sigfile, "r.fdio");
	if (fd != NULL && !Ferror(fd)) {
	    /*@-type@*/ /* FIX: eliminate timedRead @*/
	    rc = timedRead(fd, *sig, *size);
	    /*@=type@*/
	    if (sigfile) (void) unlink(sigfile);
	    (void) Fclose(fd);
	}
	if (rc != *size) {
	    *sig = _free(*sig);
	    rpmError(RPMERR_SIGGEN, _("unable to read the signature\n"));
	    return 1;
	}
    }

    rpmMessage(RPMMESS_DEBUG, _("Got %d bytes of GPG sig\n"), *size);

    return 0;
}

int rpmAddSignature(Header h, const char * file, int_32 sigTag,
		const char *passPhrase)
{
    struct stat st;
    int_32 size;
    byte buf[16];
    void *sig;
    int ret = -1;

    switch (sigTag) {
    case RPMSIGTAG_SIZE:
	(void) stat(file, &st);
	size = st.st_size;
	ret = 0;
	(void) headerAddEntry(h, RPMSIGTAG_SIZE, RPM_INT32_TYPE, &size, 1);
	break;
    case RPMSIGTAG_MD5:
	ret = mdbinfile(file, buf);
	if (ret == 0)
	    (void) headerAddEntry(h, sigTag, RPM_BIN_TYPE, buf, 16);
	break;
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
	rpmMessage(RPMMESS_VERBOSE, _("Generating signature using PGP.\n"));
	ret = makePGPSignature(file, &sig, &size, passPhrase);
	if (ret == 0)
	    (void) headerAddEntry(h, sigTag, RPM_BIN_TYPE, sig, size);
	break;
    case RPMSIGTAG_GPG:
	rpmMessage(RPMMESS_VERBOSE, _("Generating signature using GPG.\n"));
        ret = makeGPGSignature(file, &sig, &size, passPhrase);
	if (ret == 0)
	    (void) headerAddEntry(h, sigTag, RPM_BIN_TYPE, sig, size);
	break;
    }

    return ret;
}

static int checkPassPhrase(const char * passPhrase, const int sigTag)
	/*@globals rpmGlobalMacroContext,
		fileSystem @*/
	/*@modifies rpmGlobalMacroContext,
		fileSystem @*/
{
    int passPhrasePipe[2];
    int pid, status;
    int fd;

    passPhrasePipe[0] = passPhrasePipe[1] = 0;
    (void) pipe(passPhrasePipe);
    if (!(pid = fork())) {
	(void) close(STDIN_FILENO);
	(void) close(STDOUT_FILENO);
	(void) close(passPhrasePipe[1]);
	/*@-internalglobs@*/ /* FIX: shrug */
	if (! rpmIsVerbose()) {
	    (void) close(STDERR_FILENO);
	}
	/*@=internalglobs@*/
	if ((fd = open("/dev/null", O_RDONLY)) != STDIN_FILENO) {
	    (void) dup2(fd, STDIN_FILENO);
	    (void) close(fd);
	}
	if ((fd = open("/dev/null", O_WRONLY)) != STDOUT_FILENO) {
	    (void) dup2(fd, STDOUT_FILENO);
	    (void) close(fd);
	}
	(void) dup2(passPhrasePipe[0], 3);

	switch (sigTag) {
	case RPMSIGTAG_GPG:
	{   const char *gpg_path = rpmExpand("%{_gpg_path}", NULL);
	    const char *name = rpmExpand("%{_gpg_name}", NULL);
	    if (gpg_path && *gpg_path != '%')
		(void) dosetenv("GNUPGHOME", gpg_path, 1);
	    (void) execlp("gpg", "gpg",
	           "--batch", "--no-verbose", "--passphrase-fd", "3",
	           "-u", name, "-so", "-",
	           NULL);
	    rpmError(RPMERR_EXEC, _("Couldn't exec gpg\n"));
	    _exit(RPMERR_EXEC);
	}   /*@notreached@*/ break;
	case RPMSIGTAG_PGP5:	/* XXX legacy */
	case RPMSIGTAG_PGP:
	{   const char *pgp_path = rpmExpand("%{_pgp_path}", NULL);
	    const char *name = rpmExpand("+myname=\"%{_pgp_name}\"", NULL);
	    const char *path;
	    pgpVersion pgpVer;

	    (void) dosetenv("PGPPASSFD", "3", 1);
	    if (pgp_path && *pgp_path != '%')
		(void) dosetenv("PGPPATH", pgp_path, 1);

	    if ((path = rpmDetectPGPVersion(&pgpVer)) != NULL) {
		switch(pgpVer) {
		case PGP_2:
		    (void) execlp(path, "pgp", "+batchmode=on", "+verbose=0",
			name, "-sf", NULL);
		    /*@innerbreak@*/ break;
		case PGP_5:	/* XXX legacy */
		    (void) execlp(path,"pgps", "+batchmode=on", "+verbose=0",
			name, "-f", NULL);
		    /*@innerbreak@*/ break;
		case PGP_UNKNOWN:
		case PGP_NOTDETECTED:
		    /*@innerbreak@*/ break;
		}
	    }
	    rpmError(RPMERR_EXEC, _("Couldn't exec pgp\n"));
	    _exit(RPMERR_EXEC);
	}   /*@notreached@*/ break;
	default: /* This case should have been screened out long ago. */
	    rpmError(RPMERR_SIGGEN, _("Invalid %%_signature spec in macro file\n"));
	    _exit(RPMERR_SIGGEN);
	    /*@notreached@*/ break;
	}
    }

    (void) close(passPhrasePipe[0]);
    (void) write(passPhrasePipe[1], passPhrase, strlen(passPhrase));
    (void) write(passPhrasePipe[1], "\n", 1);
    (void) close(passPhrasePipe[1]);

    (void)waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
	return 1;
    }

    /* passPhrase is good */
    return 0;
}

char * rpmGetPassPhrase(const char * prompt, const int sigTag)
{
    char *pass;
    int aok;

    switch (sigTag) {
    case RPMSIGTAG_GPG:
      { const char *name = rpmExpand("%{_gpg_name}", NULL);
	aok = (name && *name != '%');
	name = _free(name);
      }
	if (!aok) {
	    rpmError(RPMERR_SIGGEN,
		_("You must set \"%%_gpg_name\" in your macro file\n"));
	    return NULL;
	}
	break;
    case RPMSIGTAG_PGP5: 	/* XXX legacy */
    case RPMSIGTAG_PGP:
      { const char *name = rpmExpand("%{_pgp_name}", NULL);
	aok = (name && *name != '%');
	name = _free(name);
      }
	if (!aok) {
	    rpmError(RPMERR_SIGGEN,
		_("You must set \"%%_pgp_name\" in your macro file\n"));
	    return NULL;
	}
	break;
    default:
	/* Currently the calling function (rpm.c:main) is checking this and
	 * doing a better job.  This section should never be accessed.
	 */
	rpmError(RPMERR_SIGGEN, _("Invalid %%_signature spec in macro file\n"));
	return NULL;
	/*@notreached@*/ break;
    }

    pass = /*@-unrecog@*/ getpass( (prompt ? prompt : "") ) /*@=unrecog@*/ ;

    if (checkPassPhrase(pass, sigTag))
	return NULL;

    return pass;
}

static rpmVerifySignatureReturn
verifySizeSignature(/*@unused@*/ const char * fn,
		const byte * sig,
		/*@unused@*/ int siglen,
		const pgpDig dig, /*@out@*/ char * result)
	/*@modifies *result @*/
{
    char * t = result;
    int_32 size = *(int_32 *)sig;
    int res;

    *t = '\0';
    t = stpcpy(t, "Header+Payload size: ");

    /*@-type@*/
    if (size != dig->nbytes) {
	res = RPMSIG_BAD;
	sprintf(t, "BAD Expected(%d) != (%d)\n", size, dig->nbytes);
    } else {
	res = RPMSIG_OK;
	sprintf(t, "OK (%d)\n", dig->nbytes);
    }
    /*@=type@*/

    return res;
}

static rpmVerifySignatureReturn
verifyMD5Signature(/*@unused@*/ const char * fn, const byte * sig, int siglen,
		const pgpDig dig, /*@out@*/ char * result)
	/*@modifies *result @*/
{
    char * t = result;
    byte * md5sum = NULL;
    size_t md5len = 0;
    int res;

    /*@-type@*/
    (void) rpmDigestFinal(rpmDigestDup(dig->md5ctx),
		(void **)&md5sum, &md5len, 0);
    /*@=type@*/

    *t = '\0';
    t = stpcpy(t, "MD5 digest: ");

    if (md5len != siglen || memcmp(md5sum, sig, md5len)) {
	res = RPMSIG_BAD;
	t = stpcpy(t, "BAD Expected(");
	(void) pgpHexCvt(t, sig, siglen);
	t += strlen(t);
	t = stpcpy(t, ") != (");
    } else {
	res = RPMSIG_OK;
	t = stpcpy(t, "OK (");
    }
    (void) pgpHexCvt(t, md5sum, md5len);
    t += strlen(t);
    t = stpcpy(t, ")\n");

    md5sum = _free(md5sum);

    return res;
}

static rpmVerifySignatureReturn
verifyPGPSignature(/*@unused@*/ const char * fn,
		/*@unused@*/ const byte * sig,
		/*@unused@*/ int siglen,
		const pgpDig dig, /*@out@*/ char * result)
	/*@modifies *result */
{
    /*@unchecked@*/ static const byte * pgppk = NULL;
    /*@unchecked@*/ static size_t pgppklen = 0;
    char * t = result;
    int res, xx;

    *t = '\0';
    t = stpcpy(t, "V3 RSA/MD5 signature: ");

    /* XXX retrieve by keyid from signature. */

    /*@-globs -internalglobs -mods -modfilesys@*/
    if (pgppk == NULL) {
	const char * pkfn = rpmExpand("%{_pgp_pubkey}", NULL);
	if (pgpReadPkts(pkfn, &pgppk, &pgppklen)) {
	    res = RPMSIG_NOKEY;
	    t = stpcpy( stpcpy( stpcpy(t, "NOKEY ("), pkfn), ")\n");
	    pkfn = _free(pkfn);
	    return res;
	}
	rpmMessage(RPMMESS_DEBUG,
		"========== PGP RSA/MD5 pubkey %s\n", pkfn);
	xx = pgpPrtPkts(pgppk, pgppklen, NULL, rpmIsDebug());
	pkfn = _free(pkfn);
    }

    /* XXX sanity check on pubkey and signature agreement. */

#ifdef	NOTYET
    {	pgpPktSigV3 dsig = dig->signature.v3;
	pgpPktKeyV3 dpk  = dig->pubkey.v3;
	
	if (dsig->pubkey_algo != dpk->pubkey_algo) {
	    res = RPMSIG_NOKEY;
	    t = stpcpy(t, "NOKEY\n");
	    return res;
	}
    }
#endif

    /*@-nullpass@*/
    xx = pgpPrtPkts(pgppk, pgppklen, dig, 0);
    /*@=nullpass@*/
    /*@=globs =internalglobs =mods =modfilesys@*/

    /*@-type@*/
    if (!rsavrfy(&dig->rsa_pk, &dig->rsahm, &dig->c)) {
	res = RPMSIG_BAD;
	t = stpcpy(t, "BAD\n");
    } else {
	res = RPMSIG_OK;
	t = stpcpy(t, "OK\n");
    }
    /*@=type@*/

    return res;
}

static rpmVerifySignatureReturn
verifyGPGSignature(/*@unused@*/ const char * fn,
		/*@unused@*/ const byte * sig,
		/*@unused@*/ int siglen,
		const pgpDig dig, /*@out@*/ char * result)
	/*@modifies *result @*/
{
    /*@unchecked@*/ static const byte * gpgpk = NULL;
    /*@unchecked@*/ static size_t gpgpklen = 0;
    char * t = result;
    int res, xx;

    *t = '\0';
    t = stpcpy(t, "V3 DSA signature: ");

    /* XXX retrieve by keyid from signature. */

    /*@-globs -internalglobs -mods -modfilesys@*/
    if (gpgpk == NULL) {
	const char * pkfn = rpmExpand("%{_gpg_pubkey}", NULL);
	if (pgpReadPkts(pkfn, &gpgpk, &gpgpklen)) {
	    res = RPMSIG_NOKEY;
	    t = stpcpy( stpcpy( stpcpy(t, "NOKEY ("), pkfn), ")\n");
	    pkfn = _free(pkfn);
	    return res;
	}
	rpmMessage(RPMMESS_DEBUG,
		"========== GPG DSA pubkey %s\n", pkfn);
	xx = pgpPrtPkts(gpgpk, gpgpklen, NULL, rpmIsDebug());
	pkfn = _free(pkfn);
    }

    /* XXX sanity check on pubkey and signature agreement. */

#ifdef	NOTYET
    {	pgpPktSigV3 dsig = dig->signature.v3;
	pgpPktKeyV4 dpk  = dig->pubkey.v4;
	
	if (dsig->pubkey_algo != dpk->pubkey_algo) {
	    res = RPMSIG_NOKEY;
	    t = stpcpy(t, "NOKEY\n");
	    return res;
	}
    }
#endif

    /*@-nullpass@*/
    xx = pgpPrtPkts(gpgpk, gpgpklen, dig, 0);
    /*@=nullpass@*/
    /*@=globs =internalglobs =mods =modfilesys@*/

    /*@-type@*/
    if (!dsavrfy(&dig->p, &dig->q, &dig->g, &dig->hm, &dig->y, &dig->r, &dig->s)) {
	res = RPMSIG_BAD;
	t = stpcpy(t, "BAD\n");
    } else {
	res = RPMSIG_OK;
	t = stpcpy(t, "OK\n");
    }
    /*@=type@*/

    return res;
}

rpmVerifySignatureReturn
rpmVerifySignature(const char * fn, int_32 sigTag, const void * sig,
		int siglen, const pgpDig dig, char * result)
{
    int res;
    switch (sigTag) {
    case RPMSIGTAG_SIZE:
	res = verifySizeSignature(fn, sig, siglen, dig, result);
	break;
    case RPMSIGTAG_MD5:
	res = verifyMD5Signature(fn, sig, siglen, dig, result);
	break;
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
	res = verifyPGPSignature(fn, sig, siglen, dig, result);
	break;
    case RPMSIGTAG_GPG:
	res = verifyGPGSignature(fn, sig, siglen, dig, result);
	break;
    case RPMSIGTAG_LEMD5_1:
    case RPMSIGTAG_LEMD5_2:
	sprintf(result, "Broken MD5 digest: UNSUPPORTED\n");
	res = RPMSIG_UNKNOWN;
	break;
    default:
	sprintf(result, "Signature: UNKNOWN(%d)\n", sigTag);
	res = RPMSIG_UNKNOWN;
	break;
    }
    return res;
}
