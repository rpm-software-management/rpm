/** \ingroup signature
 * \file lib/signature.c
 */

#include "system.h"

#include "rpmio_internal.h"
#include <rpmlib.h>
#include <rpmmacro.h>	/* XXX for rpmGetPath() */
#include "rpmdb.h"

#include "rpmts.h"

#include "misc.h"	/* XXX for dosetenv() and makeTempFile() */
#include "legacy.h"	/* XXX for mdbinfile() */
#include "rpmlead.h"
#include "signature.h"
#include "header_internal.h"
#include "debug.h"

/*@access FD_t@*/		/* XXX ufdio->read arg1 is void ptr */
/*@access Header@*/		/* XXX compared with NULL */
/*@access entryInfo @*/		/* XXX rpmReadSignature */
/*@access indexEntry @*/	/* XXX rpmReadSignature */
/*@access DIGEST_CTX@*/		/* XXX compared with NULL */
/*@access pgpDig@*/
/*@access pgpDigParams@*/

#if !defined(__GLIBC__) && !defined(__APPLE__)
char ** environ = NULL;
#endif

int rpmLookupSignatureType(int action)
{
    /*@unchecked@*/
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
/*@-boundsread@*/
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
/*@=boundsread@*/
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
    const char *pgpbin = rpmGetPath("%{?_pgpbin}", NULL);

    if (saved_pgp_version == PGP_UNKNOWN) {
	char *pgpvbin;
	struct stat st;

/*@-boundsread@*/
	if (!(pgpbin && pgpbin[0] != '\0')) {
	    pgpbin = _free(pgpbin);
	    saved_pgp_version = -1;
	    return NULL;
	}
/*@=boundsread@*/
/*@-boundswrite@*/
	pgpvbin = (char *)alloca(strlen(pgpbin) + sizeof("v"));
	(void)stpcpy(stpcpy(pgpvbin, pgpbin), "v");
/*@=boundswrite@*/

	if (stat(pgpvbin, &st) == 0)
	    saved_pgp_version = PGP_5;
	else if (stat(pgpbin, &st) == 0)
	    saved_pgp_version = PGP_2;
	else
	    saved_pgp_version = PGP_NOTDETECTED;
    }

/*@-boundswrite@*/
    if (pgpVer && pgpbin)
	*pgpVer = saved_pgp_version;
/*@=boundswrite@*/
    return pgpbin;
}

/**
 * Print package size.
 * @todo rpmio: use fdSize rather than fstat(2) to get file size.
 * @param fd			package file handle
 * @param siglen		signature header size
 * @param pad			signature padding
 * @param datalen		length of header+payload
 * @return 			rpmRC return code
 */
static inline rpmRC printSize(FD_t fd, int siglen, int pad, int datalen)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    struct stat st;
    int fdno = Fileno(fd);

    /* HACK: workaround for davRead wiring. */
    if (fdno == 123456789) {
	st.st_size = 0;
/*@-sizeoftype@*/
	st.st_size -= sizeof(struct rpmlead)+siglen+pad+datalen;
/*@=sizeoftype@*/
    } else if (fstat(fdno, &st) < 0)
	return RPMRC_FAIL;

/*@-sizeoftype@*/
    rpmMessage(RPMMESS_DEBUG,
	_("Expected size: %12d = lead(%d)+sigs(%d)+pad(%d)+data(%d)\n"),
		(int)sizeof(struct rpmlead)+siglen+pad+datalen,
		(int)sizeof(struct rpmlead), siglen, pad, datalen);
/*@=sizeoftype@*/
    rpmMessage(RPMMESS_DEBUG,
	_("  Actual size: %12d\n"), (int)st.st_size);

    return RPMRC_OK;
}

/*@unchecked@*/
static unsigned char header_magic[8] = {
    0x8e, 0xad, 0xe8, 0x01, 0x00, 0x00, 0x00, 0x00
};

