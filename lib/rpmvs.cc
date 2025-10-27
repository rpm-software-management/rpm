#include "system.h"

#include <rpm/rpmbase64.h>
#include <rpm/rpmkeyring.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmlog.h>
#include "rpmvs.hh"
#include "rpmpgpval.hh"

#include "debug.h"

struct rpmvs_s {
    struct rpmsinfo_s *sigs;
    int nsigs;
    int nalloced;
    rpmVSFlags vsflags;
    rpmDigestBundle bundle;
    rpmKeyring keyring;
    int vfylevel;
};

struct vfytag_s {
    rpmTagVal tag;
    rpmTagType tagtype;
    rpm_count_t tagcount;
    rpm_count_t tagsize;
};

static const struct vfytag_s rpmvfytags[] = {
    {	RPMTAG_OPENPGP,			RPM_STRING_ARRAY_TYPE,	0,	0, },
    {	RPMSIGTAG_SIZE,			RPM_BIN_TYPE,		0,	0, },
    {	RPMSIGTAG_PGP,			RPM_BIN_TYPE,		0,	0, },
    {	RPMSIGTAG_MD5,			RPM_BIN_TYPE,		0,	16, },
    {	RPMSIGTAG_GPG,			RPM_BIN_TYPE,		0,	0, },
    {	RPMSIGTAG_PAYLOADSIZE,		RPM_INT32_TYPE,		1,	4, },
    {	RPMSIGTAG_RESERVEDSPACE,	RPM_BIN_TYPE,		0,	0, },
    {	RPMTAG_DSAHEADER,		RPM_BIN_TYPE,		0,	0, },
    {	RPMTAG_RSAHEADER,		RPM_BIN_TYPE,		0,	0, },
    {	RPMTAG_SHA1HEADER,		RPM_STRING_TYPE,	1,	41, },
    {	RPMSIGTAG_LONGSIZE,		RPM_INT64_TYPE,		1,	8, },
    {	RPMSIGTAG_LONGARCHIVESIZE,	RPM_INT64_TYPE,		1,	8, },
    {	RPMTAG_SHA256HEADER,		RPM_STRING_TYPE,	1,	65, },
    {	RPMTAG_SHA3_256HEADER,		RPM_STRING_TYPE,	1,	65, },
    {	RPMTAG_PAYLOADSHA256,		RPM_STRING_ARRAY_TYPE,	0,	0, },
    {	RPMTAG_PAYLOADSHA256ALT,	RPM_STRING_ARRAY_TYPE,	0,	0, },
    {	RPMTAG_PAYLOADSHA512,		RPM_STRING_TYPE,	0,	0, },
    {	RPMTAG_PAYLOADSHA512ALT,	RPM_STRING_TYPE,	0,	0, },
    {	RPMTAG_PAYLOADSHA3_256,		RPM_STRING_TYPE,	0,	0, },
    {	RPMTAG_PAYLOADSHA3_256ALT,	RPM_STRING_TYPE,	0,	0, },
    { 0 } /* sentinel */
};

struct vfyinfo_s {
    rpmTagVal tag;
    int sigh;
    struct rpmsinfo_s vi;
};

