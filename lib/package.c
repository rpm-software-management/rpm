/** \ingroup header
 * \file lib/package.c
 */

#include "system.h"

#include <netinet/in.h>
#include <pthread.h>

#include <rpm/rpmlib.h>			/* XXX RPMSIGTAG, other sig stuff */
#include <rpm/rpmts.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmkeyring.h>

#include "lib/rpmlead.h"
#include "lib/signature.h"
#include "rpmio/digest.h"
#include "rpmio/rpmio_internal.h"	/* fd digest bits */
#include "lib/header_internal.h"	/* XXX headerCheck */

#include "debug.h"

/** \ingroup header
 * Translate and merge legacy signature tags into header.
 * @param h		header (dest)
 * @param sigh		signature header (src)
 */
static void headerMergeLegacySigs(Header h, Header sigh)
{
    HeaderIterator hi;
    struct rpmtd_s td;

    hi = headerInitIterator(sigh);
    for (; headerNext(hi, &td); rpmtdFreeData(&td))
    {
	switch (td.tag) {
	/* XXX Translate legacy signature tag values. */
	case RPMSIGTAG_SIZE:
	    td.tag = RPMTAG_SIGSIZE;
	    break;
	case RPMSIGTAG_PGP:
	    td.tag = RPMTAG_SIGPGP;
	    break;
	case RPMSIGTAG_MD5:
	    td.tag = RPMTAG_SIGMD5;
	    break;
	case RPMSIGTAG_GPG:
	    td.tag = RPMTAG_SIGGPG;
	    break;
	case RPMSIGTAG_PGP5:
	    td.tag = RPMTAG_SIGPGP5;
	    break;
	case RPMSIGTAG_PAYLOADSIZE:
	    td.tag = RPMTAG_ARCHIVESIZE;
	    break;
	case RPMSIGTAG_SHA1:
	case RPMSIGTAG_DSA:
	case RPMSIGTAG_RSA:
	default:
	    if (!(td.tag >= HEADER_SIGBASE && td.tag < HEADER_TAGBASE))
		continue;
	    break;
	}
	if (!headerIsEntry(h, td.tag)) {
	    switch(td.type) {
	    case RPM_NULL_TYPE:
		continue;
		break;
	    case RPM_CHAR_TYPE:
	    case RPM_INT8_TYPE:
	    case RPM_INT16_TYPE:
	    case RPM_INT32_TYPE:
	    case RPM_INT64_TYPE:
		if (td.count != 1)
		    continue;
		break;
	    case RPM_STRING_TYPE:
	    case RPM_BIN_TYPE:
		if (td.count >= 16*1024)
		    continue;
		break;
	    case RPM_STRING_ARRAY_TYPE:
	    case RPM_I18NSTRING_TYPE:
		continue;
		break;
	    }
	    (void) headerPut(h, &td, HEADERPUT_DEFAULT);
	}
    }
    headerFreeIterator(hi);
}

static unsigned int getKeyid(pgpDigParams sigp)
{
    return (sigp != NULL) ? pgpGrab(sigp->signid+4, 4) : 0;
}

/**
 * Remember current key id.
 * XXX: This s*** needs to die. Hook it into keyring or sumthin...
 * @param keyid		signature keyid
 * @return		0 if new keyid, otherwise 1
 */
static int stashKeyid(unsigned int keyid)
{
    static pthread_mutex_t keyid_lock = PTHREAD_MUTEX_INITIALIZER;
    static const unsigned int nkeyids_max = 256;
    static unsigned int nkeyids = 0;
    static unsigned int nextkeyid  = 0;
    static unsigned int * keyids;

    int i;
    int seen = 0;

    if (keyid == 0)
	return 0;

    /* Just pretend we didn't see the keyid if we fail to lock */
    if (pthread_mutex_lock(&keyid_lock))
	return 0;

    if (keyids != NULL)
    for (i = 0; i < nkeyids; i++) {
	if (keyid == keyids[i]) {
	    seen = 1;
	    goto exit;
        }
    }

    if (nkeyids < nkeyids_max) {
	nkeyids++;
	keyids = xrealloc(keyids, nkeyids * sizeof(*keyids));
    }
    if (keyids)		/* XXX can't happen */
	keyids[nextkeyid] = keyid;
    nextkeyid++;
    nextkeyid %= nkeyids_max;

exit:
    pthread_mutex_unlock(&keyid_lock);
    return seen;
}

