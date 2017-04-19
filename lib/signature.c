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

#include "rpmio/digest.h"
#include "lib/rpmlead.h"
#include "lib/signature.h"
#include "lib/header_internal.h"

#include "debug.h"

rpmRC rpmsinfoInit(rpmtd td, const char *origin,
		      struct rpmsinfo_s *sinfo, char **msg)
{
    rpmRC rc = RPMRC_FAIL;
    rpm_tagtype_t tagtype = 0;
    rpm_count_t tagsize = 0;
    rpm_count_t tagcount = 0;
    int hexstring = 0;
    const void *data = NULL;
    rpm_count_t dlen = 0;

    memset(sinfo, 0, sizeof(*sinfo));
    switch (td->tag) {
    case RPMSIGTAG_GPG:
	tagtype = RPM_BIN_TYPE;
	sinfo->type = RPMSIG_SIGNATURE_TYPE;
	sinfo->range = (RPMSIG_HEADER|RPMSIG_PAYLOAD);
	break;
    case RPMSIGTAG_PGP5:	/* XXX legacy */
    case RPMSIGTAG_PGP:
	tagtype = RPM_BIN_TYPE;
	sinfo->type = RPMSIG_SIGNATURE_TYPE;
	sinfo->range = (RPMSIG_HEADER|RPMSIG_PAYLOAD);
	break;
    case RPMSIGTAG_RSA:
	tagtype = RPM_BIN_TYPE;
	sinfo->type = RPMSIG_SIGNATURE_TYPE;
	sinfo->range = RPMSIG_HEADER;
	break;
    case RPMSIGTAG_DSA:
	tagtype = RPM_BIN_TYPE;
	sinfo->type = RPMSIG_SIGNATURE_TYPE;
	sinfo->range = RPMSIG_HEADER;
	break;
    case RPMSIGTAG_SHA256:
	tagsize = 65; /* includes trailing \0 */
	tagtype = RPM_STRING_TYPE;
	tagcount = 1;
	hexstring = 1;
	sinfo->hashalgo = PGPHASHALGO_SHA256;
	sinfo->type = RPMSIG_DIGEST_TYPE;
	sinfo->range = RPMSIG_HEADER;
	break;
    case RPMSIGTAG_SHA1:
	tagsize = 41; /* includes trailing \0 */
	tagtype = RPM_STRING_TYPE;
	tagcount = 1;
	hexstring = 1;
	sinfo->hashalgo = PGPHASHALGO_SHA1;
	sinfo->type = RPMSIG_DIGEST_TYPE;
	sinfo->range = RPMSIG_HEADER;
	break;
    case RPMSIGTAG_MD5:
	tagtype = RPM_BIN_TYPE;
	tagsize = 16;
	sinfo->hashalgo = PGPHASHALGO_MD5;
	sinfo->type = RPMSIG_DIGEST_TYPE;
	sinfo->range = (RPMSIG_HEADER|RPMSIG_PAYLOAD);
	break;
    case RPMSIGTAG_SIZE:
    case RPMSIGTAG_PAYLOADSIZE:
	tagsize = 4;
	tagcount = 1;
	tagtype = RPM_INT32_TYPE;
	sinfo->type = RPMSIG_OTHER_TYPE;
	sinfo->range = RPMSIG_PAYLOAD;
	break;
    case RPMSIGTAG_LONGSIZE:
    case RPMSIGTAG_LONGARCHIVESIZE:
	tagsize = 8;
	tagcount = 1;
	tagtype = RPM_INT64_TYPE;
	sinfo->type = RPMSIG_OTHER_TYPE;
	sinfo->range = RPMSIG_PAYLOAD;
	break;
    case RPMSIGTAG_RESERVEDSPACE:
	tagtype = RPM_BIN_TYPE;
	sinfo->type = RPMSIG_OTHER_TYPE;
	break;
    case RPMTAG_PAYLOADDIGEST:
	tagtype = RPM_STRING_ARRAY_TYPE;
	/* XXX: the hash algo is supposed to come from another tag */
	sinfo->hashalgo = PGPHASHALGO_SHA256;
	sinfo->type = RPMSIG_DIGEST_TYPE;
	sinfo->range = RPMSIG_PAYLOAD;
	/* XXX: get the last element, fail due to tagcount is arbitrary */
	if (rpmtdSetIndex(td, rpmtdCount(td)-1) == -1)
	    tagcount = 0;
	break;
    default:
	/* anything unknown just falls through for now */
	break;
    }

    if (tagtype && tagtype != td->type) {
	rasprintf(msg, _("%s tag %u: BAD, invalid type %u"),
			origin, td->tag, td->type);
	goto exit;
    }

    if (tagcount && tagcount != td->count) {
	rasprintf(msg, _("%s: tag %u: BAD, invalid count %u"),
			origin, td->tag, td->count);
	goto exit;
    }

    switch (td->type) {
    case RPM_STRING_TYPE:
    case RPM_STRING_ARRAY_TYPE:
	data = rpmtdGetString(td);
	if (data)
	    dlen = strlen(data);
	break;
    case RPM_BIN_TYPE:
	data = td->data;
	dlen = td->count;
	break;
    }

