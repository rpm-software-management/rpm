/** \ingroup rpmio signature
 * \file rpmio/rpmpgp_internal.c
 * Routines to handle RFC-2440 detached signatures.
 */

#include "system.h"

#include <time.h>
#include <netinet/in.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmbase64.h>

#include "rpmpgpval.h"
#include "rpmpgp_internal.h"

#include "debug.h"

static int _print = 0;

typedef uint8_t pgpTime_t[4];

typedef struct pgpPktKeyV3_s {
    uint8_t version;	/*!< version number (3). */
    pgpTime_t time;	/*!< time that the key was created. */
    uint8_t valid[2];	/*!< time in days that this key is valid. */
    uint8_t pubkey_algo;	/*!< public key algorithm. */
} * pgpPktKeyV3;

typedef struct pgpPktKeyV4_s {
    uint8_t version;	/*!< version number (4). */
    pgpTime_t time;	/*!< time that the key was created. */
    uint8_t pubkey_algo;	/*!< public key algorithm. */
} * pgpPktKeyV4;

typedef struct pgpPktSigV3_s {
    uint8_t version;	/*!< version number (3). */
    uint8_t hashlen;	/*!< length of following hashed material. MUST be 5. */
    uint8_t sigtype;	/*!< signature type. */
    pgpTime_t time;	/*!< 4 byte creation time. */
    pgpKeyID_t signid;	/*!< key ID of signer. */
    uint8_t pubkey_algo;	/*!< public key algorithm. */
    uint8_t hash_algo;	/*!< hash algorithm. */
    uint8_t signhash16[2];	/*!< left 16 bits of signed hash value. */
} * pgpPktSigV3;

typedef struct pgpPktSigV4_s {
    uint8_t version;	/*!< version number (4). */
    uint8_t sigtype;	/*!< signature type. */
    uint8_t pubkey_algo;	/*!< public key algorithm. */
    uint8_t hash_algo;	/*!< hash algorithm. */
    uint8_t hashlen[2];	/*!< length of following hashed material. */
} * pgpPktSigV4;

/** \ingroup rpmio
 * Values parsed from OpenPGP signature/pubkey packet(s).
 */
struct pgpDigParams_s {
    char * userid;
    uint8_t * hash;
    uint8_t tag;

    uint8_t key_flags;		/*!< key usage flags */
    uint8_t version;		/*!< version number. */
    uint32_t time;		/*!< key/signature creation time. */
    uint8_t pubkey_algo;		/*!< public key algorithm. */

    uint8_t hash_algo;
    uint8_t sigtype;
    uint32_t hashlen;
    uint8_t signhash16[2];
    pgpKeyID_t signid;
    uint8_t saved;		/*!< Various flags.  `PGPDIG_SAVED_*` are never reset.
				 * `PGPDIG_SIG_HAS_*` are reset for each signature. */
#define	PGPDIG_SAVED_TIME	(1 << 0)
#define	PGPDIG_SAVED_ID		(1 << 1)
#define	PGPDIG_SIG_HAS_CREATION_TIME	(1 << 2)
#define	PGPDIG_SIG_HAS_KEY_FLAGS	(1 << 3)

    pgpDigAlg alg;
};

static void pgpPrtNL(void)
{
    if (!_print) return;
    fprintf(stderr, "\n");
}

static void pgpPrtHex(const char *pre, const uint8_t *p, size_t plen)
{
    char *hex = NULL;
    if (!_print) return;
    if (pre && *pre)
	fprintf(stderr, "%s", pre);
    hex = rpmhex(p, plen);
    fprintf(stderr, " %s", hex);
    free(hex);
}

static void pgpPrtVal(const char * pre, pgpValType vt, uint8_t val)
{
    if (!_print) return;
    if (pre && *pre)
	fprintf(stderr, "%s", pre);
    fprintf(stderr, "%s(%u)", pgpValString(vt, val), (unsigned)val);
}

static void pgpPrtTime(const char * pre, const uint8_t *p, size_t plen)
{
    if (!_print) return;
    if (pre && *pre)
	fprintf(stderr, "%s", pre);
    if (plen == 4) {
	char buf[1024];
	time_t t = pgpGrab(p, plen);
	struct tm _tm, *tms = localtime_r(&t, &_tm);
	if (strftime(buf, sizeof(buf), "%c", tms) > 0)
	    fprintf(stderr, " %-24.24s(0x%08x)", buf, (unsigned)t);
    } else {
	pgpPrtHex("", p+1, plen-1);
    }
}

/** \ingroup rpmpgp
 * Return value of an OpenPGP string.
 * @param vs		table of (string,value) pairs
 * @param s		string token to lookup
 * @param se		end-of-string address
 * @return		byte value
 */
static inline
int pgpValTok(pgpValTbl vs, const char * s, const char * se)
{
    do {
	size_t vlen = strlen(vs->str);
	if (vlen <= (se-s) && rstreqn(s, vs->str, vlen))
	    break;
    } while ((++vs)->val != -1);
    return vs->val;
}

/** \ingroup rpmpgp
 * Decode length from 1, 2, or 5 octet body length encoding, used in
 * new format packet headers and V4 signature subpackets.
 * Partial body lengths are (intentionally) not supported.
 * @param s		pointer to length encoding buffer
 * @param slen		buffer size
 * @param[out] *lenp	decoded length
 * @return		no. of bytes used to encode the length, 0 on error
 */
static inline
size_t pgpLen(const uint8_t *s, size_t slen, size_t * lenp)
{
    size_t dlen = 0;
    size_t lenlen = 0;

    /*
     * Callers can only ensure we'll always have the first byte, beyond
     * that the required size is not known until we decode it so we need
     * to check if we have enough bytes to read the size as we go.
     */
    if (*s < 192) {
	lenlen = 1;
	dlen = *s;
    } else if (*s < 224 && slen > 2) {
	lenlen = 2;
	dlen = (((s[0]) - 192) << 8) + s[1] + 192;
    } else if (*s == 255 && slen > 5) {
	lenlen = 5;
	dlen = pgpGrab(s+1, 4);
    }

    if (slen - lenlen < dlen)
	lenlen = 0;

    if (lenlen)
	*lenp = dlen;

    return lenlen;
}

