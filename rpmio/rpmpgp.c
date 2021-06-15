/** \ingroup rpmio signature
 * \file rpmio/rpmpgp.c
 * Routines to handle RFC-2440 detached signatures.
 */

#include "system.h"

#include <time.h>
#include <netinet/in.h>
#include <rpm/rpmstring.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmbase64.h>

#include "rpmio/digest.h"
#include "rpmio/rpmio_internal.h"	/* XXX rpmioSlurp */

#include "debug.h"

static int _print = 0;

/** \ingroup rpmio
 * Container for values parsed from an OpenPGP signature and public key.
 */
struct pgpDig_s {
    struct pgpDigParams_s * signature;
    struct pgpDigParams_s * pubkey;
};

typedef const struct pgpValTbl_s {
    int val;
    char const * const str;
} * pgpValTbl;
 
static struct pgpValTbl_s const pgpSigTypeTbl[] = {
    { PGPSIGTYPE_BINARY,	"Binary document signature" },
    { PGPSIGTYPE_TEXT,		"Text document signature" },
    { PGPSIGTYPE_STANDALONE,	"Standalone signature" },
    { PGPSIGTYPE_GENERIC_CERT,	"Generic certification of a User ID and Public Key" },
    { PGPSIGTYPE_PERSONA_CERT,	"Persona certification of a User ID and Public Key" },
    { PGPSIGTYPE_CASUAL_CERT,	"Casual certification of a User ID and Public Key" },
    { PGPSIGTYPE_POSITIVE_CERT,	"Positive certification of a User ID and Public Key" },
    { PGPSIGTYPE_SUBKEY_BINDING,"Subkey Binding Signature" },
    { PGPSIGTYPE_SIGNED_KEY,	"Signature directly on a key" },
    { PGPSIGTYPE_KEY_REVOKE,	"Key revocation signature" },
    { PGPSIGTYPE_SUBKEY_REVOKE,	"Subkey revocation signature" },
    { PGPSIGTYPE_CERT_REVOKE,	"Certification revocation signature" },
    { PGPSIGTYPE_TIMESTAMP,	"Timestamp signature" },
    { -1,			"Unknown signature type" },
};

static struct pgpValTbl_s const pgpPubkeyTbl[] = {
    { PGPPUBKEYALGO_RSA,	"RSA" },
    { PGPPUBKEYALGO_RSA_ENCRYPT,"RSA(Encrypt-Only)" },
    { PGPPUBKEYALGO_RSA_SIGN,	"RSA(Sign-Only)" },
    { PGPPUBKEYALGO_ELGAMAL_ENCRYPT,"Elgamal(Encrypt-Only)" },
    { PGPPUBKEYALGO_DSA,	"DSA" },
    { PGPPUBKEYALGO_EC,		"Elliptic Curve" },
    { PGPPUBKEYALGO_ECDSA,	"ECDSA" },
    { PGPPUBKEYALGO_ELGAMAL,	"Elgamal" },
    { PGPPUBKEYALGO_DH,		"Diffie-Hellman (X9.42)" },
    { PGPPUBKEYALGO_EDDSA,	"EdDSA" },
    { -1,			"Unknown public key algorithm" },
};

static struct pgpValTbl_s const pgpSymkeyTbl[] = {
    { PGPSYMKEYALGO_PLAINTEXT,	"Plaintext" },
    { PGPSYMKEYALGO_IDEA,	"IDEA" },
    { PGPSYMKEYALGO_TRIPLE_DES,	"3DES" },
    { PGPSYMKEYALGO_CAST5,	"CAST5" },
    { PGPSYMKEYALGO_BLOWFISH,	"BLOWFISH" },
    { PGPSYMKEYALGO_SAFER,	"SAFER" },
    { PGPSYMKEYALGO_DES_SK,	"DES/SK" },
    { PGPSYMKEYALGO_AES_128,	"AES(128-bit key)" },
    { PGPSYMKEYALGO_AES_192,	"AES(192-bit key)" },
    { PGPSYMKEYALGO_AES_256,	"AES(256-bit key)" },
    { PGPSYMKEYALGO_TWOFISH,	"TWOFISH(256-bit key)" },
    { PGPSYMKEYALGO_NOENCRYPT,	"no encryption" },
    { -1,			"Unknown symmetric key algorithm" },
};

