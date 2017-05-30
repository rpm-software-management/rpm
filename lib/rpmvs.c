#include "system.h"

#include <rpm/rpmkeyring.h>
#include "lib/rpmvs.h"
#include "rpmio/digest.h"

#include "debug.h"

struct rpmvs_s {
    struct rpmsinfo_s *sigs;
    rpmRC *rcs;
    char **results;
    int nsigs;
    int nalloced;
};

struct vfytag_s {
    rpmTagVal tag;
    rpmTagType tagtype;
    rpm_count_t tagcount;
    rpm_count_t tagsize;
};

static const struct vfytag_s rpmvfytags[] = {
    {	RPMSIGTAG_SIZE,			RPM_BIN_TYPE,		0,	0, },
    {	RPMSIGTAG_PGP,			RPM_BIN_TYPE,		0,	0, },
    {	RPMSIGTAG_MD5,			RPM_BIN_TYPE,		0,	16, },
    {	RPMSIGTAG_GPG,			RPM_BIN_TYPE,		0,	0, },
    { 	RPMSIGTAG_PGP5,			RPM_BIN_TYPE,		0,	0, },
    {	RPMSIGTAG_PAYLOADSIZE,		RPM_INT32_TYPE,		1,	4, },
    {	RPMSIGTAG_RESERVEDSPACE,	RPM_BIN_TYPE,		0,	0, },
    {	RPMTAG_DSAHEADER,		RPM_BIN_TYPE,		0,	0, },
    {	RPMTAG_RSAHEADER,		RPM_BIN_TYPE,		0,	0, },
    {	RPMTAG_SHA1HEADER,		RPM_STRING_TYPE,	1,	41, },
    {	RPMSIGTAG_LONGSIZE,		RPM_INT64_TYPE,		1,	8, },
    {	RPMSIGTAG_LONGARCHIVESIZE,	RPM_INT64_TYPE,		1,	8, },
    {	RPMTAG_SHA256HEADER,		RPM_STRING_TYPE,	1,	65, },
    {	RPMTAG_PAYLOADDIGEST,		RPM_STRING_ARRAY_TYPE,	0,	0, },
    { 0 } /* sentinel */
};

struct vfyinfo_s {
    rpmTagVal tag;
    struct rpmsinfo_s vi;
};

static const struct vfyinfo_s rpmvfyitems[] = {
    {	RPMSIGTAG_SIZE,
	{ RPMSIG_OTHER_TYPE,		0,
	(RPMSIG_HEADER|RPMSIG_PAYLOAD), 0, }, },
    {	RPMSIGTAG_PGP,
	{ RPMSIG_SIGNATURE_TYPE,		RPMVSF_NORSA,
	(RPMSIG_HEADER|RPMSIG_PAYLOAD), 0, }, },
    {	RPMSIGTAG_MD5,
	{ RPMSIG_DIGEST_TYPE,		RPMVSF_NOMD5,
	(RPMSIG_HEADER|RPMSIG_PAYLOAD), PGPHASHALGO_MD5, }, },
    {	RPMSIGTAG_GPG,
	{ RPMSIG_SIGNATURE_TYPE,		RPMVSF_NODSA,
	(RPMSIG_HEADER|RPMSIG_PAYLOAD), 0, }, },
    { 	RPMSIGTAG_PGP5,
	{ RPMSIG_SIGNATURE_TYPE,		RPMVSF_NORSA,
	(RPMSIG_HEADER|RPMSIG_PAYLOAD), 0, }, },
    {	RPMSIGTAG_PAYLOADSIZE,
	{ RPMSIG_OTHER_TYPE,		0,
	(RPMSIG_PAYLOAD),		0, }, },
    {	RPMSIGTAG_RESERVEDSPACE,
	{ RPMSIG_OTHER_TYPE,		0,
	0,				0, }, },
    {	RPMTAG_DSAHEADER,
	{ RPMSIG_SIGNATURE_TYPE,		RPMVSF_NODSAHEADER,
	(RPMSIG_HEADER),		0, }, },
    {	RPMTAG_RSAHEADER,
	{ RPMSIG_SIGNATURE_TYPE,		RPMVSF_NORSAHEADER,
	(RPMSIG_HEADER),		0, }, },
    {	RPMTAG_SHA1HEADER,
	{ RPMSIG_DIGEST_TYPE,		RPMVSF_NOSHA1HEADER,
	(RPMSIG_HEADER),		PGPHASHALGO_SHA1, }, },
    {	RPMSIGTAG_LONGSIZE,
	{ RPMSIG_OTHER_TYPE, 		0,
	(RPMSIG_HEADER|RPMSIG_PAYLOAD), 0, }, },
    {	RPMSIGTAG_LONGARCHIVESIZE,
	{ RPMSIG_OTHER_TYPE,		0,
	(RPMSIG_HEADER|RPMSIG_PAYLOAD),	0, }, },
    {	RPMTAG_SHA256HEADER,
	{ RPMSIG_DIGEST_TYPE,		RPMVSF_NOSHA256HEADER,
	(RPMSIG_HEADER),		PGPHASHALGO_SHA256, }, },
    {	RPMTAG_PAYLOADDIGEST,
	{ RPMSIG_DIGEST_TYPE,		RPMVSF_NOPAYLOAD,
	(RPMSIG_PAYLOAD),		PGPHASHALGO_SHA256, }, },
    { 0 } /* sentinel */
};