struct pgpPkt {
    uint8_t tag;		/* decoded PGP tag */
    const uint8_t *head;	/* pointer to start of packet (header) */
    const uint8_t *body;	/* pointer to packet body */
    size_t blen;		/* length of body in bytes */
};

/** \ingroup rpmpgp
 * Read a length field `nbytes` long.  Checks that the buffer is big enough to
 * hold `nbytes + *valp` bytes.
 * @param s		pointer to read from
 * @param nbytes	length of length field
 * @param send		pointer past end of buffer
 * @param[out] *valp	decoded length
 * @return		0 if buffer can hold `nbytes + *valp` of data,
 * 			otherwise -1.
 */
static int pgpGet(const uint8_t *s, size_t nbytes, const uint8_t *send,
		  size_t *valp)
{
    int rc = -1;

    *valp = 0;
    if (nbytes <= 4 && send - s >= nbytes) {
	unsigned int val = pgpGrab(s, nbytes);
	if (send - s - nbytes >= val) {
	    rc = 0;
	    *valp = val;
	}
    }

    return rc;
}

static int decodePkt(const uint8_t *p, size_t plen, struct pgpPkt *pkt)
{
    int rc = -1; /* assume failure */

    /* Valid PGP packet header must always have two or more bytes in it */
    if (p && plen >= 2 && p[0] & 0x80) {
	size_t lenlen = 0;
	size_t hlen = 0;

	if (p[0] & 0x40) {
	    /* New format packet, body length encoding in second byte */
	    lenlen = pgpLen(p+1, plen-1, &pkt->blen);
	    pkt->tag = (p[0] & 0x3f);
	} else {
	    /* Old format packet, body length encoding in tag byte */
	    lenlen = (1 << (p[0] & 0x3));
	    /* Reject indefinite length packets and check bounds */
	    if (pgpGet(p + 1, lenlen, p + plen, &pkt->blen))
		return rc;
	    pkt->tag = (p[0] >> 2) & 0xf;
	}
	hlen = lenlen + 1;

	/* Does the packet header and its body fit in our boundaries? */
	if (lenlen && (hlen + pkt->blen <= plen)) {
	    pkt->head = p;
	    pkt->body = pkt->head + hlen;
	    rc = 0;
	}
    }

    return rc;
}

#define CRC24_INIT	0xb704ce
#define CRC24_POLY	0x1864cfb

/** \ingroup rpmpgp
 * Return CRC of a buffer.
 * @param octets	bytes
 * @param len		no. of bytes
 * @return		crc of buffer
 */
static inline
unsigned int pgpCRC(const uint8_t *octets, size_t len)
{
    unsigned int crc = CRC24_INIT;
    size_t i;

    while (len--) {
	crc ^= (*octets++) << 16;
	for (i = 0; i < 8; i++) {
	    crc <<= 1;
	    if (crc & 0x1000000)
		crc ^= CRC24_POLY;
	}
    }
    return crc & 0xffffff;
}

static int pgpVersion(const uint8_t *h, size_t hlen, uint8_t *version)
{
    if (hlen < 1)
	return -1;

    *version = h[0];
    return 0;
}

int pgpSignatureType(pgpDigParams _digp)
{
    int rc = -1;

    if (_digp && _digp->tag == PGPTAG_SIGNATURE)
	rc = _digp->sigtype;

    return rc;
}

static int pgpPrtSubType(const uint8_t *h, size_t hlen, pgpSigType sigtype, 
			 pgpDigParams _digp, int hashed)
{
    const uint8_t *p = h;
    size_t plen = 0, i;
    int rc = 0;

    while (hlen > 0 && rc == 0) {
	int impl = 0;
	i = pgpLen(p, hlen, &plen);
	if (i == 0 || plen < 1 || i + plen > hlen)
	    break;

	p += i;
	hlen -= i;

	pgpPrtVal("    ", PGPVAL_SUBTYPE, (p[0]&(~PGPSUBTYPE_CRITICAL)));
	if (p[0] & PGPSUBTYPE_CRITICAL)
	    if (_print)
		fprintf(stderr, " *CRITICAL*");
	switch (*p & ~PGPSUBTYPE_CRITICAL) {
	case PGPSUBTYPE_PREFER_SYMKEY:	/* preferred symmetric algorithms */
	    for (i = 1; i < plen; i++)
		pgpPrtVal(" ", PGPVAL_SYMKEYALGO, p[i]);
	    break;
	case PGPSUBTYPE_PREFER_HASH:	/* preferred hash algorithms */
	    for (i = 1; i < plen; i++)
		pgpPrtVal(" ", PGPVAL_HASHALGO, p[i]);
	    break;
	case PGPSUBTYPE_PREFER_COMPRESS:/* preferred compression algorithms */
	    for (i = 1; i < plen; i++)
		pgpPrtVal(" ", PGPVAL_COMPRESSALGO, p[i]);
	    break;
	case PGPSUBTYPE_KEYSERVER_PREFERS:/* key server preferences */
	    for (i = 1; i < plen; i++)
		pgpPrtVal(" ", PGPVAL_SERVERPREFS, p[i]);
	    break;
	case PGPSUBTYPE_SIG_CREATE_TIME:  /* signature creation time */
	    if (!hashed)
		break; /* RFC 4880 §5.2.3.4 creation time MUST be hashed */
	    if (plen-1 != sizeof(_digp->time))
		break; /* other lengths not understood */
	    if (_digp->saved & PGPDIG_SIG_HAS_CREATION_TIME)
		return 1; /* duplicate timestamps not allowed */
	    impl = *p;
	    if (!(_digp->saved & PGPDIG_SAVED_TIME))
		_digp->time = pgpGrab(p+1, sizeof(_digp->time));
	    _digp->saved |= PGPDIG_SAVED_TIME | PGPDIG_SIG_HAS_CREATION_TIME;
	    break;
	case PGPSUBTYPE_SIG_EXPIRE_TIME:
	case PGPSUBTYPE_KEY_EXPIRE_TIME:
	    pgpPrtTime(" ", p+1, plen-1);
	    break;

	case PGPSUBTYPE_ISSUER_KEYID:	/* issuer key ID */
	    impl = *p;
	    if (!(_digp->saved & PGPDIG_SAVED_ID) &&
		(sigtype == PGPSIGTYPE_POSITIVE_CERT || sigtype == PGPSIGTYPE_BINARY || sigtype == PGPSIGTYPE_TEXT || sigtype == PGPSIGTYPE_STANDALONE))
	    {
		if (plen-1 != sizeof(_digp->signid))
		    break;
		_digp->saved |= PGPDIG_SAVED_ID;
		memcpy(_digp->signid, p+1, sizeof(_digp->signid));
	    }
	    break;
	case PGPSUBTYPE_KEY_FLAGS: /* Key usage flags */
	    /* Subpackets in the unhashed section cannot be trusted */
	    if (!hashed)
		break;
	    /* Reject duplicate key usage flags */
	    if (_digp->saved & PGPDIG_SIG_HAS_KEY_FLAGS)
		return 1;
	    impl = *p;
	    _digp->saved |= PGPDIG_SIG_HAS_KEY_FLAGS;
	    _digp->key_flags = plen >= 2 ? p[1] : 0;
	    break;
	case PGPSUBTYPE_EXPORTABLE_CERT:
	case PGPSUBTYPE_TRUST_SIG:
	case PGPSUBTYPE_REGEX:
	case PGPSUBTYPE_REVOCABLE:
	case PGPSUBTYPE_ARR:
	case PGPSUBTYPE_REVOKE_KEY:
	case PGPSUBTYPE_NOTATION:
	case PGPSUBTYPE_PREFER_KEYSERVER:
	case PGPSUBTYPE_PRIMARY_USERID:
	case PGPSUBTYPE_POLICY_URL:
	case PGPSUBTYPE_SIGNER_USERID:
	case PGPSUBTYPE_REVOKE_REASON:
	case PGPSUBTYPE_FEATURES:
	case PGPSUBTYPE_EMBEDDED_SIG:
	case PGPSUBTYPE_INTERNAL_100:
	case PGPSUBTYPE_INTERNAL_101:
	case PGPSUBTYPE_INTERNAL_102:
	case PGPSUBTYPE_INTERNAL_103:
	case PGPSUBTYPE_INTERNAL_104:
	case PGPSUBTYPE_INTERNAL_105:
	case PGPSUBTYPE_INTERNAL_106:
	case PGPSUBTYPE_INTERNAL_107:
	case PGPSUBTYPE_INTERNAL_108:
	case PGPSUBTYPE_INTERNAL_109:
	case PGPSUBTYPE_INTERNAL_110:
	default:
	    pgpPrtHex("", p+1, plen-1);
	    break;
	}
	pgpPrtNL();

	if (!impl && (p[0] & PGPSUBTYPE_CRITICAL))
	    rc = 1;

	p += plen;
	hlen -= plen;
    }

    if (hlen != 0)
	rc = 1;

    return rc;
}

