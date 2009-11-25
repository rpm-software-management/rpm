/** \ingroup signature
 * \file lib/signature.c
 */

#include "system.h"

#include <inttypes.h>
#include <popt.h>

#include <rpm/rpmtypes.h>
#include <rpm/rpmmacro.h>	/* XXX for rpmExpand() */
#include <rpm/rpmstring.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmkeyring.h>

#include "rpmio/digest.h"
#include "lib/rpmlead.h"
#include "lib/signature.h"
#include "lib/header_internal.h"

#include "debug.h"

#if !defined(__GLIBC__) && !defined(__APPLE__)
char ** environ = NULL;
#endif

static int sighdrPut(Header h, rpmSigTag tag, rpmTagType type,
                     rpm_data_t p, rpm_count_t c)
{
    struct rpmtd_s sigtd;
    rpmtdReset(&sigtd);
    sigtd.tag = tag;
    sigtd.type = type;
    sigtd.data = p;
    sigtd.count = c;
    return headerPut(h, &sigtd, HEADERPUT_DEFAULT);
}

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
    case RPMLOOKUPSIG_QUERY:
	if (disabled)
	    break;	/* Disabled */
      { char *name = rpmExpand("%{?_signature}", NULL);
	if (!(name && *name != '\0'))
	    rc = 0;
	else if (!rstrcasecmp(name, "none"))
	    rc = 0;
	else if (!rstrcasecmp(name, "pgp"))
	    rc = RPMSIGTAG_PGP;
	else if (!rstrcasecmp(name, "pgp5"))	/* XXX legacy */
	    rc = RPMSIGTAG_PGP;
	else if (!rstrcasecmp(name, "gpg"))
	    rc = RPMSIGTAG_GPG;
	else
	    rc = -1;	/* Invalid %_signature spec in macro file */
	name = _free(name);
      }	break;
    }
    return rc;
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
static inline rpmRC printSize(FD_t fd, size_t siglen, size_t pad, rpm_loff_t datalen)
{
    struct stat st;
    int fdno = Fileno(fd);

    if (fstat(fdno, &st) < 0)
	return RPMRC_FAIL;

    rpmlog(RPMLOG_DEBUG,
		"Expected size: %12" PRIu64 \
		" = lead(%d)+sigs(%zd)+pad(%zd)+data(%" PRIu64 ")\n",
		RPMLEAD_SIZE+siglen+pad+datalen,
		RPMLEAD_SIZE, siglen, pad, datalen);
    rpmlog(RPMLOG_DEBUG,
		"  Actual size: %12" PRIu64 "\n", (rpm_loff_t) st.st_size);

    return RPMRC_OK;
}