static const char *rangeName(int range);
static const char * rpmSigString(rpmRC res);
static rpmRC rpmVerifySignature(rpmKeyring keyring, struct rpmsinfo_s *sinfo,
			       DIGEST_CTX ctx, char ** result);

static int sinfoLookup(rpmTagVal tag)
{
    const struct vfyinfo_s *start = &rpmvfyitems[0];
    int ix = -1;
    for (const struct vfyinfo_s *si = start; si->tag; si++) {
	if (tag == si->tag) {
	    ix = si - start;
	    break;
	}
    }
    return ix;
}

static int validHex(const char *str, size_t slen)
{
    int valid = 0; /* Assume invalid */
    const char *b;

    /* Our hex data is always even sized and at least sha-1 long */
    if (slen % 2 || slen < 40)
	goto exit;

    for (b = str ; *b != '\0'; b++) {
	if (strchr("0123456789abcdefABCDEF", *b) == NULL)
	    goto exit;
    }
    valid = 1;

exit:
    return valid;
}

static rpmRC rpmsinfoInit(rpmtd td, const char *origin,
		      struct rpmsinfo_s *sinfo, char **msg)
{
    rpmRC rc = RPMRC_FAIL;
    const void *data = NULL;
    const struct vfytag_s *tinfo;
    const struct vfyinfo_s *vinfo;
    rpm_count_t dlen = 0;
    int ix;

    if ((ix = sinfoLookup(td->tag)) == -1) {
	/* anything unknown just falls through for now */
	rc = RPMRC_OK;
	goto exit;
    }
    vinfo = &rpmvfyitems[ix];
    tinfo = &rpmvfytags[ix];
    assert(tinfo->tag == vinfo->tag);

    *sinfo = rpmvfyitems[ix].vi; /* struct assignment */

    if (tinfo->tagtype && tinfo->tagtype != td->type) {
	rasprintf(msg, _("%s tag %u: invalid type %u"),
			origin, td->tag, td->type);
	goto exit;
    }