pgpDigAlg pgpDigAlgFree(pgpDigAlg alg)
{
    if (alg) {
	if (alg->free)
	    alg->free(alg);
	free(alg);
    }
    return NULL;
}

static int processMpis(const int mpis, pgpDigAlg sigalg,
		       const uint8_t *p, const uint8_t *const pend)
{
    int i = 0, rc = 1; /* assume failure */
    for (; i < mpis && pend - p >= 2; i++) {
	unsigned int mpil = pgpMpiLen(p);
	if (pend - p < mpil)
	    return rc;
	if (sigalg && sigalg->setmpi(sigalg, i, p))
	    return rc;
	p += mpil;
    }

    /* Does the size and number of MPI's match our expectations? */
    if (p == pend && i == mpis)
	rc = 0;
    return rc;
}

static int pgpPrtSigParams(pgpTag tag, uint8_t pubkey_algo,
		const uint8_t *p, const uint8_t *h, size_t hlen,
		pgpDigParams sigp)
{
    const uint8_t * pend = h + hlen;
    pgpDigAlg sigalg = pgpSignatureNew(pubkey_algo);

    int rc = processMpis(sigalg->mpis, sigalg, p, pend);

    /* We can't handle more than one sig at a time */
    if (rc == 0 && sigp->alg == NULL && sigp->tag == PGPTAG_SIGNATURE)
	sigp->alg = sigalg;
    else
	pgpDigAlgFree(sigalg);

    return rc;
}