static const struct vfyinfo_s rpmvfyitems[] = {
    {	RPMTAG_OPENPGP,			1,
	{ RPMSIG_SIGNATURE_TYPE,	RPMVSF_NOOPENPGP,
	(RPMSIG_HEADER),		0, 0, }, },
    {	RPMSIGTAG_SIZE,			1,
	{ RPMSIG_OTHER_TYPE,		0,
	(RPMSIG_HEADER|RPMSIG_PAYLOAD), 0, 0, }, },
    {	RPMSIGTAG_PGP,			1,
	{ RPMSIG_SIGNATURE_TYPE,		RPMVSF_NORSA,
	(RPMSIG_HEADER|RPMSIG_PAYLOAD), 0, PGPPUBKEYALGO_RSA, }, },
    {	RPMSIGTAG_MD5,			1,
	{ RPMSIG_DIGEST_TYPE,		RPMVSF_NOMD5,
	(RPMSIG_HEADER|RPMSIG_PAYLOAD), RPM_HASH_MD5, }, },
    {	RPMSIGTAG_GPG,			1,
	{ RPMSIG_SIGNATURE_TYPE,		RPMVSF_NODSA,
	(RPMSIG_HEADER|RPMSIG_PAYLOAD), 0, PGPPUBKEYALGO_DSA, }, },
    {	RPMSIGTAG_PAYLOADSIZE,		1,
	{ RPMSIG_OTHER_TYPE,		0,
	(RPMSIG_PAYLOAD),		0, 0, }, },
    {	RPMSIGTAG_RESERVEDSPACE,	1,
	{ RPMSIG_OTHER_TYPE,		0,
	0,				0, 0, }, },
    {	RPMTAG_DSAHEADER,		1,
	{ RPMSIG_SIGNATURE_TYPE,		RPMVSF_NODSAHEADER,
	(RPMSIG_HEADER),		0, PGPPUBKEYALGO_DSA, }, },
    {	RPMTAG_RSAHEADER,		1,
	{ RPMSIG_SIGNATURE_TYPE,		RPMVSF_NORSAHEADER,
	(RPMSIG_HEADER),		0, PGPPUBKEYALGO_RSA, }, },
    {	RPMTAG_SHA1HEADER,		1,
	{ RPMSIG_DIGEST_TYPE,		RPMVSF_NOSHA1HEADER,
	(RPMSIG_HEADER),		RPM_HASH_SHA1, 0, }, },
    {	RPMSIGTAG_LONGSIZE,		1,
	{ RPMSIG_OTHER_TYPE, 		0,
	(RPMSIG_HEADER|RPMSIG_PAYLOAD), 0, 0, }, },
    {	RPMSIGTAG_LONGARCHIVESIZE,	1,
	{ RPMSIG_OTHER_TYPE,		0,
	(RPMSIG_HEADER|RPMSIG_PAYLOAD),	0, 0, }, },
    {	RPMTAG_SHA256HEADER,		1,
	{ RPMSIG_DIGEST_TYPE,		RPMVSF_NOSHA256HEADER,
	(RPMSIG_HEADER),		RPM_HASH_SHA256, 0, }, },
    {	RPMTAG_SHA3_256HEADER,		1,
	{ RPMSIG_DIGEST_TYPE,		RPMVSF_NOSHA3_256HEADER,
	(RPMSIG_HEADER),		RPM_HASH_SHA3_256, 0, }, },
    {	RPMTAG_PAYLOADSHA256,		0,
	{ RPMSIG_DIGEST_TYPE,		RPMVSF_NOSHA256PAYLOAD,
	(RPMSIG_PAYLOAD),		RPM_HASH_SHA256, 0, }, },
    {	RPMTAG_PAYLOADSHA256ALT,	0,
	{ RPMSIG_DIGEST_TYPE,		RPMVSF_NOSHA256PAYLOAD,
	(RPMSIG_PAYLOAD),		RPM_HASH_SHA256, 0, 1, }, },
    {	RPMTAG_PAYLOADSHA512,		0,
	{ RPMSIG_DIGEST_TYPE,		RPMVSF_NOSHA512PAYLOAD,
	(RPMSIG_PAYLOAD),		RPM_HASH_SHA512, 0, }, },
    {	RPMTAG_PAYLOADSHA512ALT,	0,
	{ RPMSIG_DIGEST_TYPE,		RPMVSF_NOSHA512PAYLOAD,
	(RPMSIG_PAYLOAD),		RPM_HASH_SHA512, 0, 1, }, },
    {	RPMTAG_PAYLOADSHA3_256,		0,
	{ RPMSIG_DIGEST_TYPE,		RPMVSF_NOSHA3_256PAYLOAD,
	(RPMSIG_PAYLOAD),		RPM_HASH_SHA3_256, 0, }, },
    {	RPMTAG_PAYLOADSHA3_256ALT,	0,
	{ RPMSIG_DIGEST_TYPE,		RPMVSF_NOSHA3_256PAYLOAD,
	(RPMSIG_PAYLOAD),		RPM_HASH_SHA3_256, 0, 1, }, },
    { 0 } /* sentinel */
};

