/** \ingroup signature
 * \file lib/signature.c
 */

#include "system.h"

#include <inttypes.h>
#include <netinet/in.h>

#include <rpm/rpmtypes.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmfileutil.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmkeyring.h>

#include "lib/rpmlead.h"
#include "lib/signature.h"
#include "lib/header_internal.h"

#include "debug.h"

/* Dumb wrapper around headerPut() for signature header */
static int sighdrPut(Header h, rpmTagVal tag, rpmTagType type,
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
    unsigned int nb, uc;
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
    if ((xx = Freadall(fd, block, sizeof(block))) != sizeof(block)) {
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
    uc = sizeof(il) + sizeof(dl) + nb;
    ei = xmalloc(uc);
    ei[0] = block[2];
    ei[1] = block[3];
    pe = (entryInfo) &ei[2];
    dataStart = (unsigned char *) (pe + il);
    if ((xx = Freadall(fd, pe, nb)) != nb) {
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
    if (entry.info.tag == RPMTAG_HEADERSIGNATURES) {
	/* Is the region tag sane? */
	if (!(entry.info.type == REGION_TAG_TYPE &&
	      entry.info.count == REGION_TAG_COUNT)) {
	    rasprintf(&buf,
		_("region tag: BAD, tag %d type %d offset %d count %d\n"),
		entry.info.tag, entry.info.type,
		entry.info.offset, entry.info.count);
	    goto exit;
	}
	
	/* Is the trailer within the data area? */
	if (entry.info.offset + REGION_TAG_COUNT > dl) {
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
	    rpmTagVal stag = htonl(RPMTAG_HEADERSIGNATURES);
	    info.tag = stag;
	    memcpy(dataEnd, &stag, sizeof(stag));
	}
	dataEnd += REGION_TAG_COUNT;

	xx = headerVerifyInfo(1, il * sizeof(*pe), &info, &entry.info, 1);
	if (xx != -1 ||
	    !((entry.info.tag == RPMTAG_HEADERSIGNATURES || entry.info.tag == RPMTAG_HEADERIMAGE)
	   && entry.info.type == REGION_TAG_TYPE
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
    sigh = headerImport(ei, uc, 0);
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
	if (pad && (trc = Freadall(fd, block, pad)) != pad) {
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
    headerFree(sigh);
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

static int makeHDRDigest(Header sigh, const char * file, rpmTagVal sigTag)
{
    Header h = NULL;
    FD_t fd = NULL;
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

	if (headerIsEntry(h, RPMTAG_HEADERIMMUTABLE)) {
	    DIGEST_CTX ctx;
	    struct rpmtd_s utd;
	
	    if (!headerGet(h, RPMTAG_HEADERIMMUTABLE, &utd, HEADERGET_DEFAULT)
	     	||  utd.data == NULL)
	    {
		rpmlog(RPMLOG_ERR, 
				_("Immutable header region could not be read. "
				"Corrupted package?\n"));
		goto exit;
	    }
	    ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
	    (void) rpmDigestUpdate(ctx, rpm_header_magic, sizeof(rpm_header_magic));
	    (void) rpmDigestUpdate(ctx, utd.data, utd.count);
	    (void) rpmDigestFinal(ctx, (void **)&SHA1, NULL, 1);
	    rpmtdFreeData(&utd);
	} else {
	    rpmlog(RPMLOG_ERR, _("Cannot sign RPM v3 packages\n"));
	    goto exit;
	}

	if (SHA1 == NULL)
	    goto exit;
	if (!sighdrPut(sigh, RPMSIGTAG_SHA1, RPM_STRING_TYPE, SHA1, 1))
	    goto exit;
	ret = 0;
	break;
    default:
	break;
    }

exit:
    free(SHA1);
    headerFree(h);
    if (fd != NULL) (void) Fclose(fd);
    return ret;
}

int rpmGenDigest(Header sigh, const char * file, rpmTagVal sigTag)
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
    case RPMSIGTAG_SHA1:
	ret = makeHDRDigest(sigh, file, sigTag);
	break;
    default:
	break;
    }
    free(pkt);

    return ret;
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

    if (ctx == NULL) {
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

    if (ctx == NULL) {
	rasprintf(msg, "%s %s\n", title, rpmSigString(res));
	goto exit;
    }

    (void) rpmDigestFinal(ctx, (void **)&SHA1, NULL, 1);

    if (SHA1 == NULL || !rstreq(SHA1, sig)) {
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
 * @param sig		OpenPGP signature parameters
 * @param hashctx	digest context
 * @param isHdr		header-only signature?
 * @retval msg		verbose success/failure text
 * @return 		RPMRC_OK on success
 */
static rpmRC
verifySignature(rpmKeyring keyring, pgpDigParams sig, DIGEST_CTX hashctx,
		int isHdr, char **msg)
{

    rpmRC res = rpmKeyringVerifySig(keyring, sig, hashctx);

    char *sigid = pgpIdentItem(sig);
    rasprintf(msg, "%s%s: %s\n", isHdr ? _("Header ") : "", sigid, 
		rpmSigString(res));
    free(sigid);
    return res;
}

rpmRC
rpmVerifySignature(rpmKeyring keyring, rpmtd sigtd, pgpDigParams sig,
		   DIGEST_CTX ctx, char ** result)
{
    rpmRC res = RPMRC_NOTFOUND;
    char *msg = NULL;
    int hdrsig = 0;

    if (sigtd->data == NULL || sigtd->count <= 0 || ctx == NULL)
	goto exit;

    switch (sigtd->tag) {
    case RPMSIGTAG_MD5:
	res = verifyMD5Digest(sigtd, ctx, &msg);
	break;
    case RPMSIGTAG_SHA1:
	res = verifySHA1Digest(sigtd, ctx, &msg);
	break;
    case RPMSIGTAG_RSA:
    case RPMSIGTAG_DSA:
	hdrsig = 1;
	/* fallthrough */
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
    case RPMSIGTAG_GPG:
	if (sig != NULL)
	    res = verifySignature(keyring, sig, ctx, hdrsig, &msg);
	break;
    default:
	break;
    }

exit:
    if (res == RPMRC_NOTFOUND) {
	rasprintf(&msg,
		  _("Verify signature: BAD PARAMETERS (%d %p %d %p %p)\n"),
		  sigtd->tag, sigtd->data, sigtd->count, ctx, sig);
	res = RPMRC_FAIL;
    }

    if (result) {
	*result = msg;
    } else {
	free(msg);
    }
    return res;
}