static int pgpPrtSig(pgpTag tag, const uint8_t *h, size_t hlen,
		     pgpDigParams _digp)
{
    uint8_t version = 0;
    const uint8_t * p;
    size_t plen;
    int rc = 1;

    /* Reset the saved flags */
    _digp->saved &= PGPDIG_SAVED_TIME | PGPDIG_SAVED_ID;
    _digp->key_flags = 0;

    if (pgpVersion(h, hlen, &version))
	return rc;

    switch (version) {
    case 3:
    {   pgpPktSigV3 v = (pgpPktSigV3)h;

	if (hlen <= sizeof(*v) || v->hashlen != 5)
	    return 1;

	pgpPrtVal("V3 ", PGPVAL_TAG, tag);
	pgpPrtVal(" ", PGPVAL_PUBKEYALGO, v->pubkey_algo);
	pgpPrtVal(" ", PGPVAL_HASHALGO, v->hash_algo);
	pgpPrtVal(" ", PGPVAL_SIGTYPE, v->sigtype);
	pgpPrtNL();
	pgpPrtTime(" ", v->time, sizeof(v->time));
	pgpPrtNL();
	pgpPrtHex(" signer keyid", v->signid, sizeof(v->signid));
	plen = pgpGrab(v->signhash16, sizeof(v->signhash16));
	pgpPrtHex(" signhash16", v->signhash16, sizeof(v->signhash16));
	pgpPrtNL();

	if (_digp->pubkey_algo == 0) {
	    _digp->version = v->version;
	    _digp->hashlen = v->hashlen;
	    _digp->sigtype = v->sigtype;
	    _digp->hash = memcpy(xmalloc(v->hashlen), &v->sigtype, v->hashlen);
	    if (!(_digp->saved & PGPDIG_SAVED_TIME))
		_digp->time = pgpGrab(v->time, sizeof(v->time));
	    if (!(_digp->saved & PGPDIG_SAVED_ID))
		memcpy(_digp->signid, v->signid, sizeof(_digp->signid));
	    _digp->saved = PGPDIG_SAVED_TIME | PGPDIG_SIG_HAS_CREATION_TIME | PGPDIG_SAVED_ID;
	    _digp->pubkey_algo = v->pubkey_algo;
	    _digp->hash_algo = v->hash_algo;
	    memcpy(_digp->signhash16, v->signhash16, sizeof(_digp->signhash16));
	}

	p = ((uint8_t *)v) + sizeof(*v);
	rc = tag ? pgpPrtSigParams(tag, v->pubkey_algo, p, h, hlen, _digp) : 0;
    }	break;
    case 4:
    {   pgpPktSigV4 v = (pgpPktSigV4)h;
	const uint8_t *const hend = h + hlen;

	if (hlen <= sizeof(*v))
	    return 1;

	pgpPrtVal("V4 ", PGPVAL_TAG, tag);
	pgpPrtVal(" ", PGPVAL_PUBKEYALGO, v->pubkey_algo);
	pgpPrtVal(" ", PGPVAL_HASHALGO, v->hash_algo);
	pgpPrtVal(" ", PGPVAL_SIGTYPE, v->sigtype);
	pgpPrtNL();

	p = &v->hashlen[0];
	if (pgpGet(v->hashlen, sizeof(v->hashlen), hend, &plen))
	    return 1;
	p += sizeof(v->hashlen);

	if ((p + plen) > hend)
	    return 1;

	if (_digp->pubkey_algo == 0) {
	    _digp->hashlen = sizeof(*v) + plen;
	    _digp->hash = memcpy(xmalloc(_digp->hashlen), v, _digp->hashlen);
	}
	if (pgpPrtSubType(p, plen, v->sigtype, _digp, 1))
	    return 1;
	p += plen;

	if (!(_digp->saved & PGPDIG_SIG_HAS_CREATION_TIME))
	    return 1; /* RFC 4880 §5.2.3.4 creation time MUST be hashed */

	if (pgpGet(p, 2, hend, &plen))
	    return 1;
	p += 2;

	if ((p + plen) > hend)
	    return 1;

	if (pgpPrtSubType(p, plen, v->sigtype, _digp, 0))
	    return 1;
	p += plen;

	if (h + hlen - p < 2)
	    return 1;
	pgpPrtHex(" signhash16", p, 2);
	pgpPrtNL();

	if (_digp->pubkey_algo == 0) {
	    _digp->version = v->version;
	    _digp->sigtype = v->sigtype;
	    _digp->pubkey_algo = v->pubkey_algo;
	    _digp->hash_algo = v->hash_algo;
	    memcpy(_digp->signhash16, p, sizeof(_digp->signhash16));
	}

	p += 2;
	if (p > hend)
	    return 1;

	rc = tag ? pgpPrtSigParams(tag, v->pubkey_algo, p, h, hlen, _digp) : 0;
    }	break;
    default:
	rpmlog(RPMLOG_WARNING, _("Unsupported version of signature: V%d\n"), version);
	rc = 1;
	break;
    }
    return rc;
}

static uint8_t curve_oids[] = {
    PGPCURVE_NIST_P_256,	0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
    PGPCURVE_NIST_P_384,	0x05, 0x2b, 0x81, 0x04, 0x00, 0x22,
    PGPCURVE_NIST_P_521,	0x05, 0x2b, 0x81, 0x04, 0x00, 0x23,
    PGPCURVE_BRAINPOOL_P256R1,	0x09, 0x2b, 0x24, 0x03, 0x03, 0x02, 0x08, 0x01, 0x01, 0x07,
    PGPCURVE_BRAINPOOL_P512R1,	0x09, 0x2b, 0x24, 0x03, 0x03, 0x02, 0x08, 0x01, 0x01, 0x0d,
    PGPCURVE_ED25519,		0x09, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xda, 0x47, 0x0f, 0x01,
    PGPCURVE_CURVE25519,	0x0a, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x97, 0x55, 0x01, 0x05, 0x01,
    0,   
};

static int pgpCurveByOid(const uint8_t *p, int l)
{
    uint8_t *curve;
    for (curve = curve_oids; *curve; curve += 2 + curve[1])
        if (l == (int)curve[1] && !memcmp(p, curve + 2, l))
            return (int)curve[0];
    return 0;
}

static int isKey(pgpDigParams keyp)
{
    return keyp->tag == PGPTAG_PUBLIC_KEY || keyp->tag == PGPTAG_PUBLIC_SUBKEY;
}

static int pgpPrtPubkeyParams(uint8_t pubkey_algo,
		const uint8_t *p, const uint8_t *h, size_t hlen,
		pgpDigParams keyp)
{
    int rc = 1; /* assume failure */
    const uint8_t *pend = h + hlen;
    int curve = 0;
    if (!isKey(keyp))
	return rc;
    /* We can't handle more than one key at a time */
    if (keyp->alg)
	return rc;
    if (pubkey_algo == PGPPUBKEYALGO_EDDSA) {
	int len = (hlen > 1) ? p[0] : 0;
	if (len == 0 || len == 0xff || len >= hlen)
	    return rc;
	curve = pgpCurveByOid(p + 1, len);
	p += len + 1;
    }
    pgpDigAlg keyalg = pgpPubkeyNew(pubkey_algo, curve);
    rc = processMpis(keyalg->mpis, keyalg, p, pend);
    if (rc == 0) {
	keyp->pubkey_algo = pubkey_algo;
	keyp->alg = keyalg;
    } else {
	pgpDigAlgFree(keyalg);
    }
    return rc;
}

static int pgpPrtKey(pgpTag tag, const uint8_t *h, size_t hlen,
		     pgpDigParams _digp)
{
    uint8_t version = 0;
    const uint8_t * p = NULL;
    int rc = 1;

    if (pgpVersion(h, hlen, &version))
	return rc;

    /* We only permit V4 keys, V3 keys are long long since deprecated */
    switch (version) {
    case 4:
    {   pgpPktKeyV4 v = (pgpPktKeyV4)h;

	if (hlen > sizeof(*v)) {
	    pgpPrtVal("V4 ", PGPVAL_TAG, tag);
	    pgpPrtVal(" ", PGPVAL_PUBKEYALGO, v->pubkey_algo);
	    pgpPrtTime(" ", v->time, sizeof(v->time));
	    pgpPrtNL();

	    /* If _digp->hash is not NULL then signature is already loaded */
	    if (_digp->hash == NULL) {
		_digp->version = v->version;
		if (!(_digp->saved & PGPDIG_SAVED_TIME))
		    _digp->time = pgpGrab(v->time, sizeof(v->time));
		_digp->saved |= PGPDIG_SAVED_TIME | PGPDIG_SIG_HAS_CREATION_TIME;
	    }

	    p = ((uint8_t *)v) + sizeof(*v);
	    rc = pgpPrtPubkeyParams(v->pubkey_algo, p, h, hlen, _digp);
	}
    }	break;
    default:
	rpmlog(RPMLOG_WARNING, _("Unsupported version of key: V%d\n"), h[0]);
    }
    return rc;
}

