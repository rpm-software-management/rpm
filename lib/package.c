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
	if (td.data == NULL) continue;	/* XXX can't happen */
	if (!headerIsEntry(h, td.tag)) {
	    if (hdrchkType(td.type))
		continue;
	    if (td.count < 0 || hdrchkData(td.count))
		continue;
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

/*
 * Argument monster to verify header-only signature/digest if there is
 * one, otherwisereturn RPMRC_NOTFOUND to signal for plain sanity check.
 */
static rpmRC headerSigVerify(rpmKeyring keyring, rpmVSFlags vsflags,
			     int il, int dl, int ril, int rdl,
			     entryInfo pe, unsigned char * dataStart,
			     char **buf)
{
    size_t siglen = 0;
    rpmRC rc = RPMRC_FAIL;
    pgpDigParams sig = NULL;
    struct rpmtd_s sigtd;
    struct entryInfo_s info, einfo;
    struct sigtInfo_s sinfo;

    rpmtdReset(&sigtd);
    memset(&info, 0, sizeof(info));
    memset(&einfo, 0, sizeof(einfo));

    /* Find a header-only digest/signature tag. */
    for (int i = ril; i < il; i++) {
	if (headerVerifyInfo(1, dl, pe+i, &einfo, 0) != -1) {
	    rasprintf(buf,
		_("tag[%d]: BAD, tag %d type %d offset %d count %d"),
		i, einfo.tag, einfo.type,
		einfo.offset, einfo.count);
	    goto exit;
	}

	switch (einfo.tag) {
	case RPMTAG_SHA1HEADER: {
	    size_t blen = 0;
	    unsigned const char * b = dataStart + einfo.offset;
	    unsigned const char * e = dataStart + dl;
	    if (vsflags & RPMVSF_NOSHA1HEADER)
		break;
	    for (; b < e && *b != '\0'; b++) {
		if (strchr("0123456789abcdefABCDEF", *b) == NULL)
		    break;
		blen++;
	    }
	    if (einfo.type != RPM_STRING_TYPE || *b != '\0' || blen != 40)
	    {
		rasprintf(buf, _("hdr SHA1: BAD, not hex"));
		goto exit;
	    }
	    if (info.tag == 0) {
		info = einfo;	/* structure assignment */
		siglen = blen + 1;
	    }
	    } break;
	case RPMTAG_RSAHEADER:
	    if (vsflags & RPMVSF_NORSAHEADER)
		break;
	    if (einfo.type != RPM_BIN_TYPE) {
		rasprintf(buf, _("hdr RSA: BAD, not binary"));
		goto exit;
	    }
	    info = einfo;	/* structure assignment */
	    siglen = info.count;
	    break;
	case RPMTAG_DSAHEADER:
	    if (vsflags & RPMVSF_NODSAHEADER)
		break;
	    if (einfo.type != RPM_BIN_TYPE) {
		rasprintf(buf, _("hdr DSA: BAD, not binary"));
		goto exit;
	    }
	    info = einfo;	/* structure assignment */
	    siglen = info.count;
	    break;
	default:
	    break;
	}
    }

    /* No header-only digest/signature found, get outta here */
    if (info.tag == 0) {
	rc = RPMRC_NOTFOUND;
	goto exit;
    }

    sigtd.tag = info.tag;
    sigtd.type = info.type;
    sigtd.count = info.count;
    sigtd.data = memcpy(xmalloc(siglen), dataStart + info.offset, siglen);
    sigtd.flags = RPMTD_ALLOCED;

    if (rpmSigInfoParse(&sigtd, "header", &sinfo, &sig, buf))
	goto exit;

    if (sinfo.hashalgo) {
	DIGEST_CTX ctx = rpmDigestInit(sinfo.hashalgo, RPMDIGEST_NONE);
	int32_t ildl[2] = { htonl(ril), htonl(rdl) };

	rpmDigestUpdate(ctx, rpm_header_magic, sizeof(rpm_header_magic));
	rpmDigestUpdate(ctx, ildl, sizeof(ildl));
	rpmDigestUpdate(ctx, pe, (ril * sizeof(*pe)));
	rpmDigestUpdate(ctx, dataStart, rdl);

	rc = rpmVerifySignature(keyring, &sigtd, sig, ctx, buf);

    	rpmDigestFinal(ctx, NULL, NULL, 0);
    }

exit:
    rpmtdFreeData(&sigtd);
    pgpDigParamsFree(sig);

    return rc;
}

rpmRC headerVerifyRegion(rpmTagVal regionTag,
			struct indexEntry_s *entry, int il, int dl,
			entryInfo pe, unsigned char *dataStart,
			int *rilp, int *rdlp, char **buf)
{
    rpmRC rc = RPMRC_FAIL;
    struct entryInfo_s info;
    unsigned char * regionEnd = NULL;
    int32_t ril = 0;
    int32_t rdl = 0;

    /* Check that we have at least on tag */
    if (il < 1) {
	rasprintf(buf, _("region: no tags"));
	goto exit;
    }

    memset(entry, 0, sizeof(*entry));

    /* Check (and convert) the 1st tag element. */
    if (headerVerifyInfo(1, dl, pe, &entry->info, 0) != -1) {
	rasprintf(buf, _("tag[%d]: BAD, tag %d type %d offset %d count %d"),
		0, entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	goto exit;
    }

    /* Is there an immutable header region tag? */
    if (!(entry->info.tag == regionTag)) {
	rc = RPMRC_NOTFOUND;
	goto exit;
    }

    /* Is the region tag sane? */
    if (!(entry->info.type == REGION_TAG_TYPE &&
	  entry->info.count == REGION_TAG_COUNT)) {
	rasprintf(buf,
		_("region tag: BAD, tag %d type %d offset %d count %d"),
		entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	goto exit;
    }

    /* Is the trailer within the data area? */
    if (entry->info.offset + REGION_TAG_COUNT > dl) {
	rasprintf(buf, 
		_("region offset: BAD, tag %d type %d offset %d count %d"),
		entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	goto exit;
    }

    /* Is there an immutable header region tag trailer? */
    memset(&info, 0, sizeof(info));
    regionEnd = dataStart + entry->info.offset;
    (void) memcpy(&info, regionEnd, REGION_TAG_COUNT);
    regionEnd += REGION_TAG_COUNT;
    rdl = regionEnd - dataStart;

    if (headerVerifyInfo(1, il * sizeof(*pe) + REGION_TAG_COUNT, &info, &entry->info, 1) != -1 ||
	!(entry->info.tag == regionTag
       && entry->info.type == REGION_TAG_TYPE
       && entry->info.count == REGION_TAG_COUNT))
    {
	rasprintf(buf, 
		_("region trailer: BAD, tag %d type %d offset %d count %d"),
		entry->info.tag, entry->info.type,
		entry->info.offset, entry->info.count);
	goto exit;
    }

    /* Is the no. of tags in the region less than the total no. of tags? */
    ril = entry->info.offset/sizeof(*pe);
    if ((entry->info.offset % sizeof(*pe)) || ril > il) {
	rasprintf(buf, _("region size: BAD, ril(%d) > il(%d)"), ril, il);
	goto exit;
    }

    rc = RPMRC_OK;
    if (rilp)
	*rilp = ril;
    if (rdlp)
	*rdlp = rdl;

exit:
    return rc;
}

static rpmRC headerVerify(rpmKeyring keyring, rpmVSFlags vsflags,
			  const void * uh, size_t uc, char ** msg)
{
    char *buf = NULL;
    int32_t * ei = (int32_t *) uh;
    int32_t il = ntohl(ei[0]);
    int32_t dl = ntohl(ei[1]);
    entryInfo pe = (entryInfo) &ei[2];
    int32_t pvlen = sizeof(il) + sizeof(dl) + (il * sizeof(*pe)) + dl;
    unsigned char * dataStart = (unsigned char *) (pe + il);
    struct indexEntry_s entry;
    int32_t ril = 0;
    int32_t rdl = 0;
    rpmRC rc = RPMRC_FAIL;	/* assume failure */

    /* Is the blob the right size? */
    if (uc > 0 && pvlen != uc) {
	rasprintf(&buf, _("blob size(%d): BAD, 8 + 16 * il(%d) + dl(%d)"),
		(int)uc, (int)il, (int)dl);
	goto exit;
    }

    /* Verify header immutable region if there is one */
    rc = headerVerifyRegion(RPMTAG_HEADERIMMUTABLE,
			    &entry, il, dl, pe, dataStart,
			    &ril, &rdl, &buf);

    /* Verify header-only digest/signature if there is one we can use. */
    if (rc == RPMRC_OK) {
	rc = headerSigVerify(keyring, vsflags,
			     il, dl, ril, rdl,
			     pe, dataStart, &buf);
    }

exit:
    /* If no header-only digest/signature, then do simple sanity check. */
    if (rc == RPMRC_NOTFOUND) {
	int xx = headerVerifyInfo(ril-1, dl, pe+1, &entry.info, 0);
	if (xx != -1) {
	    rasprintf(&buf,
		_("tag[%d]: BAD, tag %d type %d offset %d count %d"),
		xx+1, entry.info.tag, entry.info.type,
		entry.info.offset, entry.info.count);
	    rc = RPMRC_FAIL;
	} else {
	    rasprintf(&buf, "Header sanity check: OK");
	    rc = RPMRC_OK;
	}
    }

    if (msg) 
	*msg = buf;
    else
	free(buf);

    return rc;
}

rpmRC headerCheck(rpmts ts, const void * uh, size_t uc, char ** msg)
{
    rpmRC rc;
    rpmVSFlags vsflags = rpmtsVSFlags(ts);
    rpmKeyring keyring = rpmtsGetKeyring(ts, 1);

    rpmswEnter(rpmtsOp(ts, RPMTS_OP_DIGEST), 0);
    rc = headerVerify(keyring, vsflags, uh, uc, msg);
    rpmswExit(rpmtsOp(ts, RPMTS_OP_DIGEST), uc);
    rpmKeyringFree(keyring);

    return rc;
}

static rpmRC rpmpkgReadHeader(rpmKeyring keyring, rpmVSFlags vsflags, 
		       FD_t fd, Header *hdrp, char ** msg)
{
    char *buf = NULL;
    int32_t block[4];
    int32_t il;
    int32_t dl;
    int32_t * ei = NULL;
    size_t uc;
    size_t nb;
    Header h = NULL;
    rpmRC rc = RPMRC_FAIL;		/* assume failure */
    int xx;

    if (hdrp)
	*hdrp = NULL;
    if (msg)
	*msg = NULL;

    memset(block, 0, sizeof(block));
    if ((xx = Freadall(fd, block, sizeof(block))) != sizeof(block)) {
	rasprintf(&buf, 
		_("hdr size(%d): BAD, read returned %d"), (int)sizeof(block), xx);
	goto exit;
    }
    if (memcmp(block, rpm_header_magic, sizeof(rpm_header_magic))) {
	rasprintf(&buf, _("hdr magic: BAD"));
	goto exit;
    }
    il = ntohl(block[2]);
    if (hdrchkTags(il)) {
	rasprintf(&buf, _("hdr tags: BAD, no. of tags(%d) out of range"), il);
	goto exit;
    }
    dl = ntohl(block[3]);
    if (hdrchkData(dl)) {
	rasprintf(&buf,
		  _("hdr data: BAD, no. of bytes(%d) out of range"), dl);
	goto exit;
    }

    nb = (il * sizeof(struct entryInfo_s)) + dl;
    uc = sizeof(il) + sizeof(dl) + nb;
    ei = xmalloc(uc);
    ei[0] = block[2];
    ei[1] = block[3];
    if ((xx = Freadall(fd, (char *)&ei[2], nb)) != nb) {
	rasprintf(&buf, _("hdr blob(%zd): BAD, read returned %d"), nb, xx);
	goto exit;
    }

    /* Sanity check header tags */
    rc = headerVerify(keyring, vsflags, ei, uc, &buf);
    if (rc != RPMRC_OK)
	goto exit;

    /* OK, blob looks sane, load the header. */
    h = headerImport(ei, uc, 0);
    if (h == NULL) {
	free(buf);
	rasprintf(&buf, _("hdr load: BAD"));
	rc = RPMRC_FAIL;
        goto exit;
    }
    ei = NULL;	/* XXX will be freed with header */
    
exit:
    if (hdrp && h && rc == RPMRC_OK)
	*hdrp = headerLink(h);
    free(ei);
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
    rpmRC rc;
    rpmKeyring keyring = rpmtsGetKeyring(ts, 1);
    rpmVSFlags vsflags = rpmtsVSFlags(ts);

    rc = rpmpkgReadHeader(keyring, vsflags, fd, hdrp, msg);

    rpmKeyringFree(keyring);
    return rc;
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
    rpmRC rc = RPMRC_FAIL;	/* assume failure */
    int leadtype = -1;
    headerGetFlags hgeflags = HEADERGET_DEFAULT;

    if (hdrp) *hdrp = NULL;

    rpmtdReset(&sigtd);

    if ((rc = rpmLeadRead(fd, NULL, &leadtype, msg)) != RPMRC_OK) {
	/* Avoid message spew on manifests */
	if (rc == RPMRC_NOTFOUND) {
	    *msg = _free(*msg);
	}
	goto exit;
    }

    /* Read the signature header. */
    rc = rpmReadSignature(fd, &sigh, RPMSIGTYPE_HEADERSIG, msg);

    if (rc != RPMRC_OK) {
	goto exit;
    }

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
    h = NULL;

    rc = rpmpkgReadHeader(keyring, vsflags, fd, &h, msg);

    if (rc != RPMRC_OK || h == NULL) {
	goto exit;
    }

    /* Any digests or signatures to check? */
    if (sigtag == 0) {
	rc = RPMRC_OK;
	goto exit;
    }

    /* Free up any previous "ok" message before signature/digest check */
    *msg = _free(*msg);

    /* Retrieve the tag parameters from the signature header. */
    if (!headerGet(sigh, sigtag, &sigtd, hgeflags)) {
	rc = RPMRC_FAIL;
	goto exit;
    }

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
    } else {
	rc = RPMRC_FAIL;
    }

exit:
    if (rc != RPMRC_FAIL && h != NULL && hdrp != NULL) {
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
    sigh = rpmFreeSignature(sigh);
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



