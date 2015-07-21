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
#include <rpm/rpmmacro.h>

#include "lib/rpmlead.h"
#include "lib/signature.h"
#include "lib/header_internal.h"

#include "debug.h"

rpmRC rpmSigInfoParse(rpmtd td, const char *origin,
		      struct sigtInfo_s *sinfo, pgpDigParams *sigp, char **msg)
{
    rpmRC rc = RPMRC_FAIL;
    rpm_tagtype_t tagtype = 0;
    rpm_count_t tagsize = 0;
    pgpDigParams sig = NULL;

    memset(sinfo, 0, sizeof(*sinfo));
    switch (td->tag) {
    case RPMSIGTAG_GPG:
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
	sinfo->payload = 1;
	/* fallthrough */
    case RPMSIGTAG_RSA:
    case RPMSIGTAG_DSA:
	tagtype = RPM_BIN_TYPE;
	sinfo->type = RPMSIG_SIGNATURE_TYPE;
	break;
    case RPMSIGTAG_SHA1:
	tagsize = 41; /* includes trailing \0 */
	tagtype = RPM_STRING_TYPE;
	sinfo->hashalgo = PGPHASHALGO_SHA1;
	sinfo->type = RPMSIG_DIGEST_TYPE;
	break;
    case RPMSIGTAG_MD5:
	tagtype = RPM_BIN_TYPE;
	tagsize = 16;
	sinfo->hashalgo = PGPHASHALGO_MD5;
	sinfo->type = RPMSIG_DIGEST_TYPE;
	sinfo->payload = 1;
	break;
    case RPMSIGTAG_SIZE:
    case RPMSIGTAG_PAYLOADSIZE:
	tagsize = 4;
	tagtype = RPM_INT32_TYPE;
	sinfo->type = RPMSIG_OTHER_TYPE;
	break;
    case RPMSIGTAG_LONGSIZE:
    case RPMSIGTAG_LONGARCHIVESIZE:
	tagsize = 8;
	tagtype = RPM_INT64_TYPE;
	sinfo->type = RPMSIG_OTHER_TYPE;
	break;
    case RPMSIGTAG_RESERVEDSPACE:
	tagtype = RPM_BIN_TYPE;
	sinfo->type = RPMSIG_OTHER_TYPE;
	break;
    default:
	/* anything unknown just falls through for now */
	break;
    }

    if (tagsize && (td->flags & RPMTD_IMMUTABLE) && tagsize != td->size) {
	rasprintf(msg, _("%s tag %u: BAD, invalid size %u"),
			origin, td->tag, td->size);
	goto exit;
    }

    if (tagtype && tagtype != td->type) {
	rasprintf(msg, _("%s tag %u: BAD, invalid type %u"),
			origin, td->tag, td->type);
	goto exit;
    }

    if (sinfo->type == RPMSIG_SIGNATURE_TYPE) {
	if (pgpPrtParams(td->data, td->count, PGPTAG_SIGNATURE, &sig)) {
	    rasprintf(msg, _("%s tag %u: BAD, invalid OpenPGP signature"),
		    origin, td->tag);
	    goto exit;
	}
	sinfo->hashalgo = pgpDigParamsAlgo(sig, PGPVAL_HASHALGO);
    }

