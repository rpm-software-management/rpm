/** \ingroup signature
 * \file lib/signature.c
 */

/* signature.c - RPM signature functions */

/* NOTES
 *
 * Things have been cleaned up wrt PGP.  We can now handle
 * signatures of any length (which means you can use any
 * size key you like).  We also honor PGPPATH finally.
 */

#include "system.h"

#if HAVE_ASM_BYTEORDER_H
#include <asm/byteorder.h>
#endif

#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX for rpmGetPath() */

#include "md5.h"
#include "misc.h"	/* XXX for dosetenv() and makeTempFile() */
#include "rpmlead.h"
#include "signature.h"
#include "debug.h"

/*@access Header@*/		/* XXX compared with NULL */
/*@access FD_t@*/		/* XXX compared with NULL */

typedef	unsigned char byte;

typedef int (*md5func)(const char * fn, /*@out@*/ byte * digest);

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

    rc = (((sizeof(struct rpmlead) + siglen + pad + datalen) - st.st_size)
	? RPMRC_BADSIZE : RPMRC_OK);

    rpmMessage((rc == RPMRC_OK ? RPMMESS_DEBUG : RPMMESS_WARNING),
	_("Expected size: %12d = lead(%d)+sigs(%d)+pad(%d)+data(%d)\n"),
		(int)sizeof(struct rpmlead)+siglen+pad+datalen,
		(int)sizeof(struct rpmlead), siglen, pad, datalen);
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
	if (timedRead(fd, buf, 256) != 256)
	    break;
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
	if (pad && timedRead(fd, buf, pad) != pad)
	    rc = RPMRC_SHORTREAD;
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
	/*@modifies *sig, *size, fileSystem @*/
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
	    rc = timedRead(fd, *sig, *size);
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
	/*@modifies *sig, *size, fileSystem @*/
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
	    rc = timedRead(fd, *sig, *size);
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

static rpmVerifySignatureReturn
verifySizeSignature(const char * datafile, int_32 size, /*@out@*/ char * result)
	/*@modifies *result, fileSystem @*/
{
    struct stat st;

    (void) stat(datafile, &st);
    if (size != st.st_size) {
	sprintf(result, "Header+Archive size mismatch.\n"
		"Expected %d, saw %d.\n",
		size, (int)st.st_size);
	return RPMSIG_BAD;
    }

    sprintf(result, "Header+Archive size OK: %d bytes\n", size);
    return RPMSIG_OK;
}

#define	X(_x)	(unsigned)((_x) & 0xff)

static rpmVerifySignatureReturn
verifyMD5Signature(const char * datafile, const byte * sig, 
			      /*@out@*/ char * result, md5func fn)
	/*@modifies *result, fileSystem @*/
{
    byte md5sum[16];

    memset(md5sum, 0, sizeof(md5sum));
    (void) fn(datafile, md5sum);
    if (memcmp(md5sum, sig, 16)) {
	sprintf(result, "MD5 sum mismatch\n"
		"Expected: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
		"%02x%02x%02x%02x%02x\n"
		"Saw     : %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
		"%02x%02x%02x%02x%02x\n",
		X(sig[0]),  X(sig[1]),  X(sig[2]),  X(sig[3]),
		X(sig[4]),  X(sig[5]),  X(sig[6]),  X(sig[7]),
		X(sig[8]),  X(sig[9]),  X(sig[10]), X(sig[11]),
		X(sig[12]), X(sig[13]), X(sig[14]), X(sig[15]),
		X(md5sum[0]),  X(md5sum[1]),  X(md5sum[2]),  X(md5sum[3]),
		X(md5sum[4]),  X(md5sum[5]),  X(md5sum[6]),  X(md5sum[7]),
		X(md5sum[8]),  X(md5sum[9]),  X(md5sum[10]), X(md5sum[11]),
		X(md5sum[12]), X(md5sum[13]), X(md5sum[14]), X(md5sum[15]) );
	return RPMSIG_BAD;
    }

    sprintf(result, "MD5 sum OK: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"
                    "%02x%02x%02x%02x%02x\n",
	    X(md5sum[0]),  X(md5sum[1]),  X(md5sum[2]),  X(md5sum[3]),
	    X(md5sum[4]),  X(md5sum[5]),  X(md5sum[6]),  X(md5sum[7]),
	    X(md5sum[8]),  X(md5sum[9]),  X(md5sum[10]), X(md5sum[11]),
	    X(md5sum[12]), X(md5sum[13]), X(md5sum[14]), X(md5sum[15]) );

    return RPMSIG_OK;
}

