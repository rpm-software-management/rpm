/** \ingroup signature
 * \file lib/signature.c
 */

#include "system.h"

#include "rpmio_internal.h"
#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX for rpmGetPath() */
#include "rpmdb.h"

#include "depends.h"
#include "misc.h"	/* XXX for dosetenv() and makeTempFile() */
#include "legacy.h"	/* XXX for mdbinfile() */
#include "rpmlead.h"
#include "signature.h"
#include "debug.h"

/*@access rpmTransactionSet@*/
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
      { const char *name = rpmExpand("%{?_signature}", NULL);
	if (!(name && *name != '\0'))
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
    const char *pgpbin = rpmGetPath("%{?_pgpbin}", NULL);

    if (saved_pgp_version == PGP_UNKNOWN) {
	char *pgpvbin;
	struct stat st;

	if (!(pgpbin && pgpbin[0] != '\0')) {
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

    rpmMessage((rc == RPMRC_OK ? RPMMESS_DEBUG : RPMMESS_DEBUG),
	_("Expected size: %12d = lead(%d)+sigs(%d)+pad(%d)+data(%d)\n"),
		(int)sizeof(struct rpmlead)+siglen+pad+datalen,
		(int)sizeof(struct rpmlead), siglen, pad, datalen);
    /*@=sizeoftype@*/
    rpmMessage((rc == RPMRC_OK ? RPMMESS_DEBUG : RPMMESS_DEBUG),
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
	h = headerFree(h, "ReadSignature");

    return rc;
}

int rpmWriteSignature(FD_t fd, Header h)
{
    static byte buf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
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
    return headerFree(h, "FreeSignature");
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
    const char * cmd;
    char *const *av;
    int rc;

    (void) stpcpy( stpcpy(sigfile, file), ".sig");

    addMacro(NULL, "__plaintext_filename", NULL, file, -1);
    addMacro(NULL, "__signature_filename", NULL, sigfile, -1);

    inpipe[0] = inpipe[1] = 0;
    (void) pipe(inpipe);

    if (!(pid = fork())) {
	const char *pgp_path = rpmExpand("%{?_pgp_path}", NULL);
	const char *path;
	pgpVersion pgpVer;

	(void) close(STDIN_FILENO);
	(void) dup2(inpipe[0], 3);
	(void) close(inpipe[1]);

	(void) dosetenv("PGPPASSFD", "3", 1);
	if (pgp_path && *pgp_path != '\0')
	    (void) dosetenv("PGPPATH", pgp_path, 1);

	/* dosetenv("PGPPASS", passPhrase, 1); */

	if ((path = rpmDetectPGPVersion(&pgpVer)) != NULL) {
	    switch(pgpVer) {
	    case PGP_2:
		cmd = rpmExpand("%{?__pgp_sign_cmd}", NULL);
		rc = poptParseArgvString(cmd, NULL, (const char ***)&av);
		if (!rc)
		    rc = execve(av[0], av+1, environ);
		break;
	    case PGP_5:
		cmd = rpmExpand("%{?__pgp5_sign_cmd}", NULL);
		rc = poptParseArgvString(cmd, NULL, (const char ***)&av);
		if (!rc)
		    rc = execve(av[0], av+1, environ);
		break;
	    case PGP_UNKNOWN:
	    case PGP_NOTDETECTED:
		/*@-mods@*/ /* FIX: shrug */
		errno = ENOENT;
		/*@=mods@*/
		break;
	    }
	}
	rpmError(RPMERR_EXEC, _("Could not exec %s: %s\n"), "pgp",
			strerror(errno));
	_exit(RPMERR_EXEC);
    }

    delMacro(NULL, "__plaintext_filename");
    delMacro(NULL, "__signature_filename");

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

	rc = 0;
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
    const char * cmd;
    char *const *av;
    int rc;

    (void) stpcpy( stpcpy(sigfile, file), ".sig");

    addMacro(NULL, "__plaintext_filename", NULL, file, -1);
    addMacro(NULL, "__signature_filename", NULL, sigfile, -1);

    inpipe[0] = inpipe[1] = 0;
    (void) pipe(inpipe);

    if (!(pid = fork())) {
	const char *gpg_path = rpmExpand("%{?_gpg_path}", NULL);

	(void) close(STDIN_FILENO);
	(void) dup2(inpipe[0], 3);
	(void) close(inpipe[1]);

	if (gpg_path && *gpg_path != '\0')
	    (void) dosetenv("GNUPGHOME", gpg_path, 1);

	cmd = rpmExpand("%{?__gpg_sign_cmd}", NULL);
	rc = poptParseArgvString(cmd, NULL, (const char ***)&av);
	if (!rc)
	    rc = execve(av[0], av+1, environ);

	rpmError(RPMERR_EXEC, _("Could not exec %s: %s\n"), "gpg",
			strerror(errno));
	_exit(RPMERR_EXEC);
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

	rc = 0;
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
		const char * passPhrase)
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
    case RPMSIGTAG_DSA:		/* XXX UNIMPLEMENTED */
	break;
    case RPMSIGTAG_SHA1:	/* XXX UNIMPLEMENTED */
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
    const char * cmd;
    char *const *av;
    int rc;

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
	case RPMSIGTAG_DSA:
	case RPMSIGTAG_GPG:
	{   const char *gpg_path = rpmExpand("%{?_gpg_path}", NULL);

	    if (gpg_path && *gpg_path != '\0')
  		(void) dosetenv("GNUPGHOME", gpg_path, 1);

	    cmd = rpmExpand("%{?__gpg_check_password_cmd}", NULL);
	    rc = poptParseArgvString(cmd, NULL, (const char ***)&av);
	    if (!rc)
		rc = execve(av[0], av+1, environ);

	    rpmError(RPMERR_EXEC, _("Could not exec %s: %s\n"), "gpg",
			strerror(errno));
	}   /*@notreached@*/ break;
	case RPMSIGTAG_PGP5:	/* XXX legacy */
	case RPMSIGTAG_PGP:
	{   const char *pgp_path = rpmExpand("%{?_pgp_path}", NULL);
	    const char *path;
	    pgpVersion pgpVer;

	    (void) dosetenv("PGPPASSFD", "3", 1);
	    if (pgp_path && *pgp_path != '\0')
		(void) dosetenv("PGPPATH", pgp_path, 1);

	    if ((path = rpmDetectPGPVersion(&pgpVer)) != NULL) {
		switch(pgpVer) {
		case PGP_2:
		    cmd = rpmExpand("%{?__pgp_check_password_cmd}", NULL);
		    rc = poptParseArgvString(cmd, NULL, (const char ***)&av);
		    if (!rc)
			rc = execve(av[0], av+1, environ);
		    /*@innerbreak@*/ break;
		case PGP_5:	/* XXX legacy */
		    cmd = rpmExpand("%{?__pgp5_check_password_cmd}", NULL);
		    rc = poptParseArgvString(cmd, NULL, (const char ***)&av);
		    if (!rc)
			rc = execve(av[0], av+1, environ);
		    /*@innerbreak@*/ break;
		case PGP_UNKNOWN:
		case PGP_NOTDETECTED:
		    /*@innerbreak@*/ break;
		}
	    }
	    rpmError(RPMERR_EXEC, _("Could not exec %s: %s\n"), "pgp",
			strerror(errno));
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
    case RPMSIGTAG_DSA:
    case RPMSIGTAG_GPG:
      { const char *name = rpmExpand("%{?_gpg_name}", NULL);
	aok = (name && *name != '\0');
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
      { const char *name = rpmExpand("%{?_pgp_name}", NULL);
	aok = (name && *name != '\0');
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

static /*@observer@*/ const char * rpmSigString(rpmVerifySignatureReturn res)
	/*@*/
{
    const char * str;
    switch (res) {
    case RPMSIG_OK:		str = "OK";		break;
    case RPMSIG_BAD:		str = "BAD";		break;
    case RPMSIG_NOKEY:		str = "NOKEY";		break;
    case RPMSIG_NOTTRUSTED:	str = "NOTRUSTED";	break;
    case RPMSIG_UNKNOWN:
    default:			str = "UNKNOWN";	break;
    }
    return str;
}

static rpmVerifySignatureReturn
verifySizeSignature(const rpmTransactionSet ts, /*@out@*/ char * t)
	/*@modifies *t @*/
{
    rpmVerifySignatureReturn res;
    int_32 size = 0x7fffffff;

    *t = '\0';
    t = stpcpy(t, _("Header+Payload size: "));

    if (ts->sig == NULL || ts->dig == NULL) {
	res = RPMSIG_NOKEY;
	t = stpcpy(t, rpmSigString(res));
	goto exit;
    }

/*@=nullpass =nullderef@*/ /* FIX: ts->{sig,dig} can be NULL */
    memcpy(&size, ts->sig, sizeof(size));

    /*@-type@*/
    /*@-nullderef@*/ /* FIX: ts->dig can be NULL */
    if (size != ts->dig->nbytes) {
	res = RPMSIG_BAD;
	t = stpcpy(t, rpmSigString(res));
	sprintf(t, " Expected(%d) != (%d)\n", size, ts->dig->nbytes);
    } else {
	res = RPMSIG_OK;
	t = stpcpy(t, rpmSigString(res));
	sprintf(t, " (%d)", ts->dig->nbytes);
    }
    /*@=type@*/
/*@=nullpass =nullderef@*/

exit:
    t = stpcpy(t, "\n");
    return res;
}

static rpmVerifySignatureReturn
verifyMD5Signature(const rpmTransactionSet ts, /*@out@*/ char * t)
	/*@modifies *t @*/
{
    rpmVerifySignatureReturn res;
    byte * md5sum = NULL;
    size_t md5len = 0;

    *t = '\0';
    t = stpcpy(t, _("MD5 digest: "));

    if (ts->sig == NULL || ts->dig == NULL) {
	res = RPMSIG_NOKEY;
	t = stpcpy(t, rpmSigString(res));
	goto exit;
    }

/*@=nullpass =nullderef@*/ /* FIX: ts->{sig,dig} can be NULL */
    /*@-type@*/
    (void) rpmDigestFinal(rpmDigestDup(ts->dig->md5ctx),
		(void **)&md5sum, &md5len, 0);
    /*@=type@*/

    if (md5len != ts->siglen || memcmp(md5sum, ts->sig, md5len)) {
	res = RPMSIG_BAD;
	t = stpcpy(t, rpmSigString(res));
	t = stpcpy(t, " Expected(");
	(void) pgpHexCvt(t, ts->sig, ts->siglen);
	t += strlen(t);
	t = stpcpy(t, ") != (");
    } else {
	res = RPMSIG_OK;
	t = stpcpy(t, rpmSigString(res));
	t = stpcpy(t, " (");
    }
/*@=nullpass =nullderef@*/
    (void) pgpHexCvt(t, md5sum, md5len);
    t += strlen(t);
    t = stpcpy(t, ")");

exit:
    md5sum = _free(md5sum);
    t = stpcpy(t, "\n");
    return res;
}

static rpmVerifySignatureReturn
verifySHA1Signature(const rpmTransactionSet ts, /*@out@*/ char * t)
	/*@modifies *t @*/
{
    rpmVerifySignatureReturn res;
    const char * sha1 = NULL;

    *t = '\0';
    t = stpcpy(t, _("SHA1 header digest: "));

    if (ts->sig == NULL || ts->dig == NULL) {
	res = RPMSIG_NOKEY;
	t = stpcpy(t, rpmSigString(res));
	goto exit;
    }

    /*@-type@*/
    (void) rpmDigestFinal(rpmDigestDup(ts->dig->sha1ctx),
		(void **)&sha1, NULL, 1);
    /*@=type@*/

    if (sha1 == NULL || strlen(sha1) != strlen(ts->sig)) {
	res = RPMSIG_BAD;
	t = stpcpy(t, rpmSigString(res));
	t = stpcpy(t, " Expected(");
	t = stpcpy(t, ts->sig);
	t = stpcpy(t, ") != (");
    } else {
	res = RPMSIG_OK;
	t = stpcpy(t, rpmSigString(res));
	t = stpcpy(t, " (");
    }
    if (sha1)
	t = stpcpy(t, sha1);
    t = stpcpy(t, ")");

exit:
    sha1 = _free(sha1);
    t = stpcpy(t, "\n");
    return res;
}

/**
 * Retrieve pubkey from rpm database.
 * @param ts		rpm transaction
 * @return		RPMSIG_OK on success, RPMSIG_NOKEY if not found
 */
static rpmVerifySignatureReturn
rpmtsFindPubkey(rpmTransactionSet ts)
	/*@modifies ts */
{
    struct pgpDigParams_s * sigp = NULL;
    rpmVerifySignatureReturn res;
    /*@unchecked@*/ /*@only@*/ static const byte * pkpkt = NULL;
    /*@unchecked@*/ static size_t pkpktlen = 0;
    /*@unchecked@*/ static byte pksignid[8];
    int xx;

    if (ts->sig == NULL || ts->dig == NULL) {
	res = RPMSIG_NOKEY;
	goto exit;
    }
    sigp = &ts->dig->signature;

    /*@-globs -internalglobs -mods -modfilesys@*/
    if (pkpkt == NULL || memcmp(sigp->signid, pksignid, sizeof(pksignid))) {
	int ix = -1;
	rpmdbMatchIterator mi;
	Header h;

	pkpkt = _free(pkpkt);
	pkpktlen = 0;
	memset(pksignid, 0, sizeof(pksignid));

	(void) rpmtsOpenDB(ts, ts->dbmode);

	mi = rpmtsInitIterator(ts, RPMTAG_PUBKEYS, sigp->signid, sizeof(sigp->signid));
	while ((h = rpmdbNextIterator(mi)) != NULL) {
	    const char ** pubkeys;
	    int_32 pt, pc;

	    if (!headerGetEntry(h, RPMTAG_PUBKEYS, &pt, (void **)&pubkeys, &pc))
		continue;
	    ix = rpmdbGetIteratorFileNum(mi);
	    if (ix >= pc
	    || b64decode(pubkeys[ix], (void **) &pkpkt, &pkpktlen))
		ix = -1;
	    pubkeys = headerFreeData(pubkeys, pt);
	    break;
	}
	mi = rpmdbFreeIterator(mi);

	if (ix < 0 || pkpkt == NULL) {
	    res = RPMSIG_NOKEY;
	    goto exit;
	}

	/* Make sure the pkt can be parsed, print info if debugging. */
	if (pgpPrtPkts(pkpkt, pkpktlen, NULL, 0)) {
	    res = RPMSIG_NOKEY;
	    goto exit;
	}

	/* XXX Verify the pubkey signature. */

	/* Packet looks good, save the signer id. */
	memcpy(pksignid, sigp->signid, sizeof(pksignid));
    }

#ifdef	NOTNOW
    {
	if (pkpkt == NULL) {
	    const char * pkfn = rpmExpand("%{_gpg_pubkey}", NULL);
	    if (pgpReadPkts(pkfn, &pkpkt, &pkpktlen) != PGPARMOR_PUBKEY) {
		pkfn = _free(pkfn);
		res = RPMSIG_NOKEY;
		goto exit;
	    }
	    pkfn = _free(pkfn);
	}
    }
#endif

    rpmMessage(RPMMESS_DEBUG, "========== %s pubkey id %s\n",
    	(sigp->pubkey_algo == PGPPUBKEYALGO_DSA ? "DSA" :
    	(sigp->pubkey_algo == PGPPUBKEYALGO_RSA ? "RSA" : "???")),
	pgpHexStr(sigp->signid, sizeof(sigp->signid)));

    /* Retrieve parameters from pubkey packet(s). */
    xx = pgpPrtPkts(pkpkt, pkpktlen, ts->dig, 0);
    /*@=globs =internalglobs =mods =modfilesys@*/

    /* Make sure we have the correct public key. */
    if (ts->dig->signature.pubkey_algo == ts->dig->pubkey.pubkey_algo
#ifdef	NOTYET
     && ts->dig->signature.hash_algo == ts->dig->pubkey.hash_algo
#endif
     &&	!memcmp(ts->dig->signature.signid, ts->dig->pubkey.signid, 8))
	res = RPMSIG_OK;
    else
	res = RPMSIG_NOKEY;

    /* XXX Verify the signature signature. */

exit:
    return res;
}

static rpmVerifySignatureReturn
verifyPGPSignature(rpmTransactionSet ts, /*@out@*/ char * t)
	/*@modifies ts, *t */
{
    struct pgpDigParams_s * sigp = NULL;
    rpmVerifySignatureReturn res;
    int xx;

    *t = '\0';
    t = stpcpy(t, _("V3 RSA/MD5 signature: "));

    if (ts->sig == NULL || ts->dig == NULL) {
	res = RPMSIG_NOKEY;
	goto exit;
    }
    sigp = &ts->dig->signature;

    /* XXX sanity check on ts->sigtag and signature agreement. */
    if (!(ts->sigtag == RPMSIGTAG_PGP
    	&& sigp->pubkey_algo == PGPPUBKEYALGO_RSA
    	&& sigp->hash_algo == PGPHASHALGO_MD5))
    {
	res = RPMSIG_NOKEY;
	goto exit;
    }

    /*@-type@*/ /* FIX: cast? */
    {	DIGEST_CTX ctx = rpmDigestDup(ts->dig->md5ctx);

	if (sigp->hash != NULL)
	    xx = rpmDigestUpdate(ctx, sigp->hash, sigp->hashlen);
	xx = rpmDigestFinal(ctx, (void **)&ts->dig->md5, &ts->dig->md5len, 1);

	/* XXX compare leading 16 bits of digest for quick check. */
    }
    /*@=type@*/

    {	const char * prefix = "3020300c06082a864886f70d020505000410";
	unsigned int nbits = 1024;
	unsigned int nb = (nbits + 7) >> 3;
	const char * hexstr;
	char * tt;

	hexstr = tt = xmalloc(2 * nb + 1);
	memset(tt, 'f', (2 * nb));
	tt[0] = '0'; tt[1] = '0';
	tt[2] = '0'; tt[3] = '1';
	tt += (2 * nb) - strlen(prefix) - strlen(ts->dig->md5) - 2;
	*tt++ = '0'; *tt++ = '0';
	tt = stpcpy(tt, prefix);
	tt = stpcpy(tt, ts->dig->md5);
		
	mp32nzero(&ts->dig->rsahm);	mp32nsethex(&ts->dig->rsahm, hexstr);

	hexstr = _free(hexstr);
    }

    /* Retrieve the matching public key. */
    res = rpmtsFindPubkey(ts);
    if (res != RPMSIG_OK)
	goto exit;

    /*@-type@*/
    if (rsavrfy(&ts->dig->rsa_pk, &ts->dig->rsahm, &ts->dig->c))
	res = RPMSIG_OK;
    else
	res = RPMSIG_BAD;
    /*@=type@*/

exit:
    t = stpcpy(t, rpmSigString(res));
    if (sigp != NULL) {
	t = stpcpy(t, ", key ID ");
	(void) pgpHexCvt(t, sigp->signid+4, sizeof(sigp->signid)-4);
	t += strlen(t);
    }
    t = stpcpy(t, "\n");
    return res;
}

static rpmVerifySignatureReturn
verifyGPGSignature(rpmTransactionSet ts, /*@out@*/ char * t)
	/*@modifies ts, *t @*/
{
    struct pgpDigParams_s * sigp = NULL;
    rpmVerifySignatureReturn res;
    int xx;

    *t = '\0';
    t = stpcpy(t, _("V3 DSA signature: "));

    if (ts->sig == NULL || ts->dig == NULL) {
	res = RPMSIG_NOKEY;
	goto exit;
    }
    sigp = &ts->dig->signature;

    /* XXX sanity check on ts->sigtag and signature agreement. */
    if (!((ts->sigtag == RPMSIGTAG_GPG || ts->sigtag == RPMSIGTAG_DSA)
    	&& sigp->pubkey_algo == PGPPUBKEYALGO_DSA
    	&& sigp->hash_algo == PGPHASHALGO_SHA1))
    {
	res = RPMSIG_NOKEY;
	goto exit;
    }

    /*@-type@*/ /* FIX: cast? */
    {	DIGEST_CTX ctx = rpmDigestDup(ts->dig->sha1ctx);

	if (sigp->hash != NULL)
	    xx = rpmDigestUpdate(ctx, sigp->hash, sigp->hashlen);
	xx = rpmDigestFinal(ctx, (void **)&ts->dig->sha1, &ts->dig->sha1len, 1);

	/* XXX compare leading 16 bits of digest for quick check. */

	mp32nzero(&ts->dig->hm);	mp32nsethex(&ts->dig->hm, ts->dig->sha1);
    }
    /*@=type@*/

    /* Retrieve the matching public key. */
    res = rpmtsFindPubkey(ts);
    if (res != RPMSIG_OK)
	goto exit;

    /*@-type@*/
    if (dsavrfy(&ts->dig->p, &ts->dig->q, &ts->dig->g,
		&ts->dig->hm, &ts->dig->y, &ts->dig->r, &ts->dig->s))
	res = RPMSIG_OK;
    else
	res = RPMSIG_BAD;
    /*@=type@*/

exit:
    t = stpcpy(t, rpmSigString(res));
    if (sigp != NULL) {
	t = stpcpy(t, ", key ID ");
	(void) pgpHexCvt(t, sigp->signid+4, sizeof(sigp->signid)-4);
	t += strlen(t);
    }
    t = stpcpy(t, "\n");
    return res;
}

rpmVerifySignatureReturn
rpmVerifySignature(const rpmTransactionSet ts, char * result)
{
    rpmVerifySignatureReturn res;

    if (ts->sig == NULL || ts->siglen <= 0 || ts->dig == NULL) {
	sprintf(result, _("Verify signature: BAD PARAMETERS\n"));
	return RPMSIG_UNKNOWN;
    }

    switch (ts->sigtag) {
    case RPMSIGTAG_SIZE:
	res = verifySizeSignature(ts, result);
	break;
    case RPMSIGTAG_MD5:
	res = verifyMD5Signature(ts, result);
	break;
    case RPMSIGTAG_SHA1:
	res = verifySHA1Signature(ts, result);
	break;
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
	res = verifyPGPSignature(ts, result);
	break;
    case RPMSIGTAG_DSA:
    case RPMSIGTAG_GPG:
	res = verifyGPGSignature(ts, result);
	break;
    case RPMSIGTAG_LEMD5_1:
    case RPMSIGTAG_LEMD5_2:
	sprintf(result, _("Broken MD5 digest: UNSUPPORTED\n"));
	res = RPMSIG_UNKNOWN;
	break;
    default:
	sprintf(result, _("Signature: UNKNOWN (%d)\n"), ts->sigtag);
	res = RPMSIG_UNKNOWN;
	break;
    }
    return res;
}