    /* MD5 has data length of 16, everything else is (much) larger */
    if (sinfo->hashalgo && (data == NULL || dlen < 16)) {
	rasprintf(msg, _("%s tag %u: BAD, invalid data %p (%u)"),
			origin, td->tag, data, dlen);
	goto exit;
    }

    if (td->type == RPM_STRING_TYPE && td->size == 0)
	td->size = dlen + 1;

    if (tagsize && (td->flags & RPMTD_IMMUTABLE) && tagsize != td->size) {
	rasprintf(msg, _("%s tag %u: BAD, invalid size %u"),
			origin, td->tag, td->size);
	goto exit;
    }

    if (hexstring) {
	for (const char * b = data; *b != '\0'; b++) {
	    if (strchr("0123456789abcdefABCDEF", *b) == NULL) {
		rasprintf(msg, _("%s: tag %u: BAD, not hex"), origin, td->tag);
		goto exit;
	    }
	}
    }

    if (sinfo->type == RPMSIG_SIGNATURE_TYPE) {
	if (pgpPrtParams(data, dlen, PGPTAG_SIGNATURE, &sinfo->sig)) {
	    rasprintf(msg, _("%s tag %u: BAD, invalid OpenPGP signature"),
		    origin, td->tag);
	    goto exit;
	}
	sinfo->hashalgo = pgpDigParamsAlgo(sinfo->sig, PGPVAL_HASHALGO);
	sinfo->keyid = pgpGrab(sinfo->sig->signid+4, 4);
    } else if (sinfo->type == RPMSIG_DIGEST_TYPE) {
	if (td->type == RPM_BIN_TYPE)
	    sinfo->dig = pgpHexStr(data, dlen);
	else
	    sinfo->dig = xstrdup(data);
    }

    sinfo->tag = td->tag;
    if (sinfo->hashalgo)
	sinfo->id = td->tag;

    rc = RPMRC_OK;

exit:
    return rc;
}

void rpmsinfoFini(struct rpmsinfo_s *sinfo)
{
    if (sinfo) {
	if (sinfo->type == RPMSIG_SIGNATURE_TYPE)
	    pgpDigParamsFree(sinfo->sig);
	else if (sinfo->type == RPMSIG_DIGEST_TYPE)
	    free(sinfo->dig);
	memset(sinfo, 0, sizeof(*sinfo));
    }
}

/**
 * Print package size (debug purposes only)
 * @param fd			package file handle
 * @param sigh			signature header
 */
static void printSize(FD_t fd, Header sigh)
{
    struct stat st;
    int fdno = Fileno(fd);
    size_t siglen = headerSizeof(sigh, HEADER_MAGIC_YES);
    size_t pad = (8 - (siglen % 8)) % 8; /* 8-byte pad */
    struct rpmtd_s sizetag;
    rpm_loff_t datalen = 0;

    /* Print package component sizes. */
    if (headerGet(sigh, RPMSIGTAG_LONGSIZE, &sizetag, HEADERGET_DEFAULT)) {
	rpm_loff_t *tsize = rpmtdGetUint64(&sizetag);
	datalen = (tsize) ? *tsize : 0;
    } else if (headerGet(sigh, RPMSIGTAG_SIZE, &sizetag, HEADERGET_DEFAULT)) {
	rpm_off_t *tsize = rpmtdGetUint32(&sizetag);
	datalen = (tsize) ? *tsize : 0;
    }
    rpmtdFreeData(&sizetag);

    rpmlog(RPMLOG_DEBUG,
		"Expected size: %12" PRIu64 \
		" = lead(%d)+sigs(%zd)+pad(%zd)+data(%" PRIu64 ")\n",
		RPMLEAD_SIZE+siglen+pad+datalen,
		RPMLEAD_SIZE, siglen, pad, datalen);

    if (fstat(fdno, &st) == 0) {
	rpmlog(RPMLOG_DEBUG,
		"  Actual size: %12" PRIu64 "\n", (rpm_loff_t) st.st_size);
    }
}