static rpmVerifySignatureReturn
verifyPGPSignature(const char * datafile, const void * sig, int count,
		/*@out@*/ char * result)
	/*@modifies *result, fileSystem @*/
{
    int pid, status, outpipe[2];
/*@only@*/ /*@null@*/ const char * sigfile = NULL;
    byte buf[BUFSIZ];
    FILE *file;
    int res = RPMSIG_OK;
    const char *path;
    pgpVersion pgpVer;

    /* What version do we have? */
    if ((path = rpmDetectPGPVersion(&pgpVer)) == NULL) {
	errno = ENOENT;
	rpmError(RPMERR_EXEC, 
		 _("Could not run pgp.  Use --nopgp to skip PGP checks.\n"));
	_exit(RPMERR_EXEC);
    }

    /*
     * Sad but true: pgp-5.0 returns exit value of 0 on bad signature.
     * Instead we have to use the text output to detect a bad signature.
     */
    if (pgpVer == PGP_5)
	res = RPMSIG_BAD;

    /* Write out the signature */
#ifdef	DYING
  { const char *tmppath = rpmGetPath("%{_tmppath}", NULL);
    sigfile = tempnam(tmppath, "rpmsig");
    tmppath = _free(tmppath);
  }
    sfd = Fopen(sigfile, "w.fdio");
    if (sfd != NULL && !Ferror(sfd)) {
	(void) Fwrite(sig, sizeof(char), count, sfd);
	(void) Fclose(sfd);
    }
#else
    {	FD_t sfd;
	if (!makeTempFile(NULL, &sigfile, &sfd)) {
	    (void) Fwrite(sig, sizeof(char), count, sfd);
	    (void) Fclose(sfd);
	    sfd = NULL;
	}
    }
#endif
    if (sigfile == NULL)
	return RPMSIG_BAD;

    /* Now run PGP */
    outpipe[0] = outpipe[1] = 0;
    (void) pipe(outpipe);

    if (!(pid = fork())) {
	const char *pgp_path = rpmExpand("%{_pgp_path}", NULL);

	(void) close(outpipe[0]);
	(void) close(STDOUT_FILENO);	/* XXX unnecessary */
	(void) dup2(outpipe[1], STDOUT_FILENO);

	if (pgp_path && *pgp_path != '%')
	    (void) dosetenv("PGPPATH", pgp_path, 1);

	switch (pgpVer) {
	case PGP_5:
	    /* Some output (in particular "This signature applies to */
	    /* another message") is _always_ written to stderr; we   */
	    /* want to catch that output, so dup stdout to stderr:   */
	{   int save_stderr = dup(2);
	    (void) dup2(1, 2);
	    (void) execlp(path, "pgpv", "+batchmode=on", "+verbose=0",
		   /* Write "Good signature..." to stdout: */
		   "+OutputInformationFD=1",
		   /* Write "WARNING: ... is not trusted to... to stdout: */
		   "+OutputWarningFD=1",
		   sigfile, "-o", datafile, NULL);
	    /* Restore stderr so we can print the error message below. */
	    (void) dup2(save_stderr, 2);
	    (void) close(save_stderr);
	}   break;
	case PGP_2:
	    (void) execlp(path, "pgp", "+batchmode=on", "+verbose=0",
		   sigfile, datafile, NULL);
	    break;
	case PGP_UNKNOWN:
	case PGP_NOTDETECTED:
	    break;
	}

	rpmError(RPMERR_EXEC, 
		 _("Could not run pgp.  Use --nopgp to skip PGP checks.\n"));
	_exit(RPMERR_EXEC);
    }

    (void) close(outpipe[1]);
    file = fdopen(outpipe[0], "r");
    result[0] = '\0';
    if (file) {
	while (fgets(buf, 1024, file)) {
	    if (strncmp("File '", buf, 6) &&
		strncmp("Text is assu", buf, 12) &&
		strncmp("This signature applies to another message", buf, 41) &&
		buf[0] != '\n') {
		strcat(result, buf);
	    }
	    if (!strncmp("WARNING: Can't find the right public key", buf, 40))
		res = RPMSIG_NOKEY;
	    else if (!strncmp("Signature by unknown keyid:", buf, 27))
		res = RPMSIG_NOKEY;
	    else if (!strncmp("WARNING: The signing key is not trusted", buf, 39))
		res = RPMSIG_NOTTRUSTED;
	    else if (!strncmp("Good signature", buf, 14))
		res = RPMSIG_OK;
	}
	(void) fclose(file);
    }

    (void) waitpid(pid, &status, 0);
    if (sigfile) (void) unlink(sigfile);
    sigfile = _free(sigfile);
    if (!res && (!WIFEXITED(status) || WEXITSTATUS(status))) {
	res = RPMSIG_BAD;
    }
    
    return res;
}