rpmRC rpmReadSignature(FD_t fd, Header * sighp, sigType sig_type, char ** msg)
{
    char *buf = NULL;
    int32_t block[4];
    int32_t il;
    int32_t dl;
    int32_t * ei = NULL;
    entryInfo pe;
    size_t nb;
    int32_t ril = 0;
    struct indexEntry_s entry;
    struct entryInfo_s info;
    unsigned char * dataStart;
    unsigned char * dataEnd = NULL;
    Header sigh = NULL;
    rpmRC rc = RPMRC_FAIL;		/* assume failure */
    int xx;
    int i;

    if (sighp)
	*sighp = NULL;

    if (sig_type != RPMSIGTYPE_HEADERSIG)
	goto exit;

    memset(block, 0, sizeof(block));
    if ((xx = timedRead(fd, (void *)block, sizeof(block))) != sizeof(block)) {
	rasprintf(&buf, _("sigh size(%d): BAD, read returned %d\n"), 
		  (int)sizeof(block), xx);
	goto exit;
    }
    if (memcmp(block, rpm_header_magic, sizeof(rpm_header_magic))) {
	rasprintf(&buf, _("sigh magic: BAD\n"));
	goto exit;
    }
    il = ntohl(block[2]);
    if (il < 0 || il > 32) {
	rasprintf(&buf, 
		  _("sigh tags: BAD, no. of tags(%d) out of range\n"), il);
	goto exit;
    }
    dl = ntohl(block[3]);
    if (dl < 0 || dl > 8192) {
	rasprintf(&buf, 
		  _("sigh data: BAD, no. of  bytes(%d) out of range\n"), dl);
	goto exit;
    }

    memset(&entry, 0, sizeof(entry));
    memset(&info, 0, sizeof(info));

    nb = (il * sizeof(struct entryInfo_s)) + dl;
    ei = xmalloc(sizeof(il) + sizeof(dl) + nb);
    ei[0] = block[2];
    ei[1] = block[3];
    pe = (entryInfo) &ei[2];
    dataStart = (unsigned char *) (pe + il);
    if ((xx = timedRead(fd, (void *)pe, nb)) != nb) {
	rasprintf(&buf,
		  _("sigh blob(%d): BAD, read returned %d\n"), (int)nb, xx);
	goto exit;
    }
    
    /* Check (and convert) the 1st tag element. */
    xx = headerVerifyInfo(1, dl, pe, &entry.info, 0);
    if (xx != -1) {
	rasprintf(&buf, _("tag[%d]: BAD, tag %d type %d offset %d count %d\n"),
		  0, entry.info.tag, entry.info.type,
		  entry.info.offset, entry.info.count);
	goto exit;
    }

    /* Is there an immutable header region tag? */
    if (entry.info.tag == RPMTAG_HEADERSIGNATURES
       && entry.info.type == RPM_BIN_TYPE
       && entry.info.count == REGION_TAG_COUNT)
    {

	if (entry.info.offset >= dl) {
	    rasprintf(&buf, 
		_("region offset: BAD, tag %d type %d offset %d count %d\n"),
		entry.info.tag, entry.info.type,
		entry.info.offset, entry.info.count);
	    goto exit;
	}

	/* Is there an immutable header region tag trailer? */
	dataEnd = dataStart + entry.info.offset;
	(void) memcpy(&info, dataEnd, REGION_TAG_COUNT);
	/* XXX Really old packages have HEADER_IMAGE, not HEADER_SIGNATURES. */
	if (info.tag == htonl(RPMTAG_HEADERIMAGE)) {
	    rpmSigTag stag = htonl(RPMTAG_HEADERSIGNATURES);
	    info.tag = stag;
	    memcpy(dataEnd, &stag, sizeof(stag));
	}
	dataEnd += REGION_TAG_COUNT;

	xx = headerVerifyInfo(1, dl, &info, &entry.info, 1);
	if (xx != -1 ||
	    !((entry.info.tag == RPMTAG_HEADERSIGNATURES || entry.info.tag == RPMTAG_HEADERIMAGE)
	   && entry.info.type == RPM_BIN_TYPE
	   && entry.info.count == REGION_TAG_COUNT))
	{
	    rasprintf(&buf,
		_("region trailer: BAD, tag %d type %d offset %d count %d\n"),
		entry.info.tag, entry.info.type,
		entry.info.offset, entry.info.count);
	    goto exit;
	}
	memset(&info, 0, sizeof(info));

	/* Is the no. of tags in the region less than the total no. of tags? */
	ril = entry.info.offset/sizeof(*pe);
	if ((entry.info.offset % sizeof(*pe)) || ril > il) {
	    rasprintf(&buf, _("region size: BAD, ril(%d) > il(%d)\n"), ril, il);
	    goto exit;
	}
    }

    /* Sanity check signature tags */
    memset(&info, 0, sizeof(info));
    for (i = 1; i < il; i++) {
	xx = headerVerifyInfo(1, dl, pe+i, &entry.info, 0);
	if (xx != -1) {
	    rasprintf(&buf, 
		_("sigh tag[%d]: BAD, tag %d type %d offset %d count %d\n"),
		i, entry.info.tag, entry.info.type,
		entry.info.offset, entry.info.count);
	    goto exit;
	}
    }

    /* OK, blob looks sane, load the header. */
    sigh = headerLoad(ei);
    if (sigh == NULL) {
	rasprintf(&buf, _("sigh load: BAD\n"));
	goto exit;
    }

    {	size_t sigSize = headerSizeof(sigh, HEADER_MAGIC_YES);
	size_t pad = (8 - (sigSize % 8)) % 8; /* 8-byte pad */
	ssize_t trc;
	struct rpmtd_s sizetag;
	rpm_loff_t archSize = 0;

	/* Position at beginning of header. */
	if (pad && (trc = timedRead(fd, (void *)block, pad)) != pad) {
	    rasprintf(&buf,
		      _("sigh pad(%zd): BAD, read %zd bytes\n"), pad, trc);
	    goto exit;
	}

	/* Print package component sizes. */
	if (headerGet(sigh, RPMSIGTAG_LONGSIZE, &sizetag, HEADERGET_DEFAULT)) {
	    rpm_loff_t *tsize = rpmtdGetUint64(&sizetag);
	    archSize = (tsize) ? *tsize : 0;
	} else if (headerGet(sigh, RPMSIGTAG_SIZE, &sizetag, HEADERGET_DEFAULT)) {
	    rpm_off_t *tsize = rpmtdGetUint32(&sizetag);
	    archSize = (tsize) ? *tsize : 0;
	}
	rpmtdFreeData(&sizetag);
	rc = printSize(fd, sigSize, pad, archSize);
	if (rc != RPMRC_OK) {
	    rasprintf(&buf,
		   _("sigh sigSize(%zd): BAD, fstat(2) failed\n"), sigSize);
	    goto exit;
	}
    }
    ei = NULL; /* XXX will be freed with header */

exit:
    if (sighp && sigh && rc == RPMRC_OK)
	*sighp = headerLink(sigh);
    sigh = headerFree(sigh);
    free(ei);

    if (msg != NULL) {
	*msg = buf;
    } else {
	free(buf);
    }

    return rc;
}