static struct pgpValTbl_s const pgpCompressionTbl[] = {
    { PGPCOMPRESSALGO_NONE,	"Uncompressed" },
    { PGPCOMPRESSALGO_ZIP,	"ZIP" },
    { PGPCOMPRESSALGO_ZLIB, 	"ZLIB" },
    { PGPCOMPRESSALGO_BZIP2, 	"BZIP2" },
    { -1,			"Unknown compression algorithm" },
};

static struct pgpValTbl_s const pgpHashTbl[] = {
    { PGPHASHALGO_MD5,		"MD5" },
    { PGPHASHALGO_SHA1,		"SHA1" },
    { PGPHASHALGO_RIPEMD160,	"RIPEMD160" },
    { PGPHASHALGO_MD2,		"MD2" },
    { PGPHASHALGO_TIGER192,	"TIGER192" },
    { PGPHASHALGO_HAVAL_5_160,	"HAVAL-5-160" },
    { PGPHASHALGO_SHA256,	"SHA256" },
    { PGPHASHALGO_SHA384,	"SHA384" },
    { PGPHASHALGO_SHA512,	"SHA512" },
    { PGPHASHALGO_SHA224,	"SHA224" },
    { -1,			"Unknown hash algorithm" },
};

static struct pgpValTbl_s const pgpKeyServerPrefsTbl[] = {
    { 0x80,			"No-modify" },
    { -1,			"Unknown key server preference" },
};

static struct pgpValTbl_s const pgpSubTypeTbl[] = {
    { PGPSUBTYPE_SIG_CREATE_TIME,"signature creation time" },
    { PGPSUBTYPE_SIG_EXPIRE_TIME,"signature expiration time" },
    { PGPSUBTYPE_EXPORTABLE_CERT,"exportable certification" },
    { PGPSUBTYPE_TRUST_SIG,	"trust signature" },
    { PGPSUBTYPE_REGEX,		"regular expression" },
    { PGPSUBTYPE_REVOCABLE,	"revocable" },
    { PGPSUBTYPE_KEY_EXPIRE_TIME,"key expiration time" },
    { PGPSUBTYPE_ARR,		"additional recipient request" },
    { PGPSUBTYPE_PREFER_SYMKEY,	"preferred symmetric algorithms" },
    { PGPSUBTYPE_REVOKE_KEY,	"revocation key" },
    { PGPSUBTYPE_ISSUER_KEYID,	"issuer key ID" },
    { PGPSUBTYPE_NOTATION,	"notation data" },
    { PGPSUBTYPE_PREFER_HASH,	"preferred hash algorithms" },
    { PGPSUBTYPE_PREFER_COMPRESS,"preferred compression algorithms" },
    { PGPSUBTYPE_KEYSERVER_PREFERS,"key server preferences" },
    { PGPSUBTYPE_PREFER_KEYSERVER,"preferred key server" },
    { PGPSUBTYPE_PRIMARY_USERID,"primary user id" },
    { PGPSUBTYPE_POLICY_URL,	"policy URL" },
    { PGPSUBTYPE_KEY_FLAGS,	"key flags" },
    { PGPSUBTYPE_SIGNER_USERID,	"signer's user id" },
    { PGPSUBTYPE_REVOKE_REASON,	"reason for revocation" },
    { PGPSUBTYPE_FEATURES,	"features" },
    { PGPSUBTYPE_EMBEDDED_SIG,	"embedded signature" },

    { PGPSUBTYPE_INTERNAL_100,	"internal subpkt type 100" },
    { PGPSUBTYPE_INTERNAL_101,	"internal subpkt type 101" },
    { PGPSUBTYPE_INTERNAL_102,	"internal subpkt type 102" },
    { PGPSUBTYPE_INTERNAL_103,	"internal subpkt type 103" },
    { PGPSUBTYPE_INTERNAL_104,	"internal subpkt type 104" },
    { PGPSUBTYPE_INTERNAL_105,	"internal subpkt type 105" },
    { PGPSUBTYPE_INTERNAL_106,	"internal subpkt type 106" },
    { PGPSUBTYPE_INTERNAL_107,	"internal subpkt type 107" },
    { PGPSUBTYPE_INTERNAL_108,	"internal subpkt type 108" },
    { PGPSUBTYPE_INTERNAL_109,	"internal subpkt type 109" },
    { PGPSUBTYPE_INTERNAL_110,	"internal subpkt type 110" },
    { -1,			"Unknown signature subkey type" },
};