static const char *rangeName(int range);
static const char * rpmSigString(rpmRC res);
static void rpmVerifySignature(rpmKeyring keyring, struct rpmsinfo_s *sinfo);

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

int rpmIsValidHex(const char *str, size_t slen)
{
    int valid = 0; /* Assume invalid */
    const char *b;

    /* Our hex data is always even sized */
    if (slen % 2)
	goto exit;

    for (b = str ; *b != '\0'; b++) {
	if (strchr("0123456789abcdefABCDEF", *b) == NULL)
	    goto exit;
    }
    valid = 1;

exit:
    return valid;
}

static rpmRC rpmsinfoInit(const struct vfyinfo_s *vinfo,
			  const struct vfytag_s *tinfo,
			  rpmtd td,  const char *origin,
			  struct rpmsinfo_s *sinfo)
{
    rpmRC rc = RPMRC_FAIL;
    const void *data = NULL;
    rpm_count_t dlen = 0;
    uint8_t *pkt = NULL;
    size_t pktlen = 0;

    *sinfo = vinfo->vi; /* struct assignment */
    sinfo->wrapped = (vinfo->sigh == 0);
    sinfo->strength = sinfo->type;
    sinfo->key = NULL;

    if (td == NULL) {
	rc = RPMRC_NOTFOUND;
	goto exit;
    }

    if (tinfo->tagtype && tinfo->tagtype != td->type) {
	rasprintf(&sinfo->msg, _("%s tag %u: invalid type %u"),
			origin, td->tag, td->type);
	goto exit;
    }

    if (tinfo->tagcount && tinfo->tagcount != td->count) {
	rasprintf(&sinfo->msg, _("%s: tag %u: invalid count %u"),
			origin, td->tag, td->count);
	goto exit;
    }

    switch (td->type) {
    case RPM_STRING_TYPE:
    case RPM_STRING_ARRAY_TYPE:
	data = rpmtdGetString(td);
	if (data)
	    dlen = strlen((const char *)data);
	break;
    case RPM_BIN_TYPE:
	data = td->data;
	dlen = td->count;
	break;
    }

    /* MD5 has data length of 16, everything else is (much) larger */
    if (sinfo->hashalgo && (data == NULL || dlen < 16)) {
	rasprintf(&sinfo->msg, _("%s tag %u: invalid data %p (%u)"),
			origin, td->tag, data, dlen);
	goto exit;
    }

    if (td->type == RPM_STRING_TYPE && td->size == 0)
	td->size = dlen + 1;

    if (tinfo->tagsize && (td->flags & RPMTD_IMMUTABLE) &&
		tinfo->tagsize != td->size) {
	rasprintf(&sinfo->msg, _("%s tag %u: invalid size %u"),
			origin, td->tag, td->size);
	goto exit;
    }

    if (sinfo->type == RPMSIG_SIGNATURE_TYPE) {
	if (td->type == RPM_STRING_ARRAY_TYPE) {
	    if (rpmBase64Decode((const char *)data, (void **)&pkt, &pktlen)) {
		rasprintf(&sinfo->msg, _("%s tag %u: invalid base64"),
			origin, td->tag);
		goto exit;
	    }
	    data = pkt;
	    dlen = pktlen;
	}

	char *lints = NULL;
        int ec = pgpPrtParams2((const uint8_t *)data, dlen, PGPTAG_SIGNATURE,
				&sinfo->sig, &lints);
	if (ec) {
	    if (lints) {
		rasprintf(&sinfo->msg,
			("%s tag %u: invalid OpenPGP signature: %s"),
			origin, td->tag, lints);
		free(lints);
	    } else {
		rasprintf(&sinfo->msg,
			_("%s tag %u: invalid OpenPGP signature"),
			origin, td->tag);
	    }
	    goto exit;
	} else if (lints) {
	    rpmlog(RPMLOG_WARNING, "%s\n", lints);
	    free(lints);
	}
	sinfo->hashalgo = pgpDigParamsAlgo(sinfo->sig, PGPVAL_HASHALGO);
	sinfo->keyid = rpmhex(pgpDigParamsSignID(sinfo->sig), PGP_KEYID_LEN);
    } else if (sinfo->type == RPMSIG_DIGEST_TYPE) {
	if (td->type == RPM_BIN_TYPE) {
	    sinfo->dig = rpmhex((const uint8_t *)data, dlen);
	} else {
	    /* Our hex data is always at least sha-1 long */
	    if (dlen < 40 || !rpmIsValidHex((const char *)data, dlen)) {
		rasprintf(&sinfo->msg,
			_("%s: tag %u: invalid hex"), origin, td->tag);
		goto exit;
	    }
	    sinfo->dig = xstrdup((const char *)data);
	}
    }