rpmRC rpmReadSignature(FD_t fd, Header * sighp, char ** msg)
{
    char *buf = NULL;
    struct hdrblob_s blob;
    Header sigh = NULL;
    rpmRC rc = RPMRC_FAIL;		/* assume failure */

    if (sighp)
	*sighp = NULL;

    if (hdrblobRead(fd, 1, 1, RPMTAG_HEADERSIGNATURES, &blob, &buf) != RPMRC_OK)
	goto exit;
    
    /* OK, blob looks sane, load the header. */
    if (hdrblobImport(&blob, 0, &sigh, &buf) != RPMRC_OK)
	goto exit;

    printSize(fd, sigh);
    rc = RPMRC_OK;

exit:
    if (sighp && sigh && rc == RPMRC_OK)
	*sighp = headerLink(sigh);
    headerFree(sigh);

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

rpmRC rpmGenerateSignature(char *SHA256, char *SHA1, uint8_t *MD5,
			rpm_loff_t size, rpm_loff_t payloadSize, FD_t fd)
{
    Header sig = headerNew();
    struct rpmtd_s td;
    rpmRC rc = RPMRC_OK;
    char *reservedSpace;
    int spaceSize = 32; /* always reserve a bit of space */
    int gpgSize = rpmExpandNumeric("%{__gpg_reserved_space}");

    /* Prepare signature */
    if (SHA256) {
	rpmtdReset(&td);
	td.tag = RPMSIGTAG_SHA256;
	td.count = 1;
	td.type = RPM_STRING_TYPE;
	td.data = SHA256;
	headerPut(sig, &td, HEADERPUT_DEFAULT);
    }

    if (SHA1) {
	rpmtdReset(&td);
	td.tag = RPMSIGTAG_SHA1;
	td.count = 1;
	td.type = RPM_STRING_TYPE;
	td.data = SHA1;
	headerPut(sig, &td, HEADERPUT_DEFAULT);
    }

    if (MD5) {
	rpmtdReset(&td);
	td.tag = RPMSIGTAG_MD5;
	td.count = 16;
	td.type = RPM_BIN_TYPE;
	td.data = MD5;
	headerPut(sig, &td, HEADERPUT_DEFAULT);
    }

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

	/* adjust for the size difference between 64- and 32bit tags */
	spaceSize -= 8;
    }

    if (gpgSize > 0)
	spaceSize += gpgSize;

    if (spaceSize > 0) {
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
    headerFree(sig);
    return rc;
}


static const char * rpmSigString(rpmRC res)
{
    const char * str;
    switch (res) {
    case RPMRC_OK:		str = "OK";		break;
    case RPMRC_FAIL:		str = "BAD";		break;
    case RPMRC_NOKEY:		str = "NOKEY";		break;
    case RPMRC_NOTTRUSTED:	str = "NOTTRUSTED";	break;
    default:
    case RPMRC_NOTFOUND:	str = "UNKNOWN";	break;
    }
    return str;
}

static const char *rangeName(int range)
{
    switch (range) {
    case RPMSIG_HEADER:				return _("Header ");
    case RPMSIG_PAYLOAD:			return _("Payload ");
    }
    /* trad. output for (RPMSIG_HEADER|RPMSIG_PAYLOAD) range is "" */
    return "";
}

static rpmRC verifyDigest(struct rpmsinfo_s *sinfo, DIGEST_CTX digctx,
			  char **msg)
{
    rpmRC res = RPMRC_FAIL; /* assume failure */
    char * dig = NULL;
    size_t diglen = 0;
    DIGEST_CTX ctx = rpmDigestDup(digctx);
    char *title = rstrscat(NULL, rangeName(sinfo->range),
			   pgpValString(PGPVAL_HASHALGO, sinfo->hashalgo),
			   _(" digest:"), NULL);

    if (rpmDigestFinal(ctx, (void **)&dig, &diglen, 1) || diglen == 0) {
	rasprintf(msg, "%s %s", title, rpmSigString(res));
	goto exit;
    }

    if (strcasecmp(sinfo->dig, dig) == 0) {
	res = RPMRC_OK;
	rasprintf(msg, "%s %s (%s)", title, rpmSigString(res), sinfo->dig);
    } else {
	rasprintf(msg, "%s: %s Expected(%s) != (%s)",
		  title, rpmSigString(res), sinfo->dig, dig);
    }

exit:
    free(dig);
    free(title);
    return res;
}

/**
 * Verify DSA/RSA signature.
 * @param keyring	pubkey keyring
 * @param sinfo		OpenPGP signature parameters
 * @param hashctx	digest context
 * @retval msg		verbose success/failure text
 * @return 		RPMRC_OK on success
 */
static rpmRC
verifySignature(rpmKeyring keyring, struct rpmsinfo_s *sinfo,
		DIGEST_CTX hashctx, char **msg)
{
    rpmRC res = rpmKeyringVerifySig(keyring, sinfo->sig, hashctx);

    char *sigid = pgpIdentItem(sinfo->sig);
    rasprintf(msg, "%s%s: %s", rangeName(sinfo->range), sigid,
		rpmSigString(res));
    free(sigid);
    return res;
}

rpmRC
rpmVerifySignature(rpmKeyring keyring, struct rpmsinfo_s *sinfo,
		   DIGEST_CTX ctx, char ** result)
{
    rpmRC res = RPMRC_NOTFOUND;
    char *msg = NULL;

    if (sinfo->sig == NULL || ctx == NULL)
	goto exit;

    if (sinfo->type == RPMSIG_DIGEST_TYPE)
	res = verifyDigest(sinfo, ctx, &msg);
    else if (sinfo->type == RPMSIG_SIGNATURE_TYPE)
	res = verifySignature(keyring, sinfo, ctx, &msg);

exit:
    if (res == RPMRC_NOTFOUND) {
	rasprintf(&msg,
		  _("Verify signature: BAD PARAMETERS (%d %p %d %p)"),
		  sinfo->tag, sinfo->sig, sinfo->hashalgo, ctx);
	res = RPMRC_FAIL;
    }

    if (result) {
	*result = msg;
    } else {
	free(msg);
    }
    return res;
}