static void ei2td(const struct entryInfo_s *info,
		  unsigned char * dataStart, size_t siglen,
		  struct rpmtd_s *td)
{
    td->tag = info->tag;
    td->type = info->type;
    td->count = info->count;
    td->size = siglen;
    td->data = dataStart + info->offset;
    td->flags = RPMTD_IMMUTABLE;
}

/*
 * Argument monster to verify header-only signature/digest if there is
 * one, otherwisereturn RPMRC_NOTFOUND to signal for plain sanity check.
 */
static rpmRC headerSigVerify(rpmKeyring keyring, rpmVSFlags vsflags,
			     hdrblob blob, hdrblob dstblob,
			     unsigned int *keyidp, char **buf)
{
    rpmRC rc = RPMRC_FAIL;
    pgpDigParams sig = NULL;
    struct rpmtd_s sigtd;
    struct entryInfo_s einfo;
    struct sigtInfo_s sinfo;

    rpmtdReset(&sigtd);
    memset(&einfo, 0, sizeof(einfo));

    /* Find a header-only digest/signature tag. */
    for (int i = (blob == dstblob) ? blob->ril : 1; i < blob->il; i++) {
	ei2h(blob->pe+i, &einfo);

	switch (einfo.tag) {
	case RPMTAG_SHA1HEADER:
	    if (vsflags & RPMVSF_NOSHA1HEADER)
		break;
	    if (sigtd.tag == 0)
		ei2td(&einfo, blob->dataStart, 0, &sigtd);
	    break;
	case RPMTAG_RSAHEADER:
	    if (vsflags & RPMVSF_NORSAHEADER)
		break;
	    ei2td(&einfo, blob->dataStart, einfo.count, &sigtd);
	    break;
	case RPMTAG_DSAHEADER:
	    if (vsflags & RPMVSF_NODSAHEADER)
		break;
	    ei2td(&einfo, blob->dataStart, einfo.count, &sigtd);
	    break;
	default:
	    break;
	}
    }

    /* No header-only digest/signature found, get outta here */
    if (sigtd.tag == 0) {
	rc = RPMRC_NOTFOUND;
	goto exit;
    }

    if (rpmSigInfoParse(&sigtd, "header", &sinfo, &sig, buf))
	goto exit;

    if (sinfo.hashalgo) {
	DIGEST_CTX ctx = rpmDigestInit(sinfo.hashalgo, RPMDIGEST_NONE);
	int32_t ildl[2] = { htonl(dstblob->ril), htonl(dstblob->rdl) };

	rpmDigestUpdate(ctx, rpm_header_magic, sizeof(rpm_header_magic));
	rpmDigestUpdate(ctx, ildl, sizeof(ildl));
	rpmDigestUpdate(ctx, dstblob->pe, (dstblob->ril * sizeof(*dstblob->pe)));
	rpmDigestUpdate(ctx, dstblob->dataStart, dstblob->rdl);

	rc = rpmVerifySignature(keyring, &sigtd, sig, ctx, buf);

	if (keyidp && sinfo.type == RPMSIG_SIGNATURE_TYPE)
	    *keyidp = getKeyid(sig);

    	rpmDigestFinal(ctx, NULL, NULL, 0);
    }

exit:
    rpmtdFreeData(&sigtd);
    pgpDigParamsFree(sig);

    return rc;
}

rpmRC headerCheck(rpmts ts, const void * uh, size_t uc, char ** msg)
{
    rpmRC rc = RPMRC_FAIL;
    rpmVSFlags vsflags = rpmtsVSFlags(ts);
    rpmKeyring keyring = rpmtsGetKeyring(ts, 1);
    struct hdrblob_s blob;

    if (hdrblobInit(uh, uc, 0, 0, &blob, msg) == RPMRC_OK) {
	rpmswEnter(rpmtsOp(ts, RPMTS_OP_DIGEST), 0);
	rc = headerSigVerify(keyring, vsflags, &blob, &blob, NULL, msg);
	rpmswExit(rpmtsOp(ts, RPMTS_OP_DIGEST), uc);

	if (rc == RPMRC_NOTFOUND && msg != NULL && *msg == NULL)
	    rasprintf(msg, "Header sanity check: OK");
    }

    rpmKeyringFree(keyring);

    return rc;
}