    if (tinfo->tagcount && tinfo->tagcount != td->count) {
	rasprintf(msg, _("%s: tag %u: invalid count %u"),
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
	rasprintf(msg, _("%s tag %u: invalid data %p (%u)"),
			origin, td->tag, data, dlen);
	goto exit;
    }

    if (td->type == RPM_STRING_TYPE && td->size == 0)
	td->size = dlen + 1;

    if (tinfo->tagsize && (td->flags & RPMTD_IMMUTABLE) &&
		tinfo->tagsize != td->size) {
	rasprintf(msg, _("%s tag %u: invalid size %u"),
			origin, td->tag, td->size);
	goto exit;
    }

    if (sinfo->type == RPMSIG_SIGNATURE_TYPE) {
	if (pgpPrtParams(data, dlen, PGPTAG_SIGNATURE, &sinfo->sig)) {
	    rasprintf(msg, _("%s tag %u: invalid OpenPGP signature"),
		    origin, td->tag);
	    goto exit;
	}
	sinfo->hashalgo = pgpDigParamsAlgo(sinfo->sig, PGPVAL_HASHALGO);
	sinfo->keyid = pgpGrab(sinfo->sig->signid+4, 4);
    } else if (sinfo->type == RPMSIG_DIGEST_TYPE) {
	if (td->type == RPM_BIN_TYPE) {
	    sinfo->dig = pgpHexStr(data, dlen);
	} else {
	    if (!validHex(data, dlen)) {
		rasprintf(msg, _("%s: tag %u: invalid hex"), origin, td->tag);
		goto exit;
	    }
	    sinfo->dig = xstrdup(data);
	}
    }

    if (sinfo->hashalgo)
	sinfo->id = (td->tag << 16) | rpmtdGetIndex(td);

    rc = RPMRC_OK;

exit:
    return rc;
}

static void rpmsinfoFini(struct rpmsinfo_s *sinfo)
{
    if (sinfo) {
	if (sinfo->type == RPMSIG_SIGNATURE_TYPE)
	    pgpDigParamsFree(sinfo->sig);
	else if (sinfo->type == RPMSIG_DIGEST_TYPE)
	    free(sinfo->dig);
	free(sinfo->descr);
	memset(sinfo, 0, sizeof(*sinfo));
    }
}

static int rpmsinfoDisabled(const struct rpmsinfo_s *sinfo, rpmVSFlags vsflags)
{
    if (!(sinfo->type & RPMSIG_VERIFIABLE_TYPE))
	return 1;
    if (vsflags & sinfo->disabler)
	return 1;
    if ((vsflags & RPMVSF_NEEDPAYLOAD) && (sinfo->range & RPMSIG_PAYLOAD))
	return 1;
    return 0;
}

static void rpmvsReserve(struct rpmvs_s *vs, int n)
{
    if (vs->nsigs + n >= vs->nalloced) {
	vs->nalloced = (vs->nsigs * 2) + n;
	vs->rcs = xrealloc(vs->rcs, vs->nalloced * sizeof(*vs->rcs));
	vs->results = xrealloc(vs->results, vs->nalloced * sizeof(*vs->results));
	vs->sigs = xrealloc(vs->sigs, vs->nalloced * sizeof(*vs->sigs));
    }
}

const char *rpmsinfoDescr(struct rpmsinfo_s *sinfo)
{
    if (sinfo->descr == NULL) {
	char *t;
	switch (sinfo->type) {
	case RPMSIG_DIGEST_TYPE:
	    rasprintf(&sinfo->descr, _("%s%s %s"),
		    rangeName(sinfo->range),
		    pgpValString(PGPVAL_HASHALGO, sinfo->hashalgo),
		    _("digest"));
	    break;
	case RPMSIG_SIGNATURE_TYPE:
	    t = sinfo->sig ? pgpIdentItem(sinfo->sig) : NULL;
	    rasprintf(&sinfo->descr, _("%s%s"),
		    rangeName(sinfo->range), t ? t : _("signature"));
	    free(t);
	    break;
	}
    }
    return sinfo->descr;
}

char *rpmsinfoMsg(struct rpmsinfo_s *sinfo, rpmRC rc, const char *emsg)
{
    char *msg = NULL;
    if (emsg) {
	rasprintf(&msg, "%s: %s (%s)",
		rpmsinfoDescr(sinfo), rpmSigString(rc), emsg);
    } else {
	rasprintf(&msg, "%s: %s", rpmsinfoDescr(sinfo), rpmSigString(rc));
    }
    return msg;
}

void rpmvsAppend(struct rpmvs_s *sis, hdrblob blob, rpmTagVal tag)
{
    struct rpmtd_s td;
    rpmRC rc = hdrblobGet(blob, tag, &td);

    if (rc == RPMRC_OK) {
	const char *o = (blob->il > blob->ril) ? _("header") : _("package");
	int ix;

	rpmvsReserve(sis, rpmtdCount(&td));

	while ((ix = rpmtdNext(&td)) >= 0) {
	    sis->results[sis->nsigs] = NULL;
	    sis->rcs[sis->nsigs] = rpmsinfoInit(&td, o,
						&sis->sigs[sis->nsigs],
						&sis->results[sis->nsigs]);
	    sis->nsigs++;
	}
	rpmtdFreeData(&td);
    }
}

struct rpmvs_s *rpmvsCreate(hdrblob blob, rpmVSFlags vsflags)
{
    struct rpmvs_s *sis = xcalloc(1, sizeof(*sis));