static struct pgpValTbl_s const pgpTagTbl[] = {
    { PGPTAG_PUBLIC_SESSION_KEY,"Public-Key Encrypted Session Key" },
    { PGPTAG_SIGNATURE,		"Signature" },
    { PGPTAG_SYMMETRIC_SESSION_KEY,"Symmetric-Key Encrypted Session Key" },
    { PGPTAG_ONEPASS_SIGNATURE,	"One-Pass Signature" },
    { PGPTAG_SECRET_KEY,	"Secret Key" },
    { PGPTAG_PUBLIC_KEY,	"Public Key" },
    { PGPTAG_SECRET_SUBKEY,	"Secret Subkey" },
    { PGPTAG_COMPRESSED_DATA,	"Compressed Data" },
    { PGPTAG_SYMMETRIC_DATA,	"Symmetrically Encrypted Data" },
    { PGPTAG_MARKER,		"Marker" },
    { PGPTAG_LITERAL_DATA,	"Literal Data" },
    { PGPTAG_TRUST,		"Trust" },
    { PGPTAG_USER_ID,		"User ID" },
    { PGPTAG_PUBLIC_SUBKEY,	"Public Subkey" },
    { PGPTAG_COMMENT_OLD,	"Comment (from OpenPGP draft)" },
    { PGPTAG_PHOTOID,		"PGP's photo ID" },
    { PGPTAG_ENCRYPTED_MDC,	"Integrity protected encrypted data" },
    { PGPTAG_MDC,		"Manipulaion detection code packet" },
    { PGPTAG_PRIVATE_60,	"Private #60" },
    { PGPTAG_COMMENT,		"Comment" },
    { PGPTAG_PRIVATE_62,	"Private #62" },
    { PGPTAG_CONTROL,		"Control (GPG)" },
    { -1,			"Unknown packet tag" },
};

static struct pgpValTbl_s const pgpArmorTbl[] = {
    { PGPARMOR_MESSAGE,		"MESSAGE" },
    { PGPARMOR_PUBKEY,		"PUBLIC KEY BLOCK" },
    { PGPARMOR_SIGNATURE,	"SIGNATURE" },
    { PGPARMOR_SIGNED_MESSAGE,	"SIGNED MESSAGE" },
    { PGPARMOR_FILE,		"ARMORED FILE" },
    { PGPARMOR_PRIVKEY,		"PRIVATE KEY BLOCK" },
    { PGPARMOR_SECKEY,		"SECRET KEY BLOCK" },
    { -1,			"Unknown armor block" }
};

static struct pgpValTbl_s const pgpArmorKeyTbl[] = {
    { PGPARMORKEY_VERSION,	"Version: " },
    { PGPARMORKEY_COMMENT,	"Comment: " },
    { PGPARMORKEY_MESSAGEID,	"MessageID: " },
    { PGPARMORKEY_HASH,		"Hash: " },
    { PGPARMORKEY_CHARSET,	"Charset: " },
    { -1,			"Unknown armor key" }
};

static void pgpPrtNL(void)
{
    if (!_print) return;
    fprintf(stderr, "\n");
}

static const char * pgpValStr(pgpValTbl vs, uint8_t val)
{
    do {
	if (vs->val == val)
	    break;
    } while ((++vs)->val != -1);
    return vs->str;
}

static pgpValTbl pgpValTable(pgpValType type)
{
    switch (type) {
    case PGPVAL_TAG:		return pgpTagTbl;
    case PGPVAL_ARMORBLOCK:	return pgpArmorTbl;
    case PGPVAL_ARMORKEY:	return pgpArmorKeyTbl;
    case PGPVAL_SIGTYPE:	return pgpSigTypeTbl;
    case PGPVAL_SUBTYPE:	return pgpSubTypeTbl;
    case PGPVAL_PUBKEYALGO:	return pgpPubkeyTbl;
    case PGPVAL_SYMKEYALGO:	return pgpSymkeyTbl;
    case PGPVAL_COMPRESSALGO:	return pgpCompressionTbl;
    case PGPVAL_HASHALGO: 	return pgpHashTbl;
    case PGPVAL_SERVERPREFS:	return pgpKeyServerPrefsTbl;
    default:
	break;
    }
    return NULL;
}