    if (sinfo->hashalgo)
	sinfo->id = (td->tag << 16) | rpmtdGetIndex(td);

    rc = RPMRC_OK;

exit:
    if (pkt && pkt != td->data)
	free(pkt);
    sinfo->rc = rc;
    return rc;
}

static void rpmsinfoFini(struct rpmsinfo_s *sinfo)
{
    if (sinfo) {
	if (sinfo->type == RPMSIG_SIGNATURE_TYPE)
	    pgpDigParamsFree(sinfo->sig);
	else if (sinfo->type == RPMSIG_DIGEST_TYPE)
	    free(sinfo->dig);
	rpmDigestFinal(sinfo->ctx, NULL, NULL, 0);
	rpmPubkeyFree(sinfo->key);
	free(sinfo->msg);
	free(sinfo->descr);
	free(sinfo->keyid);
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
	vs->sigs = xrealloc(vs->sigs, vs->nalloced * sizeof(*vs->sigs));
    }
}

const char *rpmsinfoDescr(struct rpmsinfo_s *sinfo)
{
    if (sinfo->descr == NULL) {
	switch (sinfo->type) {
	case RPMSIG_DIGEST_TYPE:
	    rasprintf(&sinfo->descr, _("%s %s%s %s"),
		    rangeName(sinfo->range),
		    pgpValString(PGPVAL_HASHALGO, sinfo->hashalgo),
		    sinfo->alt ? " ALT" : "",
		    _("digest"));
	    break;
	case RPMSIG_SIGNATURE_TYPE:
	    if (sinfo->sig) {
		char *t = pgpIdentItem(sinfo->sig);
		rasprintf(&sinfo->descr, _("%s OpenPGP %s"),
			rangeName(sinfo->range), t);
		free(t);
	    } else {
		if (sinfo->sigalgo) {
		    rasprintf(&sinfo->descr, _("%s OpenPGP %s %s"),
			    rangeName(sinfo->range),
			    pgpValString(PGPVAL_PUBKEYALGO, sinfo->sigalgo),
			    _("signature"));
		} else {
		    rasprintf(&sinfo->descr, _("%s OpenPGP %s"),
			    rangeName(sinfo->range),
			    _("signature"));
		}
	    }
	    break;
	}
    }
    return sinfo->descr;
}

char *rpmsinfoMsg(struct rpmsinfo_s *sinfo)
{
    char *msg = NULL;
    const char *fphex = NULL;
    char *fpmsg = NULL;
    char * descr = xstrdup(rpmsinfoDescr(sinfo));
    if (sinfo->key) {
	fphex = rpmPubkeyFingerprintAsHex(sinfo->key);
    }
    if (fphex) {
	rasprintf(&fpmsg, _(", key fingerprint: %s"), fphex);
	char * pos = strstr(descr, ", key ID");
	if (pos)
	    *pos = '\0';
    } else {
	rstrcat(&fpmsg, "");
    }

    if (sinfo->msg) {
	rasprintf(&msg, "%s%s: %s (%s)",
		  descr, fpmsg, rpmSigString(sinfo->rc), sinfo->msg);
    } else {
	rasprintf(&msg, "%s%s: %s",
		  descr, fpmsg, rpmSigString(sinfo->rc));
    }
    free(descr);
    free(fpmsg);
    return msg;
}