static rpmVerifySignatureReturn
verifyGPGSignature(const char * datafile, const void * sig, int count,
		/*@out@*/ char * result)
	/*@modifies *result, fileSystem @*/
{
    int pid, status, outpipe[2];
/*@only@*/ /*@null@*/ const char * sigfile = NULL;
    byte buf[BUFSIZ];
    FILE *file;
    int res = RPMSIG_OK;
  
    /* Write out the signature */
#ifdef	DYING
  { const char *tmppath = rpmGetPath("%{_tmppath}", NULL);
    sigfile = tempnam(tmppath, "rpmsig");
    tmppath = _free(tmppath);
  }
    sfd = Fopen(sigfile, "w.fdio");
    if (sfd != NULL && !Ferror(sfd)) {
	(void) Fwrite(sig, sizeof(char), count, sfd);
	(void) Fclose(sfd);
    }
#else
    {	FD_t sfd;
	if (!makeTempFile(NULL, &sigfile, &sfd)) {
	    (void) Fwrite(sig, sizeof(char), count, sfd);
	    (void) Fclose(sfd);
	    sfd = NULL;
	}
    }
#endif
    if (sigfile == NULL)
	return RPMSIG_BAD;

    /* Now run GPG */
    outpipe[0] = outpipe[1] = 0;
    (void) pipe(outpipe);

    if (!(pid = fork())) {
	const char *gpg_path = rpmExpand("%{_gpg_path}", NULL);

	(void) close(outpipe[0]);
	/* gpg version 0.9 sends its output to stderr. */
	(void) dup2(outpipe[1], STDERR_FILENO);

	if (gpg_path && *gpg_path != '%')
	    (void) dosetenv("GNUPGHOME", gpg_path, 1);

	(void) execlp("gpg", "gpg",
	       "--batch", "--no-verbose", 
	       "--verify", sigfile, datafile,
	       NULL);
	rpmError(RPMERR_EXEC, 
		 _("Could not run gpg.  Use --nogpg to skip GPG checks.\n"));
	_exit(RPMERR_EXEC);
    }

    (void) close(outpipe[1]);
    file = fdopen(outpipe[0], "r");
    result[0] = '\0';
    if (file) {
	while (fgets(buf, 1024, file)) {
	    strcat(result, buf);
	    if (!xstrncasecmp("gpg: Can't check signature: Public key not found", buf, 48)) {
		res = RPMSIG_NOKEY;
	    }
	}
	(void) fclose(file);
    }
  
    (void) waitpid(pid, &status, 0);
    if (sigfile) (void) unlink(sigfile);
    sigfile = _free(sigfile);
    if (!res && (!WIFEXITED(status) || WEXITSTATUS(status))) {
	res = RPMSIG_BAD;
    }
    
    return res;
}

static int checkPassPhrase(const char * passPhrase, const int sigTag)
	/*@modifies fileSystem @*/
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
	if (! rpmIsVerbose()) {
	    (void) close(STDERR_FILENO);
	}
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
		    break;
		case PGP_5:	/* XXX legacy */
		    (void) execlp(path,"pgps", "+batchmode=on", "+verbose=0",
			name, "-f", NULL);
		    break;
		case PGP_UNKNOWN:
		case PGP_NOTDETECTED:
		    break;
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

rpmVerifySignatureReturn
rpmVerifySignature(const char * file, int_32 sigTag, const void * sig,
		int count, char * result)
{
    switch (sigTag) {
    case RPMSIGTAG_SIZE:
	return verifySizeSignature(file, *(int_32 *)sig, result);
	/*@notreached@*/ break;
    case RPMSIGTAG_MD5:
	return verifyMD5Signature(file, sig, result, mdbinfile);
	/*@notreached@*/ break;
    case RPMSIGTAG_LEMD5_1:
    case RPMSIGTAG_LEMD5_2:
	return verifyMD5Signature(file, sig, result, mdbinfileBroken);
	/*@notreached@*/ break;
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
	return verifyPGPSignature(file, sig, count, result);
	/*@notreached@*/ break;
    case RPMSIGTAG_GPG:
	return verifyGPGSignature(file, sig, count, result);
	/*@notreached@*/ break;
    default:
	sprintf(result, "Do not know how to verify sig type %d\n", sigTag);
	return RPMSIG_UNKNOWN;
    }
    /*@notreached@*/
    return RPMSIG_OK;
}