rpmRC rpmReadSignature(FD_t fd, Header * sighp, sigType sig_type,
		const char ** msg)
{
    char buf[BUFSIZ];
    int_32 block[4];
    int_32 il;
    int_32 dl;
    int_32 * ei = NULL;
    entryInfo pe;
    size_t nb;
    int_32 ril = 0;
    indexEntry entry = memset(alloca(sizeof(*entry)), 0, sizeof(*entry));
    entryInfo info = memset(alloca(sizeof(*info)), 0, sizeof(*info));
    unsigned char * dataStart;
    unsigned char * dataEnd = NULL;
    Header sigh = NULL;
    rpmRC rc = RPMRC_FAIL;		/* assume failure */
    int xx;
    int i;

/*@-boundswrite@*/
    if (sighp)
	*sighp = NULL;

    buf[0] = '\0';
/*@=boundswrite@*/

    if (sig_type != RPMSIGTYPE_HEADERSIG)
	goto exit;

    memset(block, 0, sizeof(block));
    if ((xx = timedRead(fd, (char *)block, sizeof(block))) != sizeof(block)) {
	(void) snprintf(buf, sizeof(buf),
		_("sigh size(%d): BAD, read returned %d\n"), (int)sizeof(block), xx);
	goto exit;
    }
    if (memcmp(block, header_magic, sizeof(header_magic))) {
	(void) snprintf(buf, sizeof(buf),
		_("sigh magic: BAD\n"));
	goto exit;
    }
/*@-boundsread@*/
    il = ntohl(block[2]);
/*@=boundsread@*/
    if (il < 0 || il > 32) {
	(void) snprintf(buf, sizeof(buf),
		_("sigh tags: BAD, no. of tags(%d) out of range\n"), il);
	goto exit;
    }
/*@-boundsread@*/
    dl = ntohl(block[3]);
/*@=boundsread@*/
    if (dl < 0 || dl > 8192) {
	(void) snprintf(buf, sizeof(buf),
		_("sigh data: BAD, no. of  bytes(%d) out of range\n"), dl);
	goto exit;
    }

/*@-sizeoftype@*/
    nb = (il * sizeof(struct entryInfo_s)) + dl;
/*@=sizeoftype@*/
    ei = xmalloc(sizeof(il) + sizeof(dl) + nb);
/*@-bounds@*/
    ei[0] = block[2];
    ei[1] = block[3];
    pe = (entryInfo) &ei[2];
/*@=bounds@*/
    dataStart = (unsigned char *) (pe + il);
    if ((xx = timedRead(fd, (char *)pe, nb)) != nb) {
	(void) snprintf(buf, sizeof(buf),
		_("sigh blob(%d): BAD, read returned %d\n"), (int)nb, xx);
	goto exit;
    }
    
    /* Check (and convert) the 1st tag element. */
    xx = headerVerifyInfo(1, dl, pe, &entry->info, 0);
    if (xx != -1) {
	(void) snprintf(buf, sizeof(buf),
		_("tag[%d]: BAD, tag %d type %d offset %d count %d\n"),
		0, entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	goto exit;
    }

    /* Is there an immutable header region tag? */
/*@-sizeoftype@*/
    if (entry->info.tag == RPMTAG_HEADERSIGNATURES
       && entry->info.type == RPM_BIN_TYPE
       && entry->info.count == REGION_TAG_COUNT)
    {
/*@=sizeoftype@*/

	if (entry->info.offset >= dl) {
	    (void) snprintf(buf, sizeof(buf),
		_("region offset: BAD, tag %d type %d offset %d count %d\n"),
		entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	    goto exit;
	}

	/* Is there an immutable header region tag trailer? */
	dataEnd = dataStart + entry->info.offset;
/*@-sizeoftype@*/
/*@-bounds@*/
	(void) memcpy(info, dataEnd, REGION_TAG_COUNT);
	/* XXX Really old packages have HEADER_IMAGE, not HEADER_SIGNATURES. */
	if (info->tag == htonl(RPMTAG_HEADERIMAGE)) {
	    int_32 stag = htonl(RPMTAG_HEADERSIGNATURES);
	    info->tag = stag;
	    memcpy(dataEnd, &stag, sizeof(stag));
	}
/*@=bounds@*/
	dataEnd += REGION_TAG_COUNT;

	xx = headerVerifyInfo(1, dl, info, &entry->info, 1);
	if (xx != -1 ||
	    !(entry->info.tag == RPMTAG_HEADERSIGNATURES
	   && entry->info.type == RPM_BIN_TYPE
	   && entry->info.count == REGION_TAG_COUNT))
	{
	    (void) snprintf(buf, sizeof(buf),
		_("region trailer: BAD, tag %d type %d offset %d count %d\n"),
		entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	    goto exit;
	}
/*@=sizeoftype@*/
/*@-boundswrite@*/
	memset(info, 0, sizeof(*info));
/*@=boundswrite@*/

	/* Is the no. of tags in the region less than the total no. of tags? */
	ril = entry->info.offset/sizeof(*pe);
	if ((entry->info.offset % sizeof(*pe)) || ril > il) {
	    (void) snprintf(buf, sizeof(buf),
		_("region size: BAD, ril(%d) > il(%d)\n"), ril, il);
	    goto exit;
	}
    }

    /* Sanity check signature tags */
/*@-boundswrite@*/
    memset(info, 0, sizeof(*info));
/*@=boundswrite@*/
    for (i = 1; i < il; i++) {
	xx = headerVerifyInfo(1, dl, pe+i, &entry->info, 0);
	if (xx != -1) {
	    (void) snprintf(buf, sizeof(buf),
		_("sigh tag[%d]: BAD, tag %d type %d offset %d count %d\n"),
		i, entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	    goto exit;
	}
    }

    /* OK, blob looks sane, load the header. */
    sigh = headerLoad(ei);
    if (sigh == NULL) {
	(void) snprintf(buf, sizeof(buf), _("sigh load: BAD\n"));
	goto exit;
    }
    sigh->flags |= HEADERFLAG_ALLOCATED;

    {	int sigSize = headerSizeof(sigh, HEADER_MAGIC_YES);
	int pad = (8 - (sigSize % 8)) % 8; /* 8-byte pad */
	int_32 * archSize = NULL;

	/* Position at beginning of header. */
	if (pad && (xx = timedRead(fd, (char *)block, pad)) != pad) {
	    (void) snprintf(buf, sizeof(buf),
		_("sigh pad(%d): BAD, read %d bytes\n"), pad, xx);
	    goto exit;
	}

	/* Print package component sizes. */
	if (headerGetEntry(sigh, RPMSIGTAG_SIZE, NULL,(void **)&archSize, NULL)) {
	    rc = printSize(fd, sigSize, pad, *archSize);
	    if (rc != RPMRC_OK)
		(void) snprintf(buf, sizeof(buf),
			_("sigh sigSize(%d): BAD, fstat(2) failed\n"), sigSize);
	}
    }

exit:
/*@-boundswrite@*/
    if (sighp && sigh && rc == RPMRC_OK)
	*sighp = headerLink(sigh);
    sigh = headerFree(sigh);

    if (msg != NULL) {
	buf[sizeof(buf)-1] = '\0';
	*msg = xstrdup(buf);
    }
/*@=boundswrite@*/

    return rc;
}

int rpmWriteSignature(FD_t fd, Header sigh)
{
    static byte buf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int sigSize, pad;
    int rc;

    rc = headerWrite(fd, sigh, HEADER_MAGIC_YES);
    if (rc)
	return rc;

    sigSize = headerSizeof(sigh, HEADER_MAGIC_YES);
    pad = (8 - (sigSize % 8)) % 8;
    if (pad) {
/*@-boundswrite@*/
	if (Fwrite(buf, sizeof(buf[0]), pad, fd) != pad)
	    rc = 1;
/*@=boundswrite@*/
    }
    rpmMessage(RPMMESS_DEBUG, _("Signature: size(%d)+pad(%d)\n"), sigSize, pad);
    return rc;
}

Header rpmNewSignature(void)
{
    Header sigh = headerNew();
    return sigh;
}

Header rpmFreeSignature(Header sigh)
{
    return headerFree(sigh);
}

/**
 * Generate PGP signature(s) for a header+payload file.
 * @param file		header+payload file name
 * @retval *sigTagp	signature tag
 * @retval *pktp	signature packet(s)
 * @retval *pktlenp	signature packet(s) length
 * @param passPhrase	private key pass phrase
 * @return		0 on success, 1 on failure
 */
static int makePGPSignature(const char * file, int_32 * sigTagp,
		/*@out@*/ byte ** pktp, /*@out@*/ int_32 * pktlenp,
		/*@null@*/ const char * passPhrase)
	/*@globals errno, rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies errno, *pktp, *pktlenp, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    char * sigfile = alloca(1024);
    int pid, status;
    int inpipe[2];
    struct stat st;
    const char * cmd;
    char *const *av;
    pgpDig dig = NULL;
    pgpDigParams sigp = NULL;
    int rc;

/*@-boundswrite@*/
    (void) stpcpy( stpcpy(sigfile, file), ".sig");
/*@=boundswrite@*/

    addMacro(NULL, "__plaintext_filename", NULL, file, -1);
    addMacro(NULL, "__signature_filename", NULL, sigfile, -1);

    inpipe[0] = inpipe[1] = 0;
/*@-boundsread@*/
    (void) pipe(inpipe);
/*@=boundsread@*/

    if (!(pid = fork())) {
	const char *pgp_path = rpmExpand("%{?_pgp_path}", NULL);
	const char *path;
	pgpVersion pgpVer;

	(void) close(STDIN_FILENO);
	(void) dup2(inpipe[0], 3);
	(void) close(inpipe[1]);

	(void) dosetenv("PGPPASSFD", "3", 1);
/*@-boundsread@*/
	if (pgp_path && *pgp_path != '\0')
	    (void) dosetenv("PGPPATH", pgp_path, 1);
/*@=boundsread@*/

	/* dosetenv("PGPPASS", passPhrase, 1); */

	unsetenv("MALLOC_CHECK_");
	if ((path = rpmDetectPGPVersion(&pgpVer)) != NULL) {
	    switch(pgpVer) {
	    case PGP_2:
		cmd = rpmExpand("%{?__pgp_sign_cmd}", NULL);
		rc = poptParseArgvString(cmd, NULL, (const char ***)&av);
/*@-boundsread@*/
		if (!rc)
		    rc = execve(av[0], av+1, environ);
/*@=boundsread@*/
		break;
	    case PGP_5:
		cmd = rpmExpand("%{?__pgp5_sign_cmd}", NULL);
		rc = poptParseArgvString(cmd, NULL, (const char ***)&av);
/*@-boundsread@*/
		if (!rc)
		    rc = execve(av[0], av+1, environ);
/*@=boundsread@*/
		break;
	    case PGP_UNKNOWN:
	    case PGP_NOTDETECTED:
		errno = ENOENT;
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

/*@-boundswrite@*/
    *pktlenp = st.st_size;
    rpmMessage(RPMMESS_DEBUG, _("PGP sig size: %d\n"), *pktlenp);
    *pktp = xmalloc(*pktlenp);
/*@=boundswrite@*/

/*@-boundsread@*/
    {	FD_t fd;

	rc = 0;
	fd = Fopen(sigfile, "r.fdio");
	if (fd != NULL && !Ferror(fd)) {
	    rc = timedRead(fd, *pktp, *pktlenp);
	    if (sigfile) (void) unlink(sigfile);
	    (void) Fclose(fd);
	}
	if (rc != *pktlenp) {
/*@-boundswrite@*/
	    *pktp = _free(*pktp);
/*@=boundswrite@*/
	    rpmError(RPMERR_SIGGEN, _("unable to read the signature\n"));
	    return 1;
	}
    }

    rpmMessage(RPMMESS_DEBUG, _("Got %d bytes of PGP sig\n"), *pktlenp);
/*@=boundsread@*/

#ifdef	NOTYET
    /* Parse the signature, change signature tag as appropriate. */
    dig = pgpNewDig();

    (void) pgpPrtPkts(*pktp, *pktlenp, dig, 0);
    sigp = &dig->signature;

    dig = pgpFreeDig(dig);
#endif

    return 0;
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
static int makeGPGSignature(const char * file, int_32 * sigTagp,
		/*@out@*/ byte ** pktp, /*@out@*/ int_32 * pktlenp,
		/*@null@*/ const char * passPhrase)
	/*@globals rpmGlobalMacroContext, h_errno,
		fileSystem, internalState @*/
	/*@modifies *pktp, *pktlenp, rpmGlobalMacroContext,
		fileSystem, internalState @*/
{
    char * sigfile = alloca(strlen(file)+sizeof(".sig"));
    int pid, status;
    int inpipe[2];
    FILE * fpipe;
    struct stat st;
    const char * cmd;
    char *const *av;
    pgpDig dig = NULL;
    pgpDigParams sigp = NULL;
    int rc;

/*@-boundswrite@*/
    (void) stpcpy( stpcpy(sigfile, file), ".sig");
/*@=boundswrite@*/

    addMacro(NULL, "__plaintext_filename", NULL, file, -1);
    addMacro(NULL, "__signature_filename", NULL, sigfile, -1);

    inpipe[0] = inpipe[1] = 0;
/*@-boundsread@*/
    (void) pipe(inpipe);
/*@=boundsread@*/

    if (!(pid = fork())) {
	const char *gpg_path = rpmExpand("%{?_gpg_path}", NULL);

	(void) close(STDIN_FILENO);
	(void) dup2(inpipe[0], 3);
	(void) close(inpipe[1]);

/*@-boundsread@*/
	if (gpg_path && *gpg_path != '\0')
	    (void) dosetenv("GNUPGHOME", gpg_path, 1);
/*@=boundsread@*/

	unsetenv("MALLOC_CHECK_");
	cmd = rpmExpand("%{?__gpg_sign_cmd}", NULL);
	rc = poptParseArgvString(cmd, NULL, (const char ***)&av);
/*@-boundsread@*/
	if (!rc)
	    rc = execve(av[0], av+1, environ);
/*@=boundsread@*/

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
	rpmError(RPMERR_SIGGEN, _("gpg exec failed (%d)\n"), WEXITSTATUS(status));
	return 1;
    }

    if (stat(sigfile, &st)) {
	/* GPG failed to write signature */
	if (sigfile) (void) unlink(sigfile);  /* Just in case */
	rpmError(RPMERR_SIGGEN, _("gpg failed to write signature\n"));
	return 1;
    }

/*@-boundswrite@*/
    *pktlenp = st.st_size;
    rpmMessage(RPMMESS_DEBUG, _("GPG sig size: %d\n"), *pktlenp);
    *pktp = xmalloc(*pktlenp);
/*@=boundswrite@*/

/*@-boundsread@*/
    {	FD_t fd;

	rc = 0;
	fd = Fopen(sigfile, "r.fdio");
	if (fd != NULL && !Ferror(fd)) {
	    rc = timedRead(fd, *pktp, *pktlenp);
	    if (sigfile) (void) unlink(sigfile);
	    (void) Fclose(fd);
	}
	if (rc != *pktlenp) {
/*@-boundswrite@*/
	    *pktp = _free(*pktp);
/*@=boundswrite@*/
	    rpmError(RPMERR_SIGGEN, _("unable to read the signature\n"));
	    return 1;
	}
    }

    rpmMessage(RPMMESS_DEBUG, _("Got %d bytes of GPG sig\n"), *pktlenp);
/*@=boundsread@*/

    /* Parse the signature, change signature tag as appropriate. */
    dig = pgpNewDig();

    (void) pgpPrtPkts(*pktp, *pktlenp, dig, 0);
    sigp = &dig->signature;

    switch (*sigTagp) {
    case RPMSIGTAG_SIZE:
    case RPMSIGTAG_MD5:
    case RPMSIGTAG_SHA1:
	break;
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
    }

    dig = pgpFreeDig(dig);

    return 0;
}

/**
 * Generate header only signature(s) from a header+payload file.
 * @param sigh		signature header
 * @param file		header+payload file name
 * @param sigTag	type of signature(s) to add
 * @param passPhrase	private key pass phrase
 * @return		0 on success, -1 on failure
 */
static int makeHDRSignature(Header sigh, const char * file, int_32 sigTag,
		/*@null@*/ const char * passPhrase)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies sigh, rpmGlobalMacroContext, fileSystem, internalState @*/
{
    Header h = NULL;
    FD_t fd = NULL;
    byte * pkt;
    int_32 pktlen;
    const char * fn = NULL;
    const char * SHA1 = NULL;
    int ret = -1;	/* assume failure. */

    switch (sigTag) {
    case RPMSIGTAG_SIZE:
    case RPMSIGTAG_MD5:
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
    case RPMSIGTAG_GPG:
	goto exit;
	/*@notreached@*/ break;
    case RPMSIGTAG_SHA1:
	fd = Fopen(file, "r.fdio");
	if (fd == NULL || Ferror(fd))
	    goto exit;
	h = headerRead(fd, HEADER_MAGIC_YES);
	if (h == NULL)
	    goto exit;
	(void) Fclose(fd);	fd = NULL;

	if (headerIsEntry(h, RPMTAG_HEADERIMMUTABLE)) {
	    DIGEST_CTX ctx;
	    void * uh;
	    int_32 uht, uhc;
	
	    if (!headerGetEntry(h, RPMTAG_HEADERIMMUTABLE, &uht, &uh, &uhc)
	     ||  uh == NULL)
	    {
		h = headerFree(h);
		goto exit;
	    }
	    ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
	    (void) rpmDigestUpdate(ctx, header_magic, sizeof(header_magic));
	    (void) rpmDigestUpdate(ctx, uh, uhc);
	    (void) rpmDigestFinal(ctx, (void **)&SHA1, NULL, 1);
	    uh = headerFreeData(uh, uht);
	}
	h = headerFree(h);

	if (SHA1 == NULL)
	    goto exit;
	if (!headerAddEntry(sigh, RPMSIGTAG_SHA1, RPM_STRING_TYPE, SHA1, 1))
	    goto exit;
	ret = 0;
	break;
    case RPMSIGTAG_DSA:
	fd = Fopen(file, "r.fdio");
	if (fd == NULL || Ferror(fd))
	    goto exit;
	h = headerRead(fd, HEADER_MAGIC_YES);
	if (h == NULL)
	    goto exit;
	(void) Fclose(fd);	fd = NULL;
	if (makeTempFile(NULL, &fn, &fd))
	    goto exit;
	if (headerWrite(fd, h, HEADER_MAGIC_YES))
	    goto exit;
	(void) Fclose(fd);	fd = NULL;
	if (makeGPGSignature(fn, &sigTag, &pkt, &pktlen, passPhrase)
	 || !headerAddEntry(sigh, sigTag, RPM_BIN_TYPE, pkt, pktlen))
	    goto exit;
	ret = 0;
	break;
    case RPMSIGTAG_RSA:
	fd = Fopen(file, "r.fdio");
	if (fd == NULL || Ferror(fd))
	    goto exit;
	h = headerRead(fd, HEADER_MAGIC_YES);
	if (h == NULL)
	    goto exit;
	(void) Fclose(fd);	fd = NULL;
	if (makeTempFile(NULL, &fn, &fd))
	    goto exit;
	if (headerWrite(fd, h, HEADER_MAGIC_YES))
	    goto exit;
	(void) Fclose(fd);	fd = NULL;
	if (makePGPSignature(fn, &sigTag, &pkt, &pktlen, passPhrase)
	 || !headerAddEntry(sigh, sigTag, RPM_BIN_TYPE, pkt, pktlen))
	    goto exit;
	ret = 0;
	break;
    }

exit:
    if (fn) {
	(void) unlink(fn);
	fn = _free(fn);
    }
    SHA1 = _free(SHA1);
    h = headerFree(h);
    if (fd != NULL) (void) Fclose(fd);
    return ret;
}

int rpmAddSignature(Header sigh, const char * file, int_32 sigTag,
		const char * passPhrase)
{
    struct stat st;
    byte * pkt;
    int_32 pktlen;
    int ret = -1;	/* assume failure. */

    switch (sigTag) {
    case RPMSIGTAG_SIZE:
	if (stat(file, &st) != 0)
	    break;
	pktlen = st.st_size;
	if (!headerAddEntry(sigh, sigTag, RPM_INT32_TYPE, &pktlen, 1))
	    break;
	ret = 0;
	break;
    case RPMSIGTAG_MD5:
	pktlen = 16;
	pkt = memset(alloca(pktlen), 0, pktlen);
	if (domd5(file, pkt, 0, NULL)
	 || !headerAddEntry(sigh, sigTag, RPM_BIN_TYPE, pkt, pktlen))
	    break;
	ret = 0;
	break;
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
	if (makePGPSignature(file, &sigTag, &pkt, &pktlen, passPhrase)
	 || !headerAddEntry(sigh, sigTag, RPM_BIN_TYPE, pkt, pktlen))
	    break;
#ifdef	NOTYET	/* XXX needs hdrmd5ctx, like hdrsha1ctx. */
	/* XXX Piggyback a header-only RSA signature as well. */
	ret = makeHDRSignature(sigh, file, RPMSIGTAG_RSA, passPhrase);
#endif
	ret = 0;
	break;
    case RPMSIGTAG_GPG:
	if (makeGPGSignature(file, &sigTag, &pkt, &pktlen, passPhrase)
	 || !headerAddEntry(sigh, sigTag, RPM_BIN_TYPE, pkt, pktlen))
	    break;
	/* XXX Piggyback a header-only DSA signature as well. */
	ret = makeHDRSignature(sigh, file, RPMSIGTAG_DSA, passPhrase);
	break;
    case RPMSIGTAG_RSA:
    case RPMSIGTAG_DSA:
    case RPMSIGTAG_SHA1:
	ret = makeHDRSignature(sigh, file, sigTag, passPhrase);
	break;
    }

    return ret;
}

static int checkPassPhrase(const char * passPhrase, const int sigTag)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies rpmGlobalMacroContext, fileSystem, internalState @*/
{
    int passPhrasePipe[2];
    int pid, status;
    int rc;
    int xx;

    passPhrasePipe[0] = passPhrasePipe[1] = 0;
/*@-boundsread@*/
    xx = pipe(passPhrasePipe);
/*@=boundsread@*/
    if (!(pid = fork())) {
	const char * cmd;
	char *const *av;
	int fdno;

	xx = close(STDIN_FILENO);
	xx = close(STDOUT_FILENO);
	xx = close(passPhrasePipe[1]);
	if (! rpmIsVerbose())
	    xx = close(STDERR_FILENO);
	if ((fdno = open("/dev/null", O_RDONLY)) != STDIN_FILENO) {
	    xx = dup2(fdno, STDIN_FILENO);
	    xx = close(fdno);
	}
	if ((fdno = open("/dev/null", O_WRONLY)) != STDOUT_FILENO) {
	    xx = dup2(fdno, STDOUT_FILENO);
	    xx = close(fdno);
	}
	xx = dup2(passPhrasePipe[0], 3);

	unsetenv("MALLOC_CHECK_");
	switch (sigTag) {
	case RPMSIGTAG_DSA:
	case RPMSIGTAG_GPG:
	{   const char *gpg_path = rpmExpand("%{?_gpg_path}", NULL);

/*@-boundsread@*/
	    if (gpg_path && *gpg_path != '\0')
  		(void) dosetenv("GNUPGHOME", gpg_path, 1);
/*@=boundsread@*/

	    cmd = rpmExpand("%{?__gpg_check_password_cmd}", NULL);
	    rc = poptParseArgvString(cmd, NULL, (const char ***)&av);
/*@-boundsread@*/
	    if (!rc)
		rc = execve(av[0], av+1, environ);
/*@=boundsread@*/

	    rpmError(RPMERR_EXEC, _("Could not exec %s: %s\n"), "gpg",
			strerror(errno));
	}   /*@notreached@*/ break;
	case RPMSIGTAG_RSA:
	case RPMSIGTAG_PGP5:	/* XXX legacy */
	case RPMSIGTAG_PGP:
	{   const char *pgp_path = rpmExpand("%{?_pgp_path}", NULL);
	    const char *path;
	    pgpVersion pgpVer;

	    (void) dosetenv("PGPPASSFD", "3", 1);
/*@-boundsread@*/
	    if (pgp_path && *pgp_path != '\0')
		xx = dosetenv("PGPPATH", pgp_path, 1);
/*@=boundsread@*/

	    if ((path = rpmDetectPGPVersion(&pgpVer)) != NULL) {
		switch(pgpVer) {
		case PGP_2:
		    cmd = rpmExpand("%{?__pgp_check_password_cmd}", NULL);
		    rc = poptParseArgvString(cmd, NULL, (const char ***)&av);
/*@-boundsread@*/
		    if (!rc)
			rc = execve(av[0], av+1, environ);
/*@=boundsread@*/
		    /*@innerbreak@*/ break;
		case PGP_5:	/* XXX legacy */
		    cmd = rpmExpand("%{?__pgp5_check_password_cmd}", NULL);
		    rc = poptParseArgvString(cmd, NULL, (const char ***)&av);
/*@-boundsread@*/
		    if (!rc)
			rc = execve(av[0], av+1, environ);
/*@=boundsread@*/
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

    xx = close(passPhrasePipe[0]);
    xx = write(passPhrasePipe[1], passPhrase, strlen(passPhrase));
    xx = write(passPhrasePipe[1], "\n", 1);
    xx = close(passPhrasePipe[1]);

    (void) waitpid(pid, &status, 0);

    return ((!WIFEXITED(status) || WEXITSTATUS(status)) ? 1 : 0);
}

char * rpmGetPassPhrase(const char * prompt, const int sigTag)
{
    char *pass = NULL;
    int aok = 0;

    switch (sigTag) {
    case RPMSIGTAG_DSA:
    case RPMSIGTAG_GPG:
/*@-boundsread@*/
      { const char *name = rpmExpand("%{?_gpg_name}", NULL);
	aok = (name && *name != '\0');
	name = _free(name);
      }
/*@=boundsread@*/
	if (aok)
	    break;
	rpmError(RPMERR_SIGGEN,
		_("You must set \"%%_gpg_name\" in your macro file\n"));
	break;
    case RPMSIGTAG_RSA:
    case RPMSIGTAG_PGP5: 	/* XXX legacy */
    case RPMSIGTAG_PGP:
/*@-boundsread@*/
      { const char *name = rpmExpand("%{?_pgp_name}", NULL);
	aok = (name && *name != '\0');
	name = _free(name);
      }
/*@=boundsread@*/
	if (aok)
	    break;
	rpmError(RPMERR_SIGGEN,
		_("You must set \"%%_pgp_name\" in your macro file\n"));
	break;
    default:
	/* Currently the calling function (rpm.c:main) is checking this and
	 * doing a better job.  This section should never be accessed.
	 */
	rpmError(RPMERR_SIGGEN, _("Invalid %%_signature spec in macro file\n"));
	break;
    }

    if (aok) {
/*@-moduncon -nullpass -unrecog @*/
	pass = getpass( (prompt ? prompt : "") );
/*@=moduncon =nullpass =unrecog @*/

	if (checkPassPhrase(pass, sigTag))
	    pass = NULL;
    }

    return pass;
}

static /*@observer@*/ const char * rpmSigString(rpmRC res)
	/*@*/
{
    const char * str;
    switch (res) {
    case RPMRC_OK:		str = "OK";		break;
    case RPMRC_FAIL:		str = "BAD";		break;
    case RPMRC_NOKEY:		str = "NOKEY";		break;
    case RPMRC_NOTTRUSTED:	str = "NOTRUSTED";	break;
    default:
    case RPMRC_NOTFOUND:	str = "UNKNOWN";	break;
    }
    return str;
}

/*@-boundswrite@*/
static rpmRC
verifySizeSignature(const rpmts ts, /*@out@*/ char * t)
	/*@modifies *t @*/
{
    const void * sig = rpmtsSig(ts);
    pgpDig dig = rpmtsDig(ts);
    rpmRC res;
    int_32 size = 0x7fffffff;

    *t = '\0';
    t = stpcpy(t, _("Header+Payload size: "));

    if (sig == NULL || dig == NULL || dig->nbytes == 0) {
	res = RPMRC_NOKEY;
	t = stpcpy(t, rpmSigString(res));
	goto exit;
    }

    memcpy(&size, sig, sizeof(size));

    if (size != dig->nbytes) {
	res = RPMRC_FAIL;
	t = stpcpy(t, rpmSigString(res));
	sprintf(t, " Expected(%d) != (%d)\n", (int)size, (int)dig->nbytes);
    } else {
	res = RPMRC_OK;
	t = stpcpy(t, rpmSigString(res));
	sprintf(t, " (%d)", (int)dig->nbytes);
    }

exit:
    t = stpcpy(t, "\n");
    return res;
}
/*@=boundswrite@*/

/*@-boundswrite@*/
static rpmRC
verifyMD5Signature(const rpmts ts, /*@out@*/ char * t,
		/*@null@*/ DIGEST_CTX md5ctx)
	/*@globals internalState @*/
	/*@modifies *t, internalState @*/
{
    const void * sig = rpmtsSig(ts);
    int_32 siglen = rpmtsSiglen(ts);
    pgpDig dig = rpmtsDig(ts);
    rpmRC res;
    byte * md5sum = NULL;
    size_t md5len = 0;

    *t = '\0';
    t = stpcpy(t, _("MD5 digest: "));

    if (md5ctx == NULL || sig == NULL || dig == NULL) {
	res = RPMRC_NOKEY;
	t = stpcpy(t, rpmSigString(res));
	goto exit;
    }

    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_DIGEST), 0);
    (void) rpmDigestFinal(rpmDigestDup(md5ctx),
		(void **)&md5sum, &md5len, 0);
    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DIGEST), 0);
    rpmtsOp(ts, RPMTS_OP_DIGEST)->count--;	/* XXX one too many */

    if (md5len != siglen || memcmp(md5sum, sig, md5len)) {
	res = RPMRC_FAIL;
	t = stpcpy(t, rpmSigString(res));
	t = stpcpy(t, " Expected(");
	(void) pgpHexCvt(t, sig, siglen);
	t += strlen(t);
	t = stpcpy(t, ") != (");
    } else {
	res = RPMRC_OK;
	t = stpcpy(t, rpmSigString(res));
	t = stpcpy(t, " (");
    }
    (void) pgpHexCvt(t, md5sum, md5len);
    t += strlen(t);
    t = stpcpy(t, ")");

exit:
    md5sum = _free(md5sum);
    t = stpcpy(t, "\n");
    return res;
}
/*@=boundswrite@*/

/*@-boundswrite@*/
/**
 * Verify header immutable region SHA1 digest.
 * @param ts		transaction set
 * @retval t		verbose success/failure text
 * @param sha1ctx
 * @return 		RPMRC_OK on success
 */
static rpmRC
verifySHA1Signature(const rpmts ts, /*@out@*/ char * t,
		/*@null@*/ DIGEST_CTX sha1ctx)
	/*@globals internalState @*/
	/*@modifies *t, internalState @*/
{
    const void * sig = rpmtsSig(ts);
#ifdef	NOTYET
    int_32 siglen = rpmtsSiglen(ts);
#endif
    pgpDig dig = rpmtsDig(ts);
    rpmRC res;
    const char * SHA1 = NULL;

    *t = '\0';
    t = stpcpy(t, _("Header SHA1 digest: "));

    if (sha1ctx == NULL || sig == NULL || dig == NULL) {
	res = RPMRC_NOKEY;
	t = stpcpy(t, rpmSigString(res));
	goto exit;
    }

    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_DIGEST), 0);
    (void) rpmDigestFinal(rpmDigestDup(sha1ctx),
		(void **)&SHA1, NULL, 1);
    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DIGEST), 0);

    if (SHA1 == NULL || strlen(SHA1) != strlen(sig) || strcmp(SHA1, sig)) {
	res = RPMRC_FAIL;
	t = stpcpy(t, rpmSigString(res));
	t = stpcpy(t, " Expected(");
	t = stpcpy(t, sig);
	t = stpcpy(t, ") != (");
    } else {
	res = RPMRC_OK;
	t = stpcpy(t, rpmSigString(res));
	t = stpcpy(t, " (");
    }
    if (SHA1)
	t = stpcpy(t, SHA1);
    t = stpcpy(t, ")");

exit:
    SHA1 = _free(SHA1);
    t = stpcpy(t, "\n");
    return res;
}
/*@=boundswrite@*/

/**
 * Convert hex to binary nibble.
 * @param c            hex character
 * @return             binary nibble
 */
static inline unsigned char nibble(char c)
	/*@*/
{
    if (c >= '0' && c <= '9')
	return (c - '0');
    if (c >= 'A' && c <= 'F')
	return (c - 'A') + 10;
    if (c >= 'a' && c <= 'f')
	return (c - 'a') + 10;
    return 0;
}

/*@-boundswrite@*/
/**
 * Verify RSA signature.
 * @param ts		transaction set
 * @retval t		verbose success/failure text
 * @param md5ctx
 * @return 		RPMRC_OK on success
 */
static rpmRC
verifyRSASignature(rpmts ts, /*@out@*/ char * t,
		/*@null@*/ DIGEST_CTX md5ctx)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, *t, rpmGlobalMacroContext, fileSystem, internalState */
{
    const void * sig = rpmtsSig(ts);
#ifdef	NOTYET
    int_32 siglen = rpmtsSiglen(ts);
#endif
    int_32 sigtag = rpmtsSigtag(ts);
    pgpDig dig = rpmtsDig(ts);
    pgpDigParams sigp = rpmtsSignature(ts);
    const char * prefix = NULL;
    rpmRC res = RPMRC_OK;
    int xx;

    *t = '\0';
    if (dig != NULL && dig->hdrmd5ctx == md5ctx)
	t = stpcpy(t, _("Header "));
    *t++ = 'V';
    switch (sigp->version) {
    case 3:	*t++ = '3';	break;
    case 4:	*t++ = '4';	break;
    }

    if (md5ctx == NULL || sig == NULL || dig == NULL || sigp == NULL) {
	res = RPMRC_NOKEY;
    }

    /* Verify the desired signature match. */
    switch (sigp->pubkey_algo) {
    case PGPPUBKEYALGO_RSA:
	if (sigtag == RPMSIGTAG_PGP || sigtag == RPMSIGTAG_PGP5 || sigtag == RPMSIGTAG_RSA)
	    break;
	/*@fallthrough@*/
    default:
	res = RPMRC_NOKEY;
	break;
    }

    /* Verify the desired hash match. */
    /* XXX Values from PKCS#1 v2.1 (aka RFC-3447) */
    switch (sigp->hash_algo) {
    case PGPHASHALGO_MD5:
	t = stpcpy(t, " RSA/MD5");
	prefix = "3020300c06082a864886f70d020505000410";
	break;
    case PGPHASHALGO_SHA1:
	t = stpcpy(t, " RSA/SHA1");
	prefix = "3021300906052b0e03021a05000414";
	break;
    case PGPHASHALGO_RIPEMD160:
	res = RPMRC_NOKEY;
	prefix = NULL;
	break;
    case PGPHASHALGO_MD2:
	t = stpcpy(t, " RSA/MD2");
	prefix = "3020300c06082a864886f70d020205000410";
	break;
    case PGPHASHALGO_TIGER192:
	res = RPMRC_NOKEY;
	prefix = NULL;
	break;
    case PGPHASHALGO_HAVAL_5_160:
	res = RPMRC_NOKEY;
	prefix = NULL;
	break;
    case PGPHASHALGO_SHA256:
	t = stpcpy(t, " RSA/SHA256");
	prefix = "3031300d060960864801650304020105000420";
	break;
    case PGPHASHALGO_SHA384:
	t = stpcpy(t, " RSA/SHA384");
	prefix = "3041300d060960864801650304020205000430";
	break;
    case PGPHASHALGO_SHA512:
	t = stpcpy(t, " RSA/SHA512");
	prefix = "3051300d060960864801650304020305000440";
	break;
    default:
	res = RPMRC_NOKEY;
	prefix = NULL;
	break;
    }

    t = stpcpy(t, _(" signature: "));
    if (res != RPMRC_OK) {
	goto exit;
    }

    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_DIGEST), 0);
    {	DIGEST_CTX ctx = rpmDigestDup(md5ctx);
	byte signhash16[2];
	const char * s;

	if (sigp->hash != NULL)
	    xx = rpmDigestUpdate(ctx, sigp->hash, sigp->hashlen);

#ifdef	NOTYET	/* XXX not for binary/text signatures as in packages. */
	if (!(sigp->sigtype == PGPSIGTYPE_BINARY || sigp->sigtype == PGP_SIGTYPE_TEXT)) {
	    int nb = dig->nbytes + sigp->hashlen;
	    byte trailer[6];
	    nb = htonl(nb);
	    trailer[0] = 0x4;
	    trailer[1] = 0xff;
	    memcpy(trailer+2, &nb, sizeof(nb));
	    xx = rpmDigestUpdate(ctx, trailer, sizeof(trailer));
	}
#endif

	xx = rpmDigestFinal(ctx, (void **)&dig->md5, &dig->md5len, 1);
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DIGEST), sigp->hashlen);
	rpmtsOp(ts, RPMTS_OP_DIGEST)->count--;	/* XXX one too many */

	/* Compare leading 16 bits of digest for quick check. */
	s = dig->md5;
	signhash16[0] = (nibble(s[0]) << 4) | nibble(s[1]);
	signhash16[1] = (nibble(s[2]) << 4) | nibble(s[3]);
	if (memcmp(signhash16, sigp->signhash16, sizeof(signhash16))) {
	    res = RPMRC_FAIL;
	    goto exit;
	}
    }

    /* Generate RSA modulus parameter. */
    {	unsigned int nbits = MP_WORDS_TO_BITS(dig->c.size);
	unsigned int nb = (nbits + 7) >> 3;
	const char * hexstr;
	char * tt;

assert(prefix != NULL);
	hexstr = tt = xmalloc(2 * nb + 1);
	memset(tt, 'f', (2 * nb));
	tt[0] = '0'; tt[1] = '0';
	tt[2] = '0'; tt[3] = '1';
	tt += (2 * nb) - strlen(prefix) - strlen(dig->md5) - 2;
	*tt++ = '0'; *tt++ = '0';
	tt = stpcpy(tt, prefix);
	tt = stpcpy(tt, dig->md5);

	mpnzero(&dig->rsahm);	(void) mpnsethex(&dig->rsahm, hexstr);

	hexstr = _free(hexstr);

    }

    /* Retrieve the matching public key. */
    res = rpmtsFindPubkey(ts);
    if (res != RPMRC_OK)
	goto exit;

    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_SIGNATURE), 0);