    rc = RPMRC_OK;
    if (sigp)
	*sigp = sig;
    else
	pgpDigParamsFree(sig);

exit:
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
    unsigned int nb, uc;
    struct indexEntry_s entry;
    unsigned char * dataStart;
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
	rasprintf(&buf, _("sigh size(%d): BAD, read returned %d"), 
		  (int)sizeof(block), xx);
	goto exit;
    }
    if (memcmp(block, rpm_header_magic, sizeof(rpm_header_magic))) {
	rasprintf(&buf, _("sigh magic: BAD"));
	goto exit;
    }
    il = ntohl(block[2]);
    if (il < 0 || il > 32) {
	rasprintf(&buf, 
		  _("sigh tags: BAD, no. of tags(%d) out of range"), il);
	goto exit;
    }
    dl = ntohl(block[3]);
    if (dl < 0 || dl > 8192) {
	rasprintf(&buf, 
		  _("sigh data: BAD, no. of  bytes(%d) out of range"), dl);
	goto exit;
    }

    memset(&entry, 0, sizeof(entry));

    nb = (il * sizeof(struct entryInfo_s)) + dl;
    uc = sizeof(il) + sizeof(dl) + nb;
    ei = xmalloc(uc);
    ei[0] = block[2];
    ei[1] = block[3];
    pe = (entryInfo) &ei[2];
    dataStart = (unsigned char *) (pe + il);
    if ((xx = Freadall(fd, pe, nb)) != nb) {
	rasprintf(&buf,
		  _("sigh blob(%d): BAD, read returned %d"), (int)nb, xx);
	goto exit;
    }
    
    /* Verify header immutable region if there is one */
    xx = headerVerifyRegion(RPMTAG_HEADERSIGNATURES,
			    &entry, il, dl, pe, dataStart,
			    NULL, NULL, &buf);
    /* Not found means a legacy V3 package with no immutable region */
    if (xx != RPMRC_OK && xx != RPMRC_NOTFOUND)
	goto exit;

    /* Sanity check signature tags */
    for (i = 1; i < il; i++) {
	xx = headerVerifyInfo(1, dl, pe+i, &entry.info, 0);
	if (xx != -1) {
	    rasprintf(&buf, 
		_("sigh tag[%d]: BAD, tag %d type %d offset %d count %d"),
		i, entry.info.tag, entry.info.type,
		entry.info.offset, entry.info.count);
	    goto exit;
	}
    }

    /* OK, blob looks sane, load the header. */
    sigh = headerImport(ei, uc, 0);
    if (sigh == NULL) {
	rasprintf(&buf, _("sigh load: BAD"));
	goto exit;
    }
    ei = NULL; /* XXX will be freed with header */

    {	size_t sigSize = headerSizeof(sigh, HEADER_MAGIC_YES);
	size_t pad = (8 - (sigSize % 8)) % 8; /* 8-byte pad */
	ssize_t trc;
	struct rpmtd_s sizetag;
	rpm_loff_t archSize = 0;

	/* Position at beginning of header. */
	if (pad && (trc = Freadall(fd, block, pad)) != pad) {
	    rasprintf(&buf,
		      _("sigh pad(%zd): BAD, read %zd bytes"), pad, trc);
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
		   _("sigh sigSize(%zd): BAD, fstat(2) failed"), sigSize);
	    goto exit;
	}
    }

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
    static const uint8_t zeros[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int sigSize, pad;
    int rc;

    rc = headerWrite(fd, sigh, HEADER_MAGIC_YES);
    if (rc)
	return rc;

    sigSize = headerSizeof(sigh, HEADER_MAGIC_YES);
    pad = (8 - (sigSize % 8)) % 8;
    if (pad) {
	if (Fwrite(zeros, sizeof(zeros[0]), pad, fd) != pad)
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

rpmRC rpmGenerateSignature(char *SHA1, uint8_t *MD5, rpm_loff_t size,
				rpm_loff_t payloadSize, FD_t fd)
{
    Header sig = NULL;
    struct rpmtd_s td;
    rpmRC rc = RPMRC_OK;
    char *reservedSpace;
    int spaceSize = 0;

    /* Prepare signature */
    sig = rpmNewSignature();

    rpmtdReset(&td);
    td.tag = RPMSIGTAG_SHA1;
    td.count = 1;
    td.type = RPM_STRING_TYPE;
    td.data = SHA1;
    headerPut(sig, &td, HEADERPUT_DEFAULT);

    rpmtdReset(&td);
    td.tag = RPMSIGTAG_MD5;
    td.count = 16;
    td.type = RPM_BIN_TYPE;
    td.data = MD5;
    headerPut(sig, &td, HEADERPUT_DEFAULT);

    rpmtdReset(&td);
    td.count = 1;
    if (payloadSize < UINT32_MAX) {
	rpm_off_t p = payloadSize;
	rpm_off_t s = size;
	td.type = RPM_INT32_TYPE;

	td.tag = RPMSIGTAG_PAYLOADSIZE;
	td.data = &p;
	headerPut(sig, &td, HEADERPUT_DEFAULT);

	td.tag = RPMSIGTAG_SIZE;
	td.data = &s;
	headerPut(sig, &td, HEADERPUT_DEFAULT);
    } else {
	rpm_loff_t p = payloadSize;
	rpm_loff_t s = size;
	td.type = RPM_INT64_TYPE;

	td.tag = RPMSIGTAG_LONGARCHIVESIZE;
	td.data = &p;
	headerPut(sig, &td, HEADERPUT_DEFAULT);

	td.tag = RPMSIGTAG_LONGSIZE;
	td.data = &s;
	headerPut(sig, &td, HEADERPUT_DEFAULT);
    }

    spaceSize = rpmExpandNumeric("%{__gpg_reserved_space}");
    if(spaceSize > 0) {
	reservedSpace = xcalloc(spaceSize, sizeof(char));
	rpmtdReset(&td);
	td.tag = RPMSIGTAG_RESERVEDSPACE;
	td.count = spaceSize;
	td.type = RPM_BIN_TYPE;
	td.data = reservedSpace;
	headerPut(sig, &td, HEADERPUT_DEFAULT);
	free(reservedSpace);
    }

    /* Reallocate the signature into one contiguous region. */
    sig = headerReload(sig, RPMTAG_HEADERSIGNATURES);
    if (sig == NULL) { /* XXX can't happen */
	rpmlog(RPMLOG_ERR, _("Unable to reload signature header.\n"));
	rc = RPMRC_FAIL;
	goto exit;
    }

    /* Write the signature section into the package. */
    if (rpmWriteSignature(fd, sig)) {
	rc = RPMRC_FAIL;
	goto exit;
    }

exit:
    rpmFreeSignature(sig);
    return rc;
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

static rpmRC verifyDigest(rpmtd sigtd, DIGEST_CTX digctx, const char *title,
			  char **msg)
{
    rpmRC res = RPMRC_FAIL; /* assume failure */
    char * dig = NULL;
    size_t diglen = 0;
    char *pkgdig = rpmtdFormat(sigtd, RPMTD_FORMAT_STRING, NULL);
    DIGEST_CTX ctx = rpmDigestDup(digctx);

    if (rpmDigestFinal(ctx, (void **)&dig, &diglen, 1) || diglen == 0) {
	rasprintf(msg, "%s %s", title, rpmSigString(res));
	goto exit;
    }

    if (strcmp(pkgdig, dig) == 0) {
	res = RPMRC_OK;
	rasprintf(msg, "%s %s (%s)", title, rpmSigString(res), pkgdig);
    } else {
	rasprintf(msg, "%s: %s Expected(%s) != (%s)",
		  title, rpmSigString(res), pkgdig, dig);
    }

exit:
    free(dig);
    free(pkgdig);
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
    rasprintf(msg, "%s%s: %s", isHdr ? _("Header ") : "", sigid, 
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
	res = verifyDigest(sigtd, ctx, _("MD5 digest:"), &msg);
	break;
    case RPMSIGTAG_SHA1:
	res = verifyDigest(sigtd, ctx,  _("Header SHA1 digest:"), &msg);
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
		  _("Verify signature: BAD PARAMETERS (%d %p %d %p %p)"),
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