int rpmWriteSignature(FD_t fd, Header sigh)
{
    static uint8_t buf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int sigSize, pad;
    int rc;

    rc = headerWrite(fd, sigh, HEADER_MAGIC_YES);
    if (rc)
	return rc;

    sigSize = headerSizeof(sigh, HEADER_MAGIC_YES);
    pad = (8 - (sigSize % 8)) % 8;
    if (pad) {
	if (Fwrite(buf, sizeof(buf[0]), pad, fd) != pad)
	    rc = 1;
    }
    rpmlog(RPMLOG_DEBUG, "Signature: size(%d)+pad(%d)\n", sigSize, pad);
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
    char * sigfile = NULL;
    int pid, status;
    int inpipe[2];
    FILE * fpipe;
    struct stat st;
    const char * cmd;
    char *const *av;
    pgpDig dig = NULL;
    pgpDigParams sigp = NULL;
    int rc = 1; /* assume failure */

    rasprintf(&sigfile, "%s.sig", file);

    addMacro(NULL, "__plaintext_filename", NULL, file, -1);
    addMacro(NULL, "__signature_filename", NULL, sigfile, -1);

    inpipe[0] = inpipe[1] = 0;
    if (pipe(inpipe) < 0) {
	rpmlog(RPMLOG_ERR, _("Couldn't create pipe for signing: %m"));
	goto exit;
    }

    if (!(pid = fork())) {
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
	goto exit;
    }

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

    dig = pgpFreeDig(dig);
    rc = 0;

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
    char * SHA1 = NULL;
    int ret = -1;	/* assume failure. */

    switch (sigTag) {
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
	    struct rpmtd_s utd;
	
	    if (!headerGet(h, RPMTAG_HEADERIMMUTABLE, &utd, HEADERGET_DEFAULT)
	     	||  utd.data == NULL)
	    {
		rpmlog(RPMLOG_ERR, 
				_("Immutable header region could not be read. "
				"Corrupted package?\n"));
		h = headerFree(h);
		goto exit;
	    }
	    ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
	    (void) rpmDigestUpdate(ctx, rpm_header_magic, sizeof(rpm_header_magic));
	    (void) rpmDigestUpdate(ctx, utd.data, utd.count);
	    (void) rpmDigestFinal(ctx, (void **)&SHA1, NULL, 1);
	    rpmtdFreeData(&utd);
	}
	h = headerFree(h);

	if (SHA1 == NULL)
	    goto exit;
	if (!sighdrPut(sigh, RPMSIGTAG_SHA1, RPM_STRING_TYPE, SHA1, 1))
	    goto exit;
	ret = 0;
	break;
    case RPMSIGTAG_DSA:
    case RPMSIGTAG_RSA:
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
	(void) Fclose(fd);	fd = NULL;
	if (makeGPGSignature(fn, &sigTag, &pkt, &pktlen, passPhrase)
	 || !sighdrPut(sigh, sigTag, RPM_BIN_TYPE, pkt, pktlen))
	    goto exit;
	ret = 0;
	break;
    default:
	goto exit;
	break;
    }