#if HAVE_BEECRYPT_API_H
    xx = rsavrfy(&dig->rsa_pk.n, &dig->rsa_pk.e, &dig->c, &dig->rsahm);
#else
    xx = rsavrfy(&dig->rsa_pk, &dig->rsahm, &dig->c);
#endif
    if (xx)
	res = RPMRC_OK;
    else
	res = RPMRC_FAIL;
    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_SIGNATURE), 0);

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
/*@=boundswrite@*/

/**
 * Verify DSA signature.
 * @param ts		transaction set
 * @retval t		verbose success/failure text
 * @param sha1ctx
 * @return 		RPMRC_OK on success
 */
/*@-boundswrite@*/
static rpmRC
verifyDSASignature(rpmts ts, /*@out@*/ char * t,
		/*@null@*/ DIGEST_CTX sha1ctx)
	/*@globals rpmGlobalMacroContext, h_errno, fileSystem, internalState @*/
	/*@modifies ts, *t, rpmGlobalMacroContext, fileSystem, internalState */
{
    const void * sig = rpmtsSig(ts);
#ifdef	NOTYET
    int_32 siglen = rpmtsSiglen(ts);
#endif
    int_32 sigtag = rpmtsSigtag(ts);
    pgpDig dig = rpmtsDig(ts);
    pgpDigParams sigp = rpmtsSignature(ts);
    rpmRC res;
    int xx;

    *t = '\0';
    if (dig != NULL && dig->hdrsha1ctx == sha1ctx)
	t = stpcpy(t, _("Header "));
    *t++ = 'V';
    switch (sigp->version) {
    case 3:    *t++ = '3';     break;
    case 4:    *t++ = '4';     break;
    }
    t = stpcpy(t, _(" DSA signature: "));

    if (sha1ctx == NULL || sig == NULL || dig == NULL || sigp == NULL) {
	res = RPMRC_NOKEY;
	goto exit;
    }

    /* XXX sanity check on sigtag and signature agreement. */
    if (!((sigtag == RPMSIGTAG_GPG || sigtag == RPMSIGTAG_DSA)
    	&& sigp->pubkey_algo == PGPPUBKEYALGO_DSA
    	&& sigp->hash_algo == PGPHASHALGO_SHA1))
    {
	res = RPMRC_NOKEY;
	goto exit;
    }

    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_DIGEST), 0);
    {	DIGEST_CTX ctx = rpmDigestDup(sha1ctx);
	byte signhash16[2];

	if (sigp->hash != NULL)
	    xx = rpmDigestUpdate(ctx, sigp->hash, sigp->hashlen);

	if (sigp->version == 4) {
	    int nb = sigp->hashlen;
	    byte trailer[6];
	    nb = htonl(nb);
	    trailer[0] = sigp->version;
	    trailer[1] = 0xff;
	    memcpy(trailer+2, &nb, sizeof(nb));
	    xx = rpmDigestUpdate(ctx, trailer, sizeof(trailer));
	}
	xx = rpmDigestFinal(ctx, (void **)&dig->sha1, &dig->sha1len, 1);
	(void) rpmswExit(rpmtsOp(ts, RPMTS_OP_DIGEST), sigp->hashlen);
	rpmtsOp(ts, RPMTS_OP_DIGEST)->count--;	/* XXX one too many */

	mpnzero(&dig->hm);	(void) mpnsethex(&dig->hm, dig->sha1);

	/* Compare leading 16 bits of digest for quick check. */
	signhash16[0] = (*dig->hm.data >> 24) & 0xff;
	signhash16[1] = (*dig->hm.data >> 16) & 0xff;
	if (memcmp(signhash16, sigp->signhash16, sizeof(signhash16))) {
	    res = RPMRC_FAIL;
	    goto exit;
	}
    }

    /* Retrieve the matching public key. */
    res = rpmtsFindPubkey(ts);
    if (res != RPMRC_OK)
	goto exit;

    (void) rpmswEnter(rpmtsOp(ts, RPMTS_OP_SIGNATURE), 0);
    if (dsavrfy(&dig->p, &dig->q, &dig->g,
		&dig->hm, &dig->y, &dig->r, &dig->s))
	res = RPMRC_OK;
    else
	res = RPMRC_FAIL;
    (void) rpmswExit(rpmtsOp(ts, RPMTS_OP_SIGNATURE), 0);

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
/*@=boundswrite@*/