static int pgpPrtUserID(pgpTag tag, const uint8_t *h, size_t hlen,
			pgpDigParams _digp)
{
    pgpPrtVal("", PGPVAL_TAG, tag);
    if (_print)
	fprintf(stderr, " \"%.*s\"", (int)hlen, (const char *)h);
    pgpPrtNL();
    free(_digp->userid);
    _digp->userid = memcpy(xmalloc(hlen+1), h, hlen);
    _digp->userid[hlen] = '\0';
    return 0;
}

static int getPubkeyFingerprint(const uint8_t *h, size_t hlen,
			  uint8_t **fp, size_t *fplen)
{
    int rc = -1; /* assume failure */
    const uint8_t *se;
    const uint8_t *pend = h + hlen;
    uint8_t version = 0;

    if (pgpVersion(h, hlen, &version))
	return rc;

    /* We only permit V4 keys, V3 keys are long long since deprecated */
    switch (version) {
    case 4:
      {	pgpPktKeyV4 v = (pgpPktKeyV4) (h);
	int mpis = -1;

	/* Packet must be strictly larger than v to have room for the
	 * required MPIs and (for EdDSA) the curve ID */
	if (hlen < sizeof(*v) + sizeof(uint8_t))
	    return rc;
	se = (uint8_t *)(v + 1);
	switch (v->pubkey_algo) {
	case PGPPUBKEYALGO_EDDSA:
	    /* EdDSA has a curve id before the MPIs */
	    if (se[0] == 0x00 || se[0] == 0xff || pend - se < 1 + se[0])
		return rc;
	    se += 1 + se[0];
	    mpis = 1;
	    break;
	case PGPPUBKEYALGO_RSA:
	    mpis = 2;
	    break;
	case PGPPUBKEYALGO_DSA:
	    mpis = 4;
	    break;
	default:
	    return rc;
	}

	/* Does the size and number of MPI's match our expectations? */
	if (processMpis(mpis, NULL, se, pend) == 0) {
	    DIGEST_CTX ctx = rpmDigestInit(RPM_HASH_SHA1, RPMDIGEST_NONE);
	    uint8_t *d = NULL;
	    size_t dlen = 0;
	    uint8_t in[3] = { 0x99, (hlen >> 8), hlen };

	    (void) rpmDigestUpdate(ctx, in, 3);
	    (void) rpmDigestUpdate(ctx, h, hlen);
	    (void) rpmDigestFinal(ctx, (void **)&d, &dlen, 0);

	    if (dlen == 20) {
		rc = 0;
		*fp = d;
		*fplen = dlen;
	    } else {
		free(d);
	    }
	}

      }	break;
    default:
	rpmlog(RPMLOG_WARNING, _("Unsupported version of key: V%d\n"), version);
    }
    return rc;
}

int pgpPubkeyFingerprint(const uint8_t * pkt, size_t pktlen,
                         uint8_t **fp, size_t *fplen)
{
    struct pgpPkt p;

    if (decodePkt(pkt, pktlen, &p))
	return -1;

    return getPubkeyFingerprint(p.body, p.blen, fp, fplen);
}

static int getKeyID(const uint8_t *h, size_t hlen, pgpKeyID_t keyid)
{
    uint8_t *fp = NULL;
    size_t fplen = 0;
    int rc = getPubkeyFingerprint(h, hlen, &fp, &fplen);
    if (fp && fplen > 8) {
	memcpy(keyid, (fp + (fplen-8)), 8);
	free(fp);
    }
    return rc;
}

int pgpPubkeyKeyID(const uint8_t * pkt, size_t pktlen, pgpKeyID_t keyid)
{
    struct pgpPkt p;

    if (decodePkt(pkt, pktlen, &p))
	return -1;
    
    return getKeyID(p.body, p.blen, keyid);
}

static int pgpPrtPkt(struct pgpPkt *p, pgpDigParams _digp)
{
    int rc = 0;

    switch (p->tag) {
    case PGPTAG_SIGNATURE:
	rc = pgpPrtSig(p->tag, p->body, p->blen, _digp);
	break;
    case PGPTAG_PUBLIC_KEY:
	/* Get the public key Key ID. */
	rc = getKeyID(p->body, p->blen, _digp->signid);
	if (rc)
	    memset(_digp->signid, 0, sizeof(_digp->signid));
	else {
	    _digp->saved |= PGPDIG_SAVED_ID;
	    rc = pgpPrtKey(p->tag, p->body, p->blen, _digp);
	}
	break;
    case PGPTAG_USER_ID:
	rc = pgpPrtUserID(p->tag, p->body, p->blen, _digp);
	break;
    case PGPTAG_RESERVED:
	rc = -1;
	break;
    case PGPTAG_COMMENT:
    case PGPTAG_COMMENT_OLD:
    case PGPTAG_PUBLIC_SUBKEY:
    case PGPTAG_SECRET_KEY:
    case PGPTAG_SECRET_SUBKEY:
    case PGPTAG_PUBLIC_SESSION_KEY:
    case PGPTAG_SYMMETRIC_SESSION_KEY:
    case PGPTAG_COMPRESSED_DATA:
    case PGPTAG_SYMMETRIC_DATA:
    case PGPTAG_MARKER:
    case PGPTAG_LITERAL_DATA:
    case PGPTAG_TRUST:
    case PGPTAG_PHOTOID:
    case PGPTAG_ENCRYPTED_MDC:
    case PGPTAG_MDC:
    case PGPTAG_PRIVATE_60:
    case PGPTAG_PRIVATE_62:
    case PGPTAG_CONTROL:
    default:
	pgpPrtVal("", PGPVAL_TAG, p->tag);
	pgpPrtHex("", p->body, p->blen);
	pgpPrtNL();
	break;
    }

    return rc;
}