static rpmRC rpmpkgReadHeader(FD_t fd, Header *hdrp, char ** msg)
{
    char *buf = NULL;
    struct hdrblob_s blob;
    Header h = NULL;
    rpmRC rc = RPMRC_FAIL;		/* assume failure */

    if (hdrp)
	*hdrp = NULL;
    if (msg)
	*msg = NULL;

    if (hdrblobRead(fd, 1, 1, RPMTAG_HEADERIMMUTABLE, &blob, &buf) != RPMRC_OK)
	goto exit;

    /* OK, blob looks sane, load the header. */
    rc = hdrblobImport(&blob, 0, &h, &buf);
    
exit:
    if (hdrp && h && rc == RPMRC_OK)
	*hdrp = headerLink(h);
    headerFree(h);

    if (msg != NULL && *msg == NULL && buf != NULL) {
	*msg = buf;
    } else {
	free(buf);
    }

    return rc;
}

rpmRC rpmReadHeader(rpmts ts, FD_t fd, Header *hdrp, char ** msg)
{
    return rpmpkgReadHeader(fd, hdrp, msg);
}

static void applyRetrofits(Header h, int leadtype)
{
    /* Retrofit RPMTAG_SOURCEPACKAGE to srpms for compatibility */
    if (leadtype == RPMLEAD_SOURCE && headerIsSource(h)) {
	if (!headerIsEntry(h, RPMTAG_SOURCEPACKAGE)) {
	    uint32_t one = 1;
	    headerPutUint32(h, RPMTAG_SOURCEPACKAGE, &one, 1);
	}
    }
    /*
     * Try to make sure binary rpms have RPMTAG_SOURCERPM set as that's
     * what we use for differentiating binary vs source elsewhere.
     */
    if (!headerIsEntry(h, RPMTAG_SOURCEPACKAGE) && headerIsSource(h)) {
	headerPutString(h, RPMTAG_SOURCERPM, "(none)");
    }
    /*
     * Convert legacy headers on the fly. Not having immutable region
     * equals a truly ancient package, do full retrofit. OTOH newer
     * packages might have been built with --nodirtokens, test and handle
     * the non-compressed filelist case separately.
     */
    if (!headerIsEntry(h, RPMTAG_HEADERIMMUTABLE))
	headerConvert(h, HEADERCONV_RETROFIT_V3);
    else if (headerIsEntry(h, RPMTAG_OLDFILENAMES))
	headerConvert(h, HEADERCONV_COMPRESSFILELIST);
}