rpmRC
rpmVerifySignature(const rpmts ts, char * result)
{
    const void * sig = rpmtsSig(ts);
    int_32 siglen = rpmtsSiglen(ts);
    int_32 sigtag = rpmtsSigtag(ts);
    pgpDig dig = rpmtsDig(ts);
    rpmRC res;

    if (sig == NULL || siglen <= 0 || dig == NULL) {
	sprintf(result, _("Verify signature: BAD PARAMETERS\n"));
	return RPMRC_NOTFOUND;
    }

    switch (sigtag) {
    case RPMSIGTAG_SIZE:
	res = verifySizeSignature(ts, result);
	break;
    case RPMSIGTAG_MD5:
	res = verifyMD5Signature(ts, result, dig->md5ctx);
	break;
    case RPMSIGTAG_SHA1:
	res = verifySHA1Signature(ts, result, dig->hdrsha1ctx);
	break;
    case RPMSIGTAG_RSA:
	res = verifyRSASignature(ts, result, dig->hdrmd5ctx);
	break;
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
	res = verifyRSASignature(ts, result,
		((dig->signature.hash_algo == PGPHASHALGO_MD5)
			? dig->md5ctx : dig->sha1ctx));
	break;
    case RPMSIGTAG_DSA:
	res = verifyDSASignature(ts, result, dig->hdrsha1ctx);
	break;
    case RPMSIGTAG_GPG:
	res = verifyDSASignature(ts, result, dig->sha1ctx);
	break;
    case RPMSIGTAG_LEMD5_1:
    case RPMSIGTAG_LEMD5_2:
	sprintf(result, _("Broken MD5 digest: UNSUPPORTED\n"));
	res = RPMRC_NOTFOUND;
	break;
    default:
	sprintf(result, _("Signature: UNKNOWN (%d)\n"), sigtag);
	res = RPMRC_NOTFOUND;
	break;
    }
    return res;
}