pgpDigParams pgpDigParamsFree(pgpDigParams digp)
{
    if (digp) {
	pgpDigAlgFree(digp->alg);
	free(digp->userid);
	free(digp->hash);
	memset(digp, 0, sizeof(*digp));
	free(digp);
    }
    return NULL;
}

int pgpDigParamsCmp(pgpDigParams p1, pgpDigParams p2)
{
    int rc = 1; /* assume different, eg if either is NULL */
    if (p1 && p2) {
	/* XXX Should we compare something else too? */
	if (p1->tag != p2->tag)
	    goto exit;
	if (p1->hash_algo != p2->hash_algo)
	    goto exit;
	if (p1->pubkey_algo != p2->pubkey_algo)
	    goto exit;
	if (p1->version != p2->version)
	    goto exit;
	if (p1->sigtype != p2->sigtype)
	    goto exit;
	if (memcmp(p1->signid, p2->signid, sizeof(p1->signid)) != 0)
	    goto exit;
	if (p1->userid && p2->userid && strcmp(p1->userid, p2->userid) != 0)
	    goto exit;

	/* Parameters match ... at least for our purposes */
	rc = 0;
    }
exit:
    return rc;
}

unsigned int pgpDigParamsAlgo(pgpDigParams digp, unsigned int algotype)
{
    unsigned int algo = 0; /* assume failure */
    if (digp) {
	switch (algotype) {
	case PGPVAL_PUBKEYALGO:
	    algo = digp->pubkey_algo;
	    break;
	case PGPVAL_HASHALGO:
	    algo = digp->hash_algo;
	    break;
	}
    }
    return algo;
}

const uint8_t *pgpDigParamsSignID(pgpDigParams digp)
{
    return digp->signid;
}

const char *pgpDigParamsUserID(pgpDigParams digp)
{
    return digp->userid;
}

int pgpDigParamsVersion(pgpDigParams digp)
{
    return digp->version;
}

uint32_t pgpDigParamsCreationTime(pgpDigParams digp)
{
    return digp->time;
}

static pgpDigParams pgpDigParamsNew(uint8_t tag)
{
    pgpDigParams digp = xcalloc(1, sizeof(*digp));
    digp->tag = tag;
    return digp;
}

static int hashKey(DIGEST_CTX hash, const struct pgpPkt *pkt, int exptag)
{
    int rc = -1;
    if (pkt->tag == exptag) {
	uint8_t head[] = {
	    0x99,
	    (pkt->blen >> 8),
	    (pkt->blen     ),
	};

	rpmDigestUpdate(hash, head, 3);
	rpmDigestUpdate(hash, pkt->body, pkt->blen);
	rc = 0;
    }
    return rc;
}

static int pgpVerifySelf(pgpDigParams key, pgpDigParams selfsig,
			const struct pgpPkt *all, int i)
{
    int rc = -1;
    DIGEST_CTX hash = NULL;

    switch (selfsig->sigtype) {
    case PGPSIGTYPE_SUBKEY_BINDING:
	hash = rpmDigestInit(selfsig->hash_algo, 0);
	if (hash) {
	    rc = hashKey(hash, &all[0], PGPTAG_PUBLIC_KEY);
	    if (!rc)
		rc = hashKey(hash, &all[i-1], PGPTAG_PUBLIC_SUBKEY);
	}
	break;
    default:
	/* ignore types we can't handle */
	rc = 0;
	break;
    }

    if (hash && rc == 0)
	rc = pgpVerifySignature(key, selfsig, hash);

    rpmDigestFinal(hash, NULL, NULL, 0);

    return rc;
}

static int parseSubkeySig(const struct pgpPkt *pkt, uint8_t tag,
			  pgpDigParams *params_p) {
    pgpDigParams params = *params_p = NULL; /* assume failure */

    if (pkt->tag != PGPTAG_SIGNATURE)
	goto fail;

    params = pgpDigParamsNew(tag);

    if (pgpPrtSig(tag, pkt->body, pkt->blen, params))
	goto fail;

    if (params->sigtype != PGPSIGTYPE_SUBKEY_BINDING &&
	params->sigtype != PGPSIGTYPE_SUBKEY_REVOKE)
    {
	goto fail;
    }

    *params_p = params;
    return 0;
fail:
    pgpDigParamsFree(params);
    return -1;
}

static const size_t RPM_MAX_OPENPGP_BYTES = 65535; /* max number of bytes in a key */

int pgpPrtParams(const uint8_t * pkts, size_t pktlen, unsigned int pkttype,
		 pgpDigParams * ret)
{
    const uint8_t *p = pkts;
    const uint8_t *pend = pkts + pktlen;
    pgpDigParams digp = NULL;
    pgpDigParams selfsig = NULL;
    int i = 0;
    int alloced = 16; /* plenty for normal cases */
    int rc = -1; /* assume failure */
    int expect = 0;
    int prevtag = 0;

    if (pktlen > RPM_MAX_OPENPGP_BYTES)
	return rc; /* reject absurdly large data */

    struct pgpPkt *all = xmalloc(alloced * sizeof(*all));
    while (p < pend) {
	struct pgpPkt *pkt = &all[i];
	if (decodePkt(p, (pend - p), pkt))
	    break;

	if (digp == NULL) {
	    if (pkttype && pkt->tag != pkttype) {
		break;
	    } else {
		digp = pgpDigParamsNew(pkt->tag);
	    }
	} else if (pkt->tag == PGPTAG_PUBLIC_KEY)
	    break;

	if (expect) {
	    if (pkt->tag != expect)
		break;
	    selfsig = pgpDigParamsNew(pkt->tag);
	}

	if (pgpPrtPkt(pkt, selfsig ? selfsig : digp))
	    break;

	if (selfsig) {
	    /* subkeys must be followed by binding signature */
	    int xx = 1; /* assume failure */

	    if (!(prevtag == PGPTAG_PUBLIC_SUBKEY &&
		  selfsig->sigtype != PGPSIGTYPE_SUBKEY_BINDING))
		xx = pgpVerifySelf(digp, selfsig, all, i);

	    selfsig = pgpDigParamsFree(selfsig);
	    if (xx)
		break;
	    expect = 0;
	}

	if (pkt->tag == PGPTAG_PUBLIC_SUBKEY)
	    expect = PGPTAG_SIGNATURE;
	prevtag = pkt->tag;

	i++;
	p += (pkt->body - pkt->head) + pkt->blen;
	if (pkttype == PGPTAG_SIGNATURE)
	    break;

	if (alloced <= i) {
	    alloced *= 2;
	    all = xrealloc(all, alloced * sizeof(*all));
	}
    }

    rc = (digp && (p == pend) && expect == 0) ? 0 : -1;

    free(all);
    selfsig = pgpDigParamsFree(selfsig);
    if (ret && rc == 0) {
	*ret = digp;
    } else {
	pgpDigParamsFree(digp);
    }
    return rc;
}