exit:
    if (fn) {
	(void) unlink(fn);
	free(fn);
    }
    free(pkt);
    free(SHA1);
    h = headerFree(h);
    if (fd != NULL) (void) Fclose(fd);
    return ret;
}

int rpmAddSignature(Header sigh, const char * file, rpmSigTag sigTag,
		const char * passPhrase)
{
    struct stat st;
    uint8_t * pkt = NULL;
    size_t pktlen;
    int ret = -1;	/* assume failure. */

    switch (sigTag) {
    case RPMSIGTAG_SIZE: {
	rpm_off_t size;
	if (stat(file, &st) != 0)
	    break;
	size = st.st_size;
	if (!sighdrPut(sigh, sigTag, RPM_INT32_TYPE, &size, 1))
	    break;
	ret = 0;
	} break;
    case RPMSIGTAG_LONGSIZE: {
	rpm_loff_t size;
	if (stat(file, &st) != 0)
	    break;
	size = st.st_size;
	if (!sighdrPut(sigh, sigTag, RPM_INT64_TYPE, &size, 1))
	    break;
	ret = 0;
	} break;
    case RPMSIGTAG_MD5:
	pktlen = 16;
	pkt = xcalloc(pktlen, sizeof(*pkt));
	if (rpmDoDigest(PGPHASHALGO_MD5, file, 0, pkt, NULL)
	 || !sighdrPut(sigh, sigTag, RPM_BIN_TYPE, pkt, pktlen))
	    break;
	ret = 0;
	break;
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
    case RPMSIGTAG_GPG: {
	rpmSigTag hdrtag = (sigTag == RPMSIGTAG_GPG) ?
			    RPMSIGTAG_DSA : RPMSIGTAG_RSA;
	if (makeGPGSignature(file, &sigTag, &pkt, &pktlen, passPhrase)
	 || !sighdrPut(sigh, sigTag, RPM_BIN_TYPE, pkt, pktlen))
	    break;
	/* XXX Piggyback a header-only DSA/RSA signature as well. */
	ret = makeHDRSignature(sigh, file, hdrtag, passPhrase);
	} break;
    case RPMSIGTAG_RSA:
    case RPMSIGTAG_DSA:
    case RPMSIGTAG_SHA1:
	ret = makeHDRSignature(sigh, file, sigTag, passPhrase);
	break;
    default:
	break;
    }
    free(pkt);

    return ret;
}