static void rpmvsAppend(struct rpmvs_s *sis, hdrblob blob,
			const struct vfyinfo_s *vi, const struct vfytag_s *ti)
{
    if (!(vi->vi.type & RPMSIG_VERIFIABLE_TYPE))
	return;

    const char *o = (blob->il > blob->ril) ? _("header") : _("package");
    struct rpmtd_s td;
    rpmRC rc = hdrblobGet(blob, vi->tag, &td);
    int nitems = (rc == RPMRC_OK) ? rpmtdCount(&td) : 1;

    rpmvsReserve(sis, nitems);

    if (!rpmsinfoDisabled(&vi->vi, sis->vsflags) && rc == RPMRC_OK) {
	while (rpmtdNext(&td) >= 0) {
	    if (!rpmsinfoInit(vi, ti, &td, o, &sis->sigs[sis->nsigs])) {
		/* Don't bother with v3/v4 sigs when v6 sigs exist */
		if (td.tag == RPMSIGTAG_OPENPGP) {
		    sis->vsflags |= (RPMVSF_NODSAHEADER|RPMVSF_NORSAHEADER);
		    sis->vsflags |= (RPMVSF_NODSA|RPMVSF_NORSA);
		}
	    }
	    sis->nsigs++;
	}
    } else {
	rpmsinfoInit(vi, ti, NULL, o, &sis->sigs[sis->nsigs]);
	sis->nsigs++;
    }
    rpmtdFreeData(&td);
}

void rpmvsAppendTag(struct rpmvs_s *vs, hdrblob blob, rpmTagVal tag)
{
    int ix = sinfoLookup(tag);
    if (ix >= 0) {
	const struct vfyinfo_s *vi = &rpmvfyitems[ix];
	const struct vfytag_s *ti = &rpmvfytags[ix];
	rpmvsAppend(vs, blob, vi, ti);
    }
}

struct rpmvs_s *rpmvsCreate(int vfylevel, rpmVSFlags vsflags, rpmKeyring keyring)
{
    struct rpmvs_s *sis = new rpmvs_s {};
    sis->vsflags = vsflags;
    sis->keyring = rpmKeyringLink(keyring);
    sis->vfylevel = vfylevel;

    return sis;
}

rpmVSFlags rpmvsFlags(struct rpmvs_s *vs)
{
    return vs->vsflags;
}

void rpmvsInit(struct rpmvs_s *vs, hdrblob blob, rpmDigestBundle bundle)
{
    const struct vfyinfo_s *si = &rpmvfyitems[0];
    const struct vfytag_s *ti = &rpmvfytags[0];
    int ignore_legacy = 0;

    /* Heuristics for rpm v6 and newer */
    if (hdrblobIsEntry(blob, RPMSIGTAG_RESERVED) &&
	hdrblobIsEntry(blob, RPMSIGTAG_SHA3_256))
    {
	ignore_legacy = 1;
    }

    for (; si->tag && ti->tag; si++, ti++) {
	/* Ignore non-signature tags initially */
	if (!si->sigh)
	    continue;
	/* Ignore legacy tags in the conflicting range? */
	if (ignore_legacy && ti->tag >= HEADER_TAGBASE)
	    continue;
	rpmvsAppend(vs, blob, si, ti);
    }
    vs->bundle = bundle;
}

struct rpmvs_s *rpmvsFree(struct rpmvs_s *sis)
{
    if (sis) {
	rpmKeyringFree(sis->keyring);
	for (int i = 0; i < sis->nsigs; i++) {
	    rpmsinfoFini(&sis->sigs[i]);
	}
	free(sis->sigs);
	delete sis;
    }
    return NULL;
}

void rpmvsInitRange(struct rpmvs_s *sis, int range)
{
    for (int i = 0; i < sis->nsigs; i++) {
	struct rpmsinfo_s *sinfo = &sis->sigs[i];
	if (sinfo->range & range) {
	    if (sinfo->rc != RPMRC_OK)
		continue;

	    rpmDigestBundleAddID(sis->bundle, sinfo->hashalgo, sinfo->id, 0);
	    /* OpenPGP v6 signatures need a grain of salt to go */
	    if (sinfo->type == RPMSIG_SIGNATURE_TYPE && sinfo->sig) {
		const uint8_t *salt = NULL;
		size_t slen = 0;
		if (pgpDigParamsSalt(sinfo->sig, &salt, &slen) == 0 && salt) {
		    rpmDigestBundleUpdateID(sis->bundle, sinfo->id, salt, slen);
		}
	    }
	}
    }
}