const char * pgpValString(pgpValType type, uint8_t val)
{
    pgpValTbl tbl = pgpValTable(type);
    return (tbl != NULL) ? pgpValStr(tbl, val) : NULL;
}

static void pgpPrtHex(const char *pre, const uint8_t *p, size_t plen)
{
    char *hex = NULL;
    if (!_print) return;
    if (pre && *pre)
	fprintf(stderr, "%s", pre);
    hex = pgpHexStr(p, plen);
    fprintf(stderr, " %s", hex);
    free(hex);
}

static void pgpPrtVal(const char * pre, pgpValTbl vs, uint8_t val)
{
    if (!_print) return;
    if (pre && *pre)
	fprintf(stderr, "%s", pre);
    fprintf(stderr, "%s(%u)", pgpValStr(vs, val), (unsigned)val);
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
		  unsigned int *valp)
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
	    if (plen > lenlen) {
		pkt->blen = pgpGrab(p+1, lenlen);
	    }
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

static int pgpPrtSubType(const uint8_t *h, size_t hlen, pgpSigType sigtype, 
			 pgpDigParams _digp)
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

	pgpPrtVal("    ", pgpSubTypeTbl, (p[0]&(~PGPSUBTYPE_CRITICAL)));
	if (p[0] & PGPSUBTYPE_CRITICAL)
	    if (_print)
		fprintf(stderr, " *CRITICAL*");
	switch (*p) {
	case PGPSUBTYPE_PREFER_SYMKEY:	/* preferred symmetric algorithms */
	    for (i = 1; i < plen; i++)
		pgpPrtVal(" ", pgpSymkeyTbl, p[i]);
	    break;
	case PGPSUBTYPE_PREFER_HASH:	/* preferred hash algorithms */
	    for (i = 1; i < plen; i++)
		pgpPrtVal(" ", pgpHashTbl, p[i]);
	    break;
	case PGPSUBTYPE_PREFER_COMPRESS:/* preferred compression algorithms */
	    for (i = 1; i < plen; i++)
		pgpPrtVal(" ", pgpCompressionTbl, p[i]);
	    break;
	case PGPSUBTYPE_KEYSERVER_PREFERS:/* key server preferences */
	    for (i = 1; i < plen; i++)
		pgpPrtVal(" ", pgpKeyServerPrefsTbl, p[i]);
	    break;
	case PGPSUBTYPE_SIG_CREATE_TIME:
	    impl = *p;
	    if (!(_digp->saved & PGPDIG_SAVED_TIME) &&
		(sigtype == PGPSIGTYPE_POSITIVE_CERT || sigtype == PGPSIGTYPE_BINARY || sigtype == PGPSIGTYPE_TEXT || sigtype == PGPSIGTYPE_STANDALONE))
	    {
		if (plen-1 != sizeof(_digp->time))
		    break;
		_digp->saved |= PGPDIG_SAVED_TIME;
		_digp->time = pgpGrab(p+1, sizeof(_digp->time));
	    }
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
	case PGPSUBTYPE_KEY_FLAGS:
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

static int pgpPrtSigParams(pgpTag tag, uint8_t pubkey_algo, uint8_t sigtype,
		const uint8_t *p, const uint8_t *h, size_t hlen,
		pgpDigParams sigp)
{
    int rc = 1; /* assume failure */
    const uint8_t * pend = h + hlen;
    int i;
    pgpDigAlg sigalg = pgpSignatureNew(pubkey_algo);

    for (i = 0; i < sigalg->mpis && pend - p >= 2; i++) {
	int mpil = pgpMpiLen(p);
	if (pend - p < mpil)
	    break;
	if (sigtype == PGPSIGTYPE_BINARY || sigtype == PGPSIGTYPE_TEXT) {
	    if (sigalg->setmpi(sigalg, i, p))
		break;
	}
	p += mpil;
    }

    /* Does the size and number of MPI's match our expectations? */
    if (p == pend && i == sigalg->mpis)
	rc = 0;
    
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
    unsigned int plen;
    int rc = 1;

    if (pgpVersion(h, hlen, &version))
	return rc;

    switch (version) {
    case 3:
    {   pgpPktSigV3 v = (pgpPktSigV3)h;

	if (hlen <= sizeof(*v) || v->hashlen != 5)
	    return 1;

	pgpPrtVal("V3 ", pgpTagTbl, tag);
	pgpPrtVal(" ", pgpPubkeyTbl, v->pubkey_algo);
	pgpPrtVal(" ", pgpHashTbl, v->hash_algo);
	pgpPrtVal(" ", pgpSigTypeTbl, v->sigtype);
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
	    _digp->time = pgpGrab(v->time, sizeof(v->time));
	    memcpy(_digp->signid, v->signid, sizeof(_digp->signid));
	    _digp->pubkey_algo = v->pubkey_algo;
	    _digp->hash_algo = v->hash_algo;
	    memcpy(_digp->signhash16, v->signhash16, sizeof(_digp->signhash16));
	}

	p = ((uint8_t *)v) + sizeof(*v);
	rc = pgpPrtSigParams(tag, v->pubkey_algo, v->sigtype, p, h, hlen, _digp);
    }	break;
    case 4:
    {   pgpPktSigV4 v = (pgpPktSigV4)h;
	const uint8_t *const hend = h + hlen;

	if (hlen <= sizeof(*v))
	    return 1;

	pgpPrtVal("V4 ", pgpTagTbl, tag);
	pgpPrtVal(" ", pgpPubkeyTbl, v->pubkey_algo);
	pgpPrtVal(" ", pgpHashTbl, v->hash_algo);
	pgpPrtVal(" ", pgpSigTypeTbl, v->sigtype);
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
	if (pgpPrtSubType(p, plen, v->sigtype, _digp))
	    return 1;
	p += plen;

	if (pgpGet(p, 2, hend, &plen))
	    return 1;
	p += 2;

	if ((p + plen) > hend)
	    return 1;

	if (pgpPrtSubType(p, plen, v->sigtype, _digp))
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

	rc = pgpPrtSigParams(tag, v->pubkey_algo, v->sigtype, p, h, hlen, _digp);
    }	break;
    default:
	rpmlog(RPMLOG_WARNING, _("Unsupported version of signature: V%d\n"), version);
	rc = 1;
	break;
    }
    return rc;
}