static rpmRC rpmpkgRead(rpmKeyring keyring, rpmVSFlags vsflags, 
			FD_t fd,
			Header * hdrp, unsigned int *keyidp, char **msg)
{
    pgpDigParams sig = NULL;
    Header sigh = NULL;
    rpmTagVal sigtag;
    struct rpmtd_s sigtd;
    struct sigtInfo_s sinfo;
    Header h = NULL;
    rpmRC xx = 0;
    rpmRC rc = RPMRC_FAIL;	/* assume failure */
    int leadtype = -1;
    headerGetFlags hgeflags = HEADERGET_DEFAULT;

    if (hdrp) *hdrp = NULL;

    rpmtdReset(&sigtd);

    if ((xx = rpmLeadRead(fd, &leadtype, msg)) != RPMRC_OK) {
	/* Avoid message spew on manifests */
	if (xx == RPMRC_NOTFOUND) {
	    *msg = _free(*msg);
	}
	goto exit;
    }

    /* Read the signature header. */
    if (rpmReadSignature(fd, &sigh, msg) != RPMRC_OK)
	goto exit;

#define	_chk(_mask, _tag) \
	(sigtag == 0 && !(vsflags & (_mask)) && headerIsEntry(sigh, (_tag)))

    /*
     * Figger the most effective means of verification available, prefer
     * signatures over digests. Legacy header+payload entries are not used.
     * DSA will be preferred over RSA if both exist because tested first.
     */
    sigtag = 0;
    if (_chk(RPMVSF_NODSAHEADER, RPMSIGTAG_DSA)) {
	sigtag = RPMSIGTAG_DSA;
    } else if (_chk(RPMVSF_NORSAHEADER, RPMSIGTAG_RSA)) {
	sigtag = RPMSIGTAG_RSA;
    } else if (_chk(RPMVSF_NOSHA1HEADER, RPMSIGTAG_SHA1)) {
	sigtag = RPMSIGTAG_SHA1;
    }

    /* Read the metadata, computing digest(s) on the fly. */
    if (rpmpkgReadHeader(fd, &h, msg) != RPMRC_OK)
	goto exit;

    /* Any digests or signatures to check? */
    if (sigtag == 0) {
	rc = RPMRC_OK;
	goto exit;
    }

    /* Free up any previous "ok" message before signature/digest check */
    *msg = _free(*msg);

    /* Retrieve the tag parameters from the signature header. */
    if (!headerGet(sigh, sigtag, &sigtd, hgeflags))
	goto exit;

    if (rpmSigInfoParse(&sigtd, "package", &sinfo, &sig, msg) == RPMRC_OK) {
	struct rpmtd_s utd;
	DIGEST_CTX ctx = rpmDigestInit(sinfo.hashalgo, RPMDIGEST_NONE);


	if (headerGet(h, RPMTAG_HEADERIMMUTABLE, &utd, hgeflags)) {
	    rpmDigestUpdate(ctx, rpm_header_magic, sizeof(rpm_header_magic));
	    rpmDigestUpdate(ctx, utd.data, utd.count);
	    rpmtdFreeData(&utd);
	}
	/** @todo Implement disable/enable/warn/error/anal policy. */
	rc = rpmVerifySignature(keyring, &sigtd, sig, ctx, msg);

	rpmDigestFinal(ctx, NULL, NULL, 0);
    }

exit:
    if (xx)
	rc = xx;

    if (rc != RPMRC_FAIL && h != NULL && hdrp != NULL) {
	applyRetrofits(h, leadtype);

	/* Append (and remap) signature tags to the metadata. */
	headerMergeLegacySigs(h, sigh);

	/* Bump reference count for return. */
	*hdrp = headerLink(h);

	if (keyidp)
	    *keyidp = getKeyid(sig);
    }
    rpmtdFreeData(&sigtd);
    h = headerFree(h);
    pgpDigParamsFree(sig);
    sigh = headerFree(sigh);
    return rc;
}

rpmRC rpmReadPackageFile(rpmts ts, FD_t fd, const char * fn, Header * hdrp)
{
    rpmRC rc;
    rpmVSFlags vsflags = rpmtsVSFlags(ts);
    rpmKeyring keyring = rpmtsGetKeyring(ts, 1);
    unsigned int keyid = 0;
    char *msg = NULL;

    if (fn == NULL)
	fn = Fdescr(fd);

    rc = rpmpkgRead(keyring, vsflags, fd, hdrp, &keyid, &msg);

    switch (rc) {
    case RPMRC_OK:		/* Signature is OK. */
	rpmlog(RPMLOG_DEBUG, "%s: %s\n", fn, msg);
	break;
    case RPMRC_NOTTRUSTED:	/* Signature is OK, but key is not trusted. */
    case RPMRC_NOKEY:		/* Public key is unavailable. */
	/* XXX Print NOKEY/NOTTRUSTED warning only once. */
    {	int lvl = (stashKeyid(keyid) ? RPMLOG_DEBUG : RPMLOG_WARNING);
	rpmlog(lvl, "%s: %s\n", fn, msg);
    }	break;
    case RPMRC_NOTFOUND:	/* Signature is unknown type or manifest. */
	/* msg == NULL is probably a manifest */
	if (msg)
	    rpmlog(RPMLOG_WARNING, "%s: %s\n", fn, msg);
	break;
    default:
    case RPMRC_FAIL:		/* Signature does not verify. */
	rpmlog(RPMLOG_ERR, "%s: %s\n", fn, msg);
	break;
    }
    rpmKeyringFree(keyring);
    free(msg);

    return rc;
}