void rpmvsFiniRange(struct rpmvs_s *sis, int range)
{
    for (int i = 0; i < sis->nsigs; i++) {
	struct rpmsinfo_s *sinfo = &sis->sigs[i];

	if (sinfo->range == range && sinfo->rc == RPMRC_OK) {
	    sinfo->ctx = rpmDigestBundleDupCtx(sis->bundle, sinfo->id);
	    /* Handle unsupported digests the same as disabled ones */
	    if (sinfo->ctx == NULL)
		sinfo->rc = RPMRC_NOTFOUND;
	    rpmDigestBundleFinal(sis->bundle, sinfo->id, NULL, NULL, 0);
	}
    }
}

int rpmvsRange(struct rpmvs_s *vs)
{
    int range = 0;
    for (int i = 0; i < vs->nsigs; i++) {
	if (rpmsinfoDisabled(&vs->sigs[i], vs->vsflags))
	    continue;
	range |= vs->sigs[i].range;
    }

    return range;
}

static int sinfoCmp(const void *a, const void *b)
{
    const struct rpmsinfo_s *sa = (const struct rpmsinfo_s *)a;
    const struct rpmsinfo_s *sb = (const struct rpmsinfo_s *)b;
    int rc = sa->range - sb->range;
    /* signatures before digests */
    if (rc == 0)
	rc = sb->type - sa->type;
    /* strongest (in the "newer is better" sense) algos first */
    if (rc == 0)
	rc = sb->sigalgo - sb->sigalgo;
    if (rc == 0)
	rc = sb->hashalgo - sb->hashalgo;
    /* last resort, these only makes sense from consistency POV */
    if (rc == 0)
	rc = sb->id - sa->id;
    if (rc == 0)
	rc = sb->disabler - sa->disabler;
    return rc;
}


static const struct rpmsinfo_s *getAlt(const struct rpmvs_s *vs, const struct rpmsinfo_s *sinfo)
{
    for (int i = 0; i < vs->nsigs; i++) {
	const struct rpmsinfo_s *alt = &vs->sigs[i];
	if ((sinfo->id != alt->id) && (sinfo->disabler == alt->disabler))
	    return alt;
    }
    return NULL;
}

int rpmvsVerify(struct rpmvs_s *sis, int type,
		       rpmsinfoCb cb, void *cbdata)
{
    int success = 0;
    int failed = 0;
    int cont = 1;
    int range = 0, vfylevel = sis->vfylevel;
    int verified[3] = { 0, 0, 0 };

    /* sort for consistency and rough "better comes first" semantics*/
    qsort(sis->sigs, sis->nsigs, sizeof(*sis->sigs), sinfoCmp);

    for (int i = 0; i < sis->nsigs && cont; i++) {
	struct rpmsinfo_s *sinfo = &sis->sigs[i];

	if (type & sinfo->type) {
	    /* Digests in signed header are signature strength */
	    if (sinfo->wrapped) {
		if (verified[RPMSIG_SIGNATURE_TYPE] & RPMSIG_HEADER)
		    sinfo->strength = RPMSIG_SIGNATURE_TYPE;
	    }

	    if (sinfo->ctx) {
		rpmVerifySignature(sis->keyring, sinfo);
		if (sinfo->rc == RPMRC_OK) {
		    verified[sinfo->type] |= sinfo->range;
		    verified[sinfo->strength] |= sinfo->range;
		}
	    }
	    range |= sinfo->range;
	}
    }

    /* Unconditionally reject partially signed packages */
    if (verified[RPMSIG_SIGNATURE_TYPE])
	vfylevel |= RPMSIG_SIGNATURE_TYPE;

    /* Cannot verify payload if RPMVSF_NEEDPAYLOAD is set */
    if (sis->vsflags & RPMVSF_NEEDPAYLOAD)
	range &= ~RPMSIG_PAYLOAD;

    for (int i = 0; i < sis->nsigs && cont; i++) {
	struct rpmsinfo_s *sinfo = &sis->sigs[i];
	int strength = (sinfo->type | sinfo->strength);
	int required = 0;

	/* Ignore a digest failure if an alternative exists and verifies ok */
	if (sinfo->type == RPMSIG_DIGEST_TYPE && sinfo->rc == RPMRC_FAIL) {
	    const struct rpmsinfo_s * alt = getAlt(sis, sinfo);
	    if (alt && alt->rc == RPMRC_OK)
		sinfo->rc = RPMRC_NOTFOUND;
	}

	if (vfylevel & strength & RPMSIG_DIGEST_TYPE) {
	    int missing = (range & ~verified[RPMSIG_DIGEST_TYPE]);
	    required |= (missing & sinfo->range);
	}
	if (vfylevel & strength & RPMSIG_SIGNATURE_TYPE) {
	    int missing = (range & ~verified[RPMSIG_SIGNATURE_TYPE]);
	    required |= (missing & sinfo->range);
	}

	if (!required && sinfo->rc == RPMRC_NOTFOUND)
	    continue;

	if (cb)
	    cont = cb(sinfo, cbdata);

	switch (sinfo->rc) {
	case RPMRC_OK:
	    success++;
	    break;
	case RPMRC_FAIL:
	case RPMRC_NOKEY:
	case RPMRC_NOTFOUND:
	    failed++;
	    break;
	case RPMRC_NOTTRUSTED:
	default:
	    /* ignore */
	    break;
	};
    }

    return (success >= 1 && failed == 0) ? 0 : failed;
}