int pgpPrtParams2(const uint8_t * pkts, size_t pktlen, unsigned int pkttype,
                  pgpDigParams * ret, char **lints)
{
    if (lints)
        *lints = NULL;
    return pgpPrtParams(pkts, pktlen, pkttype, ret);
}

int pgpPrtParamsSubkeys(const uint8_t *pkts, size_t pktlen,
			pgpDigParams mainkey, pgpDigParams **subkeys,
			int *subkeysCount)
{
    const uint8_t *p = pkts;
    const uint8_t *pend = pkts + pktlen;
    pgpDigParams *digps = NULL;
    int count = 0;
    int alloced = 10;
    struct pgpPkt pkt;
    int rc, i;

    digps = xmalloc(alloced * sizeof(*digps));

    while (p < pend) {
	if (decodePkt(p, (pend - p), &pkt))
	    break;

	p += (pkt.body - pkt.head) + pkt.blen;

	if (pkt.tag == PGPTAG_PUBLIC_SUBKEY) {
	    if (count == alloced) {
		alloced <<= 1;
		digps = xrealloc(digps, alloced * sizeof(*digps));
	    }

	    digps[count] = pgpDigParamsNew(PGPTAG_PUBLIC_SUBKEY);
	    /* Copy UID from main key to subkey */
	    digps[count]->userid = xstrdup(mainkey->userid);

	    if (getKeyID(pkt.body, pkt.blen, digps[count]->signid)) {
		pgpDigParamsFree(digps[count]);
		continue;
	    }

	    if (pgpPrtKey(pkt.tag, pkt.body, pkt.blen, digps[count])) {
		pgpDigParamsFree(digps[count]);
		continue;
	    }

	    pgpDigParams subkey_sig = NULL;
	    if (decodePkt(p, pend - p, &pkt) ||
	        parseSubkeySig(&pkt, 0, &subkey_sig))
	    {
		pgpDigParamsFree(digps[count]);
		break;
	    }

	    /* Is the subkey revoked or incapable of signing? */
	    int ignore = subkey_sig->sigtype != PGPSIGTYPE_SUBKEY_BINDING ||
			 !((subkey_sig->saved & PGPDIG_SIG_HAS_KEY_FLAGS) &&
			   (subkey_sig->key_flags & 0x02));
	    if (ignore) {
		pgpDigParamsFree(digps[count]);
	    } else {
		digps[count]->key_flags = subkey_sig->key_flags;
		digps[count]->saved |= PGPDIG_SIG_HAS_KEY_FLAGS;
		count++;
	    }
	    p += (pkt.body - pkt.head) + pkt.blen;
	    pgpDigParamsFree(subkey_sig);
	}
    }
    rc = (p == pend) ? 0 : -1;

    if (rc == 0) {
	*subkeys = xrealloc(digps, count * sizeof(*digps));
	*subkeysCount = count;
    } else {
	for (i = 0; i < count; i++)
	    pgpDigParamsFree(digps[i]);
	free(digps);
    }

    return rc;
}

rpmRC pgpVerifySignature(pgpDigParams key, pgpDigParams sig, DIGEST_CTX hashctx)
{
    DIGEST_CTX ctx = rpmDigestDup(hashctx);
    uint8_t *hash = NULL;
    size_t hashlen = 0;
    rpmRC res = RPMRC_FAIL; /* assume failure */

    if (sig == NULL || ctx == NULL)
	goto exit;

    if (sig->tag != PGPTAG_SIGNATURE)
	goto exit;

    if (sig->hash != NULL)
	rpmDigestUpdate(ctx, sig->hash, sig->hashlen);

    if (sig->version == 4) {
	/* V4 trailer is six octets long (rfc4880) */
	uint8_t trailer[6];
	uint32_t nb = sig->hashlen;
	nb = htonl(nb);
	trailer[0] = sig->version;
	trailer[1] = 0xff;
	memcpy(trailer+2, &nb, 4);
	rpmDigestUpdate(ctx, trailer, sizeof(trailer));
    }

    rpmDigestFinal(ctx, (void **)&hash, &hashlen, 0);
    ctx = NULL;

    /* Compare leading 16 bits of digest for quick check. */
    if (hash == NULL || memcmp(hash, sig->signhash16, 2) != 0)
	goto exit;

    /*
     * If we have a key, verify the signature for real. Otherwise we've
     * done all we can, return NOKEY to indicate "looks okay but dunno."
     */
    if (key && key->alg) {
	if (!isKey(key))
	    goto exit;
	pgpDigAlg sa = sig->alg;
	pgpDigAlg ka = key->alg;
	if (sa && sa->verify && sig->pubkey_algo == key->pubkey_algo) {
	    if (sa->verify(ka, sa, hash, hashlen, sig->hash_algo) == 0) {
		res = RPMRC_OK;
	    }
	}
    } else {
	res = RPMRC_NOKEY;
    }

exit:
    free(hash);
    rpmDigestFinal(ctx, NULL, NULL, 0);
    return res;

}

rpmRC pgpVerifySignature2(pgpDigParams key, pgpDigParams sig, DIGEST_CTX hashctx, char **lints)
{
    if (lints)
        *lints = NULL;
    return pgpVerifySignature(key, sig, hashctx);
}