    rpmvsReserve(sis, 2); /* XXX bump this up later */

    for (const struct vfyinfo_s *si = &rpmvfyitems[0]; si->tag; si++) {
	if (rpmsinfoDisabled(&si->vi, vsflags))
	    continue;
	rpmvsAppend(sis, blob, si->tag);
    }
    return sis;
}

struct rpmvs_s *rpmvsFree(struct rpmvs_s *sis)
{
    if (sis) {
	free(sis->rcs);
	for (int i = 0; i < sis->nsigs; i++) {
	    rpmsinfoFini(&sis->sigs[i]);
	    free(sis->results[i]);
	}
	free(sis->sigs);
	free(sis->results);
	free(sis);
    }
    return NULL;
}

void rpmvsInitDigests(struct rpmvs_s *sis, int range, rpmDigestBundle bundle)
{
    for (int i = 0; i < sis->nsigs; i++) {
	struct rpmsinfo_s *sinfo = &sis->sigs[i];
	if (sinfo->range & range) {
	    if (sis->rcs[i] == RPMRC_OK)
		rpmDigestBundleAddID(bundle, sinfo->hashalgo, sinfo->id, 0);
	}
    }
}

int rpmvsVerifyItems(struct rpmvs_s *sis, int range, rpmDigestBundle bundle,
		       rpmKeyring keyring, rpmsinfoCb cb, void *cbdata)
{
    int failed = 0;

    for (int i = 0; i < sis->nsigs; i++) {
	struct rpmsinfo_s *sinfo = &sis->sigs[i];

	if (sinfo->range == range) {
	    if (sis->rcs[i] == RPMRC_OK) {
		DIGEST_CTX ctx = rpmDigestBundleDupCtx(bundle, sinfo->id);
		sis->results[i] = _free(sis->results[i]);
		sis->rcs[i] = rpmVerifySignature(keyring, sinfo, ctx, &sis->results[i]);
		rpmDigestFinal(ctx, NULL, NULL, 0);
		rpmDigestBundleFinal(bundle, sinfo->id, NULL, NULL, 0);
	    }

	    if (cb)
		sis->rcs[i] = cb(sinfo, sis->rcs[i], sis->results[i], cbdata);

	    if (sis->rcs[i] != RPMRC_OK)
		failed++;
	}
    }

    return failed;
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

    if (rpmDigestFinal(ctx, (void **)&dig, &diglen, 1) || diglen == 0)
	goto exit;

    if (strcasecmp(sinfo->dig, dig) == 0) {
	res = RPMRC_OK;
    } else {
	rasprintf(msg, "Expected %s != %s", sinfo->dig, dig);
    }

exit:
    free(dig);
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

    return res;
}

static rpmRC
rpmVerifySignature(rpmKeyring keyring, struct rpmsinfo_s *sinfo,
		   DIGEST_CTX ctx, char ** result)
{
    rpmRC res = RPMRC_FAIL;

    if (sinfo->type == RPMSIG_DIGEST_TYPE)
	res = verifyDigest(sinfo, ctx, result);
    else if (sinfo->type == RPMSIG_SIGNATURE_TYPE)
	res = verifySignature(keyring, sinfo, ctx, result);

    return res;
}