static const char * rpmSigString(rpmRC res)
{
    const char * str;
    switch (res) {
    case RPMRC_OK:		str = "OK";		break;
    case RPMRC_FAIL:		str = "BAD";		break;
    case RPMRC_NOKEY:		str = "NOKEY";		break;
    case RPMRC_NOTTRUSTED:	str = "NOTTRUSTED";	break;
    case RPMRC_NOTFOUND:	str = "NOTFOUND";	break;
    default:			str = "UNKNOWN";	break;
    }
    return str;
}

static const char *rangeName(int range)
{
    switch (range) {
    case RPMSIG_HEADER:				return _("Header");
    case RPMSIG_PAYLOAD:			return _("Payload");
    }
    /* (RPMSIG_HEADER|RPMSIG_PAYLOAD) */
    return "Legacy";
}

static rpmRC verifyDigest(struct rpmsinfo_s *sinfo)
{
    rpmRC res = RPMRC_FAIL; /* assume failure */
    char * dig = NULL;
    size_t diglen = 0;
    DIGEST_CTX ctx = rpmDigestDup(sinfo->ctx);

    if (rpmDigestFinal(ctx, (void **)&dig, &diglen, 1) || diglen == 0)
	goto exit;

    if (strcasecmp(sinfo->dig, dig) == 0) {
	res = RPMRC_OK;
    } else {
	rasprintf(&sinfo->msg, "Expected %s != %s", sinfo->dig, dig);
    }

exit:
    free(dig);
    return res;
}

/**
 * Verify DSA/RSA signature.
 * @param keyring	pubkey keyring
 * @param sinfo		OpenPGP signature parameters
 * @return 		RPMRC_OK on success
 */
static rpmRC
verifySignature(rpmKeyring keyring, struct rpmsinfo_s *sinfo)
{
    rpmRC res = RPMRC_FAIL;
    if (pgpSignatureType(sinfo->sig) == PGPSIGTYPE_BINARY) {
	res = rpmKeyringVerifySig2(keyring, sinfo->sig, sinfo->ctx, &sinfo->key);
    }
    return res;
}

static void
rpmVerifySignature(rpmKeyring keyring, struct rpmsinfo_s *sinfo)
{
    if (sinfo->type == RPMSIG_DIGEST_TYPE)
	sinfo->rc = verifyDigest(sinfo);
    else if (sinfo->type == RPMSIG_SIGNATURE_TYPE)
	sinfo->rc = verifySignature(keyring, sinfo);
    else
	sinfo->rc = RPMRC_FAIL;
}