static int checkPassPhrase(const char * passPhrase, const rpmSigTag sigTag)
{
    int passPhrasePipe[2];
    int pid, status;
    int rc;
    int xx;

    passPhrasePipe[0] = passPhrasePipe[1] = 0;
    xx = pipe(passPhrasePipe);
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
	case RPMSIGTAG_RSA:
	case RPMSIGTAG_PGP5:	/* XXX legacy */
	case RPMSIGTAG_PGP:
	case RPMSIGTAG_DSA:
	case RPMSIGTAG_GPG:
	{   const char *gpg_path = rpmExpand("%{?_gpg_path}", NULL);

	    if (gpg_path && *gpg_path != '\0')
  		(void) setenv("GNUPGHOME", gpg_path, 1);

	    cmd = rpmExpand("%{?__gpg_check_password_cmd}", NULL);
	    rc = poptParseArgvString(cmd, NULL, (const char ***)&av);
	    if (!rc)
		rc = execve(av[0], av+1, environ);

	    rpmlog(RPMLOG_ERR, _("Could not exec %s: %s\n"), "gpg",
			strerror(errno));
	    _exit(EXIT_FAILURE);
	}   break;
	default: /* This case should have been screened out long ago. */
	    rpmlog(RPMLOG_ERR, _("Invalid %%_signature spec in macro file\n"));
	    _exit(EXIT_FAILURE);
	    break;
	}
    }

    xx = close(passPhrasePipe[0]);
    xx = write(passPhrasePipe[1], passPhrase, strlen(passPhrase));
    xx = write(passPhrasePipe[1], "\n", 1);
    xx = close(passPhrasePipe[1]);

    (void) waitpid(pid, &status, 0);

    return ((WIFEXITED(status) && WEXITSTATUS(status) == 0)) ? 0 : 1;
}

char * rpmGetPassPhrase(const char * prompt, const rpmSigTag sigTag)
{
    char *pass = NULL;
    int aok = 0;

    switch (sigTag) {
    case RPMSIGTAG_RSA:
    case RPMSIGTAG_PGP5: 	/* XXX legacy */
    case RPMSIGTAG_PGP:
    case RPMSIGTAG_DSA:
    case RPMSIGTAG_GPG:
      { char *name = rpmExpand("%{?_gpg_name}", NULL);
	aok = (name && *name != '\0');
	name = _free(name);
      }
	if (aok)
	    break;
	rpmlog(RPMLOG_ERR,
		_("You must set \"%%_gpg_name\" in your macro file\n"));
	break;
    default:
	/* Currently the calling function (rpm.c:main) is checking this and
	 * doing a better job.  This section should never be accessed.
	 */
	rpmlog(RPMLOG_ERR, _("Invalid %%_signature spec in macro file\n"));
	break;
    }

    if (aok) {
	pass = getpass( (prompt ? prompt : "") );

	if (checkPassPhrase(pass, sigTag))
	    pass = NULL;
    }

    return pass;
}

static const char * rpmSigString(rpmRC res)
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

static rpmRC
verifyMD5Digest(rpmtd sigtd, DIGEST_CTX md5ctx, char **msg)
{
    rpmRC res = RPMRC_FAIL; /* assume failure */
    uint8_t * md5sum = NULL;
    size_t md5len = 0;
    char *md5;
    const char *title = _("MD5 digest:");
    *msg = NULL;
    DIGEST_CTX ctx = rpmDigestDup(md5ctx);

    if (ctx == NULL || sigtd->data == NULL) {
	rasprintf(msg, "%s %s\n", title, rpmSigString(res));
	goto exit;
    }

    (void) rpmDigestFinal(ctx, (void **)&md5sum, &md5len, 0);

    md5 = pgpHexStr(md5sum, md5len);
    if (md5len != sigtd->count || memcmp(md5sum, sigtd->data, md5len)) {
	char *hex = rpmtdFormat(sigtd, RPMTD_FORMAT_STRING, NULL);
	rasprintf(msg, "%s %s Expected(%s) != (%s)\n", title,
		  rpmSigString(res), hex, md5);
	free(hex);
    } else {
	res = RPMRC_OK;
	rasprintf(msg, "%s %s (%s)\n", title, rpmSigString(res), md5);
    }
    free(md5);

exit:
    md5sum = _free(md5sum);
    return res;
}