static pgpArmor decodePkts(uint8_t *b, uint8_t **pkt, size_t *pktlen)
{
    const char * enc = NULL;
    const char * crcenc = NULL;
    uint8_t * dec;
    uint8_t * crcdec;
    size_t declen;
    size_t crclen;
    uint32_t crcpkt, crc;
    const char * armortype = NULL;
    char * t, * te;
    int pstate = 0;
    pgpArmor ec = PGPARMOR_ERR_NO_BEGIN_PGP;	/* XXX assume failure */

#define	TOKEQ(_s, _tok)	(rstreqn((_s), (_tok), sizeof(_tok)-1))

    for (t = (char *)b; t && *t; t = te) {
	int rc;
	if ((te = strchr(t, '\n')) == NULL)
	    te = t + strlen(t);
	else
	    te++;

	switch (pstate) {
	case 0:
	    armortype = NULL;
	    if (!TOKEQ(t, "-----BEGIN PGP "))
		continue;
	    t += sizeof("-----BEGIN PGP ")-1;

	    rc = pgpValTok(pgpArmorTbl, t, te);
	    if (rc < 0) {
		ec = PGPARMOR_ERR_UNKNOWN_ARMOR_TYPE;
		goto exit;
	    }
	    if (rc != PGPARMOR_PUBKEY)	/* XXX ASCII Pubkeys only, please. */
		continue;

	    armortype = pgpValString(PGPVAL_ARMORBLOCK, rc);
	    t += strlen(armortype);
	    if (!TOKEQ(t, "-----"))
		continue;
	    t += sizeof("-----")-1;
	    if (*t != '\n' && *t != '\r')
		continue;
	    *t = '\0';
	    pstate++;
	    break;
	case 1:
	    enc = NULL;
	    rc = pgpValTok(pgpArmorKeyTbl, t, te);
	    if (rc >= 0)
		continue;
	    if (*t != '\n' && *t != '\r') {
		pstate = 0;
		continue;
	    }
	    enc = te;		/* Start of encoded packets */
	    pstate++;
	    break;
	case 2:
	    crcenc = NULL;
	    if (*t != '=')
		continue;
	    *t++ = '\0';	/* Terminate encoded packets */
	    crcenc = t;		/* Start of encoded crc */
	    pstate++;
	    break;
	case 3:
	    pstate = 0;
	    if (!TOKEQ(t, "-----END PGP ")) {
		ec = PGPARMOR_ERR_NO_END_PGP;
		goto exit;
	    }
	    *t = '\0';		/* Terminate encoded crc */
	    t += sizeof("-----END PGP ")-1;
	    if (t >= te) continue;

	    if (armortype == NULL) /* XXX can't happen */
		continue;
	    if (!rstreqn(t, armortype, strlen(armortype)))
		continue;

	    t += strlen(armortype);
	    if (t >= te) continue;

	    if (!TOKEQ(t, "-----")) {
		ec = PGPARMOR_ERR_NO_END_PGP;
		goto exit;
	    }
	    t += (sizeof("-----")-1);
	    /* Handle EOF without EOL here, *t == '\0' at EOF */
	    if (*t && (t >= te)) continue;
	    /* XXX permitting \r here is not RFC-2440 compliant <shrug> */
	    if (!(*t == '\n' || *t == '\r' || *t == '\0')) continue;

	    crcdec = NULL;
	    crclen = 0;
	    if (rpmBase64Decode(crcenc, (void **)&crcdec, &crclen) != 0 || crclen != 3) {
		crcdec = _free(crcdec);
		ec = PGPARMOR_ERR_CRC_DECODE;
		goto exit;
	    }
	    crcpkt = pgpGrab(crcdec, crclen);
	    crcdec = _free(crcdec);
	    dec = NULL;
	    declen = 0;
	    if (rpmBase64Decode(enc, (void **)&dec, &declen) != 0) {
		ec = PGPARMOR_ERR_BODY_DECODE;
		goto exit;
	    }
	    crc = pgpCRC(dec, declen);
	    if (crcpkt != crc) {
		ec = PGPARMOR_ERR_CRC_CHECK;
		_free(dec);
		goto exit;
	    }
	    if (pkt)
		*pkt = dec;
	    else
		_free(dec);
	    if (pktlen) *pktlen = declen;
	    ec = PGPARMOR_PUBKEY;	/* XXX ASCII Pubkeys only, please. */
	    goto exit;
	    break;
	}
    }
    ec = PGPARMOR_NONE;

exit:
    return ec;
}


pgpArmor pgpParsePkts(const char *armor, uint8_t ** pkt, size_t * pktlen)
{
    pgpArmor ec = PGPARMOR_ERR_NO_BEGIN_PGP;	/* XXX assume failure */
    if (armor && strlen(armor) > 0) {
	uint8_t *b = (uint8_t*) xstrdup(armor);
	ec = decodePkts(b, pkt, pktlen);
	free(b);
    }
    return ec;
}

int pgpPubKeyCertLen(const uint8_t *pkts, size_t pktslen, size_t *certlen)
{
    const uint8_t *p = pkts;
    const uint8_t *pend = pkts + pktslen;
    struct pgpPkt pkt;

    while (p < pend) {
	if (decodePkt(p, (pend - p), &pkt))
	    return -1;

	if (pkt.tag == PGPTAG_PUBLIC_KEY && pkts != p) {
	    *certlen = p - pkts;
	    return 0;
	}

	p += (pkt.body - pkt.head) + pkt.blen;
    }

    *certlen = pktslen;

    return 0;
}

char * pgpArmorWrap(int atype, const unsigned char * s, size_t ns)
{
    char *buf = NULL, *val = NULL;
    char *enc = rpmBase64Encode(s, ns, -1);
    char *crc = rpmBase64CRC(s, ns);
    const char *valstr = pgpValString(PGPVAL_ARMORBLOCK, atype);

    if (crc != NULL && enc != NULL) {
	rasprintf(&buf, "%s=%s", enc, crc);
    }
    free(crc);
    free(enc);

    rasprintf(&val, "-----BEGIN PGP %s-----\nVersion: rpm-" VERSION"\n\n"
		    "%s\n-----END PGP %s-----\n",
		    valstr, buf != NULL ? buf : "", valstr);

    free(buf);
    return val;
}

rpmRC pgpPubKeyLint(const uint8_t *pkts, size_t pktslen, char **explanation)
{
    *explanation = NULL;
    return RPMRC_OK;
}