char * pgpHexStr(const uint8_t *p, size_t plen)
{
    char *t, *str;
    str = t = xmalloc(plen * 2 + 1);
    static char const hex[] = "0123456789abcdef";
    while (plen-- > 0) {
	size_t i;
	i = *p++;
	*t++ = hex[ (i >> 4) & 0xf ];
	*t++ = hex[ (i     ) & 0xf ];
    }
    *t = '\0';
    return str;
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

static int pgpPrtPubkeyParams(uint8_t pubkey_algo,
		const uint8_t *p, const uint8_t *h, size_t hlen,
		pgpDigParams keyp)
{
    int rc = 1;
    const uint8_t *pend = h + hlen;
    int i, curve = 0;
    pgpDigAlg keyalg;
    if (pubkey_algo == PGPPUBKEYALGO_EDDSA) {
	int len = p + 1 < pend ? p[0] : 0;
	if (len == 0 || len == 0xff || p + 1 + len > pend)
	    goto exit;
	curve = pgpCurveByOid(p + 1, len);
	p += len + 1;
    }
    keyalg = pgpPubkeyNew(pubkey_algo, curve);
    for (i = 0; i < keyalg->mpis && p + 2 <= pend; i++) {
	int mpil = pgpMpiLen(p);
	if (p + mpil > pend)
	    break;
	if (keyalg->setmpi(keyalg, i, p))
	    break;
	p += mpil;
    }

    /* Does the size and number of MPI's match our expectations? */
    if (p == pend && i == keyalg->mpis)
	rc = 0;

    /* We can't handle more than one key at a time */
    if (rc == 0 && keyp->alg == NULL && (keyp->tag == PGPTAG_PUBLIC_KEY ||
	keyp->tag == PGPTAG_PUBLIC_SUBKEY))

	keyp->alg = keyalg;
    else
	pgpDigAlgFree(keyalg);

exit:
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
	    pgpPrtVal("V4 ", pgpTagTbl, tag);
	    pgpPrtVal(" ", pgpPubkeyTbl, v->pubkey_algo);
	    pgpPrtTime(" ", v->time, sizeof(v->time));
	    pgpPrtNL();

	    /* If _digp->hash is not NULL then signature is already loaded */
	    if (_digp->hash == NULL) {
		_digp->version = v->version;
		_digp->time = pgpGrab(v->time, sizeof(v->time));
		_digp->pubkey_algo = v->pubkey_algo;
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
    pgpPrtVal("", pgpTagTbl, tag);
    if (_print)
	fprintf(stderr, " \"%.*s\"", (int)hlen, (const char *)h);
    pgpPrtNL();
    free(_digp->userid);
    _digp->userid = memcpy(xmalloc(hlen+1), h, hlen);
    _digp->userid[hlen] = '\0';
    return 0;
}

int pgpPubkeyFingerprint(const uint8_t *h, size_t hlen,
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

	/* Packet must be larger than v to have room for the required MPIs */
	if (hlen > sizeof(*v)) {
	    switch (v->pubkey_algo) {
	    case PGPPUBKEYALGO_RSA:
		mpis = 2;
		break;
	    case PGPPUBKEYALGO_DSA:
		mpis = 4;
		break;
	    case PGPPUBKEYALGO_EDDSA:
		mpis = 1;
		break;
	    }
	}

	se = (uint8_t *)(v + 1);
	/* EdDSA has a curve id before the MPIs */
	if (v->pubkey_algo == PGPPUBKEYALGO_EDDSA) {
	    if (se < pend && se[0] != 0x00 && se[0] != 0xff)
		se += 1 + se[0];
	    else
		se = pend;      /* error out when reading the MPI */
	}
	while (se < pend && mpis-- > 0)
	    se += pgpMpiLen(se);

	/* Does the size and number of MPI's match our expectations? */
	if (se == pend && mpis == 0) {
	    DIGEST_CTX ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
	    uint8_t *d = NULL;
	    size_t dlen = 0;
	    int i = se - h;
	    uint8_t in[3] = { 0x99, (i >> 8), i };

	    (void) rpmDigestUpdate(ctx, in, 3);
	    (void) rpmDigestUpdate(ctx, h, i);
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

static int getKeyID(const uint8_t *h, size_t hlen, pgpKeyID_t keyid)
{
    uint8_t *fp = NULL;
    size_t fplen = 0;
    int rc = pgpPubkeyFingerprint(h, hlen, &fp, &fplen);
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
	if (!getKeyID(p->body, p->blen, _digp->signid))
	    _digp->saved |= PGPDIG_SAVED_ID;
	else
	    memset(_digp->signid, 0, sizeof(_digp->signid));
	rc = pgpPrtKey(p->tag, p->body, p->blen, _digp);
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
	pgpPrtVal("", pgpTagTbl, p->tag);
	pgpPrtHex("", p->body, p->blen);
	pgpPrtNL();
	break;
    }

    return rc;
}

pgpDig pgpNewDig(void)
{
    pgpDig dig = xcalloc(1, sizeof(*dig));

    return dig;
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

void pgpCleanDig(pgpDig dig)
{
    if (dig != NULL) {
	pgpDigParamsFree(dig->signature);
	pgpDigParamsFree(dig->pubkey);
	memset(dig, 0, sizeof(*dig));
    }
    return;
}

pgpDig pgpFreeDig(pgpDig dig)
{
    if (dig != NULL) {

	/* DUmp the signature/pubkey data. */
	pgpCleanDig(dig);
	dig = _free(dig);
    }
    return dig;
}

pgpDigParams pgpDigGetParams(pgpDig dig, unsigned int pkttype)
{
    pgpDigParams params = NULL;
    if (dig) {
	switch (pkttype) {
	case PGPTAG_SIGNATURE:
	    params = dig->signature;
	    break;
	case PGPTAG_PUBLIC_KEY:
	    params = dig->pubkey;
	    break;
	}
    }
    return params;
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

int pgpPrtParams(const uint8_t * pkts, size_t pktlen, unsigned int pkttype,
		 pgpDigParams * ret)
{
    const uint8_t *p = pkts;
    const uint8_t *pend = pkts + pktlen;
    pgpDigParams digp = NULL;
    struct pgpPkt pkt;
    int rc = -1; /* assume failure */

    while (p < pend) {
	if (decodePkt(p, (pend - p), &pkt))
	    break;

	if (digp == NULL) {
	    if (pkttype && pkt.tag != pkttype) {
		break;
	    } else {
		digp = xcalloc(1, sizeof(*digp));
		digp->tag = pkt.tag;
	    }
	}

	if (pgpPrtPkt(&pkt, digp))
	    break;

	p += (pkt.body - pkt.head) + pkt.blen;
	if (pkttype == PGPTAG_SIGNATURE)
	    break;
    }

    rc = (digp && (p == pend)) ? 0 : -1;

    if (ret && rc == 0) {
	*ret = digp;
    } else {
	pgpDigParamsFree(digp);
    }
    return rc;
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

	    digps[count] = xcalloc(1, sizeof(**digps));
	    digps[count]->tag = PGPTAG_PUBLIC_SUBKEY;
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
	    count++;
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

int pgpPrtPkts(const uint8_t * pkts, size_t pktlen, pgpDig dig, int printing)
{
    int rc;
    pgpDigParams digp = NULL;

    _print = printing;

    rc = pgpPrtParams(pkts, pktlen, 0, &digp);

    if (dig && rc == 0) {
	if (digp->tag == PGPTAG_SIGNATURE) {
	    pgpDigParamsFree(dig->signature);
	    dig->signature = digp;
	} else {
	    pgpDigParamsFree(dig->pubkey);
	    dig->pubkey = digp;
	}
    } else {
	pgpDigParamsFree(digp);
    }

    return rc;
}

char *pgpIdentItem(pgpDigParams digp)
{
    char *id = NULL;
    if (digp) {
	
	char *signid = pgpHexStr(digp->signid+4, sizeof(digp->signid)-4);
	rasprintf(&id, _("V%d %s/%s %s, key ID %s"),
			digp->version,
			pgpValStr(pgpPubkeyTbl, digp->pubkey_algo),
			pgpValStr(pgpHashTbl, digp->hash_algo),
			pgpValStr(pgpTagTbl, digp->tag),
			signid);
	free(signid);
    } else {
	id = xstrdup(_("(none)"));
    }
    return id;
}

rpmRC pgpVerifySignature(pgpDigParams key, pgpDigParams sig, DIGEST_CTX hashctx)
{
    DIGEST_CTX ctx = rpmDigestDup(hashctx);
    uint8_t *hash = NULL;
    size_t hashlen = 0;
    rpmRC res = RPMRC_FAIL; /* assume failure */

    if (sig == NULL || ctx == NULL)
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

    /* Compare leading 16 bits of digest for quick check. */
    if (hash == NULL || memcmp(hash, sig->signhash16, 2) != 0)
	goto exit;

    /*
     * If we have a key, verify the signature for real. Otherwise we've
     * done all we can, return NOKEY to indicate "looks okay but dunno."
     */
    if (key && key->alg) {
	pgpDigAlg sa = sig->alg;
	pgpDigAlg ka = key->alg;
	if (sa && sa->verify) {
	    if (sa->verify(ka, sa, hash, hashlen, sig->hash_algo) == 0) {
		res = RPMRC_OK;
	    }
	}
    } else {
	res = RPMRC_NOKEY;
    }

exit:
    free(hash);
    return res;

}

rpmRC pgpVerifySig(pgpDig dig, DIGEST_CTX hashctx)
{
    if (dig == NULL || hashctx == NULL)
	return RPMRC_FAIL;

    return pgpVerifySignature(pgpDigGetParams(dig, PGPTAG_PUBLIC_KEY),
			      pgpDigGetParams(dig, PGPTAG_SIGNATURE), hashctx);
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

	    armortype = pgpValStr(pgpArmorTbl, rc);
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
	    if (rpmBase64Decode(crcenc, (void **)&crcdec, &crclen) != 0) {
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

pgpArmor pgpReadPkts(const char * fn, uint8_t ** pkt, size_t * pktlen)
{
    uint8_t * b = NULL;
    ssize_t blen;
    pgpArmor ec = PGPARMOR_ERR_NO_BEGIN_PGP;	/* XXX assume failure */
    int rc = rpmioSlurp(fn, &b, &blen);
    if (rc == 0 && b != NULL && blen > 0) {
	ec = decodePkts(b, pkt, pktlen);
    }
    free(b);
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
    const char *valstr = pgpValStr(pgpArmorTbl, atype);

    if (crc != NULL && enc != NULL) {
	rasprintf(&buf, "%s=%s", enc, crc);
    }
    free(crc);
    free(enc);

    rasprintf(&val, "-----BEGIN PGP %s-----\nVersion: rpm-" VERSION " (NSS-3)\n\n"
		    "%s\n-----END PGP %s-----\n",
		    valstr, buf != NULL ? buf : "", valstr);

    free(buf);
    return val;
}