/**
 * Verify header immutable region SHA1 digest.
 * @retval msg		verbose success/failure text
 * @param sha1ctx
 * @return 		RPMRC_OK on success
 */
static rpmRC
verifySHA1Digest(rpmtd sigtd, DIGEST_CTX sha1ctx, char **msg)
{
    rpmRC res = RPMRC_FAIL; /* assume failure */
    char * SHA1 = NULL;
    const char *title = _("Header SHA1 digest:");
    const char *sig = sigtd->data;
    *msg = NULL;
    DIGEST_CTX ctx = rpmDigestDup(sha1ctx);

    if (ctx == NULL || sigtd->data == NULL) {
	rasprintf(msg, "%s %s\n", title, rpmSigString(res));
	goto exit;
    }

    (void) rpmDigestFinal(ctx, (void **)&SHA1, NULL, 1);

    if (SHA1 == NULL || strlen(SHA1) != strlen(sig) || !rstreq(SHA1, sig)) {
	rasprintf(msg, "%s %s Expected(%s) != (%s)\n", title,
		  rpmSigString(res), sig, SHA1 ? SHA1 : "(nil)");
    } else {
	res = RPMRC_OK;
	rasprintf(msg, "%s %s (%s)\n", title, rpmSigString(res), SHA1);
    }

exit:
    SHA1 = _free(SHA1);
    return res;
}

/**
 * Verify DSA/RSA signature.
 * @param keyring	pubkey keyring
 * @param dig		OpenPGP container
 * @param hashctx	digest context
 * @param isHdr		header-only signature?
 * @retval msg		verbose success/failure text
 * @return 		RPMRC_OK on success
 */
static rpmRC
verifySignature(rpmKeyring keyring, pgpDig dig, DIGEST_CTX hashctx, int isHdr, 
		char **msg)
{
    pgpDigParams sigp = dig ? &dig->signature : NULL;
    rpmRC res = RPMRC_FAIL; /* assume failure */
    char *sigid = NULL;
    *msg = NULL;

    if (hashctx == NULL) {
	goto exit;
    }

    /* Retrieve the matching public key and verify. */
    res = rpmKeyringLookup(keyring, dig);
    if (res == RPMRC_OK) {
	res = pgpVerifySig(dig, hashctx);
    }

exit:
    sigid = pgpIdentItem(sigp);
    rasprintf(msg, "%s%s: %s\n", isHdr ? _("Header ") : "", sigid, 
		rpmSigString(res));
    free(sigid);
    return res;
}

rpmRC
rpmVerifySignature(rpmKeyring keyring, rpmtd sigtd, pgpDig dig, DIGEST_CTX ctx, char ** result)
{
    rpmRC res = RPMRC_NOTFOUND;
    char *msg = NULL;

    if (sigtd->data == NULL || sigtd->count <= 0 || dig == NULL) {
	rasprintf(&msg, _("Verify signature: BAD PARAMETERS\n"));
	goto exit;
    }

    switch (sigtd->tag) {
    case RPMSIGTAG_MD5:
	res = verifyMD5Digest(sigtd, ctx, &msg);
	break;
    case RPMSIGTAG_SHA1:
	res = verifySHA1Digest(sigtd, ctx, &msg);
	break;
    case RPMSIGTAG_RSA:
    case RPMSIGTAG_DSA:
	res = verifySignature(keyring, dig, ctx, 1, &msg);
	break;
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
    case RPMSIGTAG_GPG:
	res = verifySignature(keyring, dig, ctx, 0, &msg);
	break;
    default:
	rasprintf(&msg, _("Signature: UNKNOWN (%d)\n"), sigtd->tag);
	break;
    }

exit:
    if (result) {
	*result = msg;
    } else {
	free(msg);
    }
    return res;
}
