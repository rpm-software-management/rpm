/** \ingroup rpmio signature
 * \file rpmio/rpmpgp.c
 * Routines to handle RFC-2440 detached signatures.
 */

#include "system.h"

#include <rpm/rpmstring.h>

#include "rpmio/digest.h"
#include "rpmio/rpmio_internal.h"	/* XXX rpmioSlurp */

#include "debug.h"


static int _debug = 0;

static int _print = 0;

static int _crypto_initialized = 0;

static pgpDig _dig = NULL;

static pgpDigParams _digp = NULL;

struct pgpValTbl_s pgpSigTypeTbl[] = {
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

struct pgpValTbl_s pgpPubkeyTbl[] = {
    { PGPPUBKEYALGO_RSA,	"RSA" },
    { PGPPUBKEYALGO_RSA_ENCRYPT,"RSA(Encrypt-Only)" },
    { PGPPUBKEYALGO_RSA_SIGN,	"RSA(Sign-Only)" },
    { PGPPUBKEYALGO_ELGAMAL_ENCRYPT,"Elgamal(Encrypt-Only)" },
    { PGPPUBKEYALGO_DSA,	"DSA" },
    { PGPPUBKEYALGO_EC,		"Elliptic Curve" },
    { PGPPUBKEYALGO_ECDSA,	"ECDSA" },
    { PGPPUBKEYALGO_ELGAMAL,	"Elgamal" },
    { PGPPUBKEYALGO_DH,		"Diffie-Hellman (X9.42)" },
    { -1,			"Unknown public key algorithm" },
};

struct pgpValTbl_s pgpSymkeyTbl[] = {
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

struct pgpValTbl_s pgpCompressionTbl[] = {
    { PGPCOMPRESSALGO_NONE,	"Uncompressed" },
    { PGPCOMPRESSALGO_ZIP,	"ZIP" },
    { PGPCOMPRESSALGO_ZLIB, 	"ZLIB" },
    { PGPCOMPRESSALGO_BZIP2, 	"BZIP2" },
    { -1,			"Unknown compression algorithm" },
};

struct pgpValTbl_s pgpHashTbl[] = {
    { PGPHASHALGO_MD5,		"MD5" },
    { PGPHASHALGO_SHA1,		"SHA1" },
    { PGPHASHALGO_RIPEMD160,	"RIPEMD160" },
    { PGPHASHALGO_MD2,		"MD2" },
    { PGPHASHALGO_TIGER192,	"TIGER192" },
    { PGPHASHALGO_HAVAL_5_160,	"HAVAL-5-160" },
    { PGPHASHALGO_SHA256,	"SHA256" },
    { PGPHASHALGO_SHA384,	"SHA384" },
    { PGPHASHALGO_SHA512,	"SHA512" },
    { -1,			"Unknown hash algorithm" },
};

struct pgpValTbl_s pgpKeyServerPrefsTbl[] = {
    { 0x80,			"No-modify" },
    { -1,			"Unknown key server preference" },
};

struct pgpValTbl_s pgpSubTypeTbl[] = {
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

struct pgpValTbl_s pgpTagTbl[] = {
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

struct pgpValTbl_s pgpArmorTbl[] = {
    { PGPARMOR_MESSAGE,		"MESSAGE" },
    { PGPARMOR_PUBKEY,		"PUBLIC KEY BLOCK" },
    { PGPARMOR_SIGNATURE,	"SIGNATURE" },
    { PGPARMOR_SIGNED_MESSAGE,	"SIGNED MESSAGE" },
    { PGPARMOR_FILE,		"ARMORED FILE" },
    { PGPARMOR_PRIVKEY,		"PRIVATE KEY BLOCK" },
    { PGPARMOR_SECKEY,		"SECRET KEY BLOCK" },
    { -1,			"Unknown armor block" }
};

struct pgpValTbl_s pgpArmorKeyTbl[] = {
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

static void pgpPrtInt(const char *pre, int i)
{
    if (!_print) return;
    if (pre && *pre)
	fprintf(stderr, "%s", pre);
    fprintf(stderr, " %d", i);
}

static void pgpPrtStr(const char *pre, const char *s)
{
    if (!_print) return;
    if (pre && *pre)
	fprintf(stderr, "%s", pre);
    fprintf(stderr, " %s", s);
}

static void pgpPrtHex(const char *pre, const uint8_t *p, size_t plen)
{
    if (!_print) return;
    if (pre && *pre)
	fprintf(stderr, "%s", pre);
    fprintf(stderr, " %s", pgpHexStr(p, plen));
}

void pgpPrtVal(const char * pre, pgpValTbl vs, uint8_t val)
{
    if (!_print) return;
    if (pre && *pre)
	fprintf(stderr, "%s", pre);
    fprintf(stderr, "%s(%u)", pgpValStr(vs, val), (unsigned)val);
}

/**
 * @return		0 on success
 */
static int pgpMpiSet(const char * pre, int lbits,
		void *dest, const uint8_t * p, const uint8_t * pend)
{
    unsigned int mbits = pgpMpiBits(p);
    unsigned int nbits;
    size_t nbytes;
    char *t = dest;
    unsigned int ix;

    if ((p + ((mbits+7) >> 3)) > pend)
	return 1;

    if (mbits > lbits)
	return 1;

    nbits = (lbits > mbits ? lbits : mbits);
    nbytes = ((nbits + 7) >> 3);
    ix = (nbits - mbits) >> 3;

if (_debug)
fprintf(stderr, "*** mbits %u nbits %u nbytes %zu ix %u\n", mbits, nbits, nbytes, ix);
    if (ix > 0) memset(t, '\0', ix);
    memcpy(t+ix, p+2, nbytes-ix);
if (_debug)
fprintf(stderr, "*** %s %s\n", pre, pgpHexStr(dest, nbytes));

    return 0;
}

/**
 * @return		NULL on error
 */
static SECItem *pgpMpiItem(PRArenaPool *arena, SECItem *item, const uint8_t *p)
{
    size_t nbytes = pgpMpiLen(p)-2;

    if (item == NULL) {
    	if ((item=SECITEM_AllocItem(arena, item, nbytes)) == NULL)
    	    return item;
    } else {
    	if (arena != NULL)
    	    item->data = PORT_ArenaGrow(arena, item->data, item->len, nbytes);
    	else
    	    item->data = PORT_Realloc(item->data, nbytes);
    	
    	if (item->data == NULL) {
    	    if (arena == NULL)
    		SECITEM_FreeItem(item, PR_TRUE);
    	    return NULL;
    	}
    }

    memcpy(item->data, p+2, nbytes);
    item->len = nbytes;
    return item;
}
/*@=boundswrite@*/

static SECKEYPublicKey *pgpNewPublicKey(KeyType type)
{
    PRArenaPool *arena;
    SECKEYPublicKey *key;
    
    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (arena == NULL)
	return NULL;
    
    key = PORT_ArenaZAlloc(arena, sizeof(SECKEYPublicKey));
    
    if (key == NULL) {
	PORT_FreeArena(arena, PR_FALSE);
	return NULL;
    }
    
    key->keyType = type;
    key->pkcs11ID = CK_INVALID_HANDLE;
    key->pkcs11Slot = NULL;
    key->arena = arena;
    return key;
}

static SECKEYPublicKey *pgpNewRSAKey(void)
{
    return pgpNewPublicKey(rsaKey);
}

static SECKEYPublicKey *pgpNewDSAKey(void)
{
    return pgpNewPublicKey(dsaKey);
}

int pgpPrtSubType(const uint8_t *h, size_t hlen, pgpSigType sigtype)
{
    const uint8_t *p = h;
    size_t plen, i;

    while (hlen > 0) {
	i = pgpLen(p, &plen);
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
	    if (_digp && !(_digp->saved & PGPDIG_SAVED_TIME) &&
		(sigtype == PGPSIGTYPE_POSITIVE_CERT || sigtype == PGPSIGTYPE_BINARY || sigtype == PGPSIGTYPE_TEXT || sigtype == PGPSIGTYPE_STANDALONE))
	    {
		_digp->saved |= PGPDIG_SAVED_TIME;
		memcpy(_digp->time, p+1, sizeof(_digp->time));
	    }
	case PGPSUBTYPE_SIG_EXPIRE_TIME:
	case PGPSUBTYPE_KEY_EXPIRE_TIME:
	    if ((plen - 1) == 4) {
		time_t t = pgpGrab(p+1, plen-1);
		if (_print)
		   fprintf(stderr, " %-24.24s(0x%08x)", ctime(&t), (unsigned)t);
	    } else
		pgpPrtHex("", p+1, plen-1);
	    break;

	case PGPSUBTYPE_ISSUER_KEYID:	/* issuer key ID */
	    if (_digp && !(_digp->saved & PGPDIG_SAVED_ID) &&
		(sigtype == PGPSIGTYPE_POSITIVE_CERT || sigtype == PGPSIGTYPE_BINARY || sigtype == PGPSIGTYPE_TEXT || sigtype == PGPSIGTYPE_STANDALONE))
	    {
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
	p += plen;
	hlen -= plen;
    }
    return 0;
}

static const char * pgpSigRSA[] = {
    " m**d =",
    NULL,
};

static const char * pgpSigDSA[] = {
    "    r =",
    "    s =",
    NULL,
};

#ifndef DSA_SUBPRIME_LEN
#define DSA_SUBPRIME_LEN 20
#endif

static int pgpPrtSigParams(pgpTag tag, uint8_t pubkey_algo, uint8_t sigtype,
		const uint8_t *p, const uint8_t *h, size_t hlen)
{
    const uint8_t * pend = h + hlen;
    size_t i;
    SECItem dsaraw;
    unsigned char dsabuf[2*DSA_SUBPRIME_LEN];

    dsaraw.type = 0;
    dsaraw.data = dsabuf;
    dsaraw.len = sizeof(dsabuf);
    
    for (i = 0; p < pend; i++, p += pgpMpiLen(p)) {
	if (pubkey_algo == PGPPUBKEYALGO_RSA) {
	    if (i >= 1) break;
	    if (_dig &&
	(sigtype == PGPSIGTYPE_BINARY || sigtype == PGPSIGTYPE_TEXT))
	    {
		switch (i) {
		case 0:		/* m**d */
		    _dig->rsasig = pgpMpiItem(NULL, _dig->rsasig, p);
		    if (_dig->rsasig == NULL)
			return 1;
		    break;
		default:
		    break;
		}
	    }
	    pgpPrtStr("", pgpSigRSA[i]);
	} else if (pubkey_algo == PGPPUBKEYALGO_DSA) {
	    if (i >= 2) break;
	    if (_dig &&
	(sigtype == PGPSIGTYPE_BINARY || sigtype == PGPSIGTYPE_TEXT))
	    {
		int xx;
		xx = 0;
		switch (i) {
		case 0:
		    memset(dsaraw.data, '\0', 2*DSA_SUBPRIME_LEN);
				/* r */
		    xx = pgpMpiSet(pgpSigDSA[i], DSA_SUBPRIME_LEN*8, dsaraw.data, p, pend);
		    break;
		case 1:		/* s */
		    xx = pgpMpiSet(pgpSigDSA[i], DSA_SUBPRIME_LEN*8, dsaraw.data + DSA_SUBPRIME_LEN, p, pend);
		    if (_dig->dsasig != NULL)
		    	SECITEM_FreeItem(_dig->dsasig, PR_FALSE);
		    else if ((_dig->dsasig=SECITEM_AllocItem(NULL, NULL, 0)) == NULL) {
		        xx = 1;
		        break;
		    }
		    if (DSAU_EncodeDerSig(_dig->dsasig, &dsaraw) != SECSuccess)
		    	xx = 1;
		    break;
		default:
		    xx = 1;
		    break;
		}
		if (xx) return xx;
	    }
	    pgpPrtStr("", pgpSigDSA[i]);
	} else {
	    if (_print)
		fprintf(stderr, "%7zd", i);
	}
	pgpPrtStr("", pgpMpiStr(p));
	pgpPrtNL();
    }

    return 0;
}

int pgpPrtSig(pgpTag tag, const uint8_t *h, size_t hlen)
{
    uint8_t version = h[0];
    uint8_t * p;
    size_t plen;
    int rc;

    switch (version) {
    case 3:
    {   pgpPktSigV3 v = (pgpPktSigV3)h;
	time_t t;

	if (v->hashlen != 5)
	    return 1;

	pgpPrtVal("V3 ", pgpTagTbl, tag);
	pgpPrtVal(" ", pgpPubkeyTbl, v->pubkey_algo);
	pgpPrtVal(" ", pgpHashTbl, v->hash_algo);
	pgpPrtVal(" ", pgpSigTypeTbl, v->sigtype);
	pgpPrtNL();
	t = pgpGrab(v->time, sizeof(v->time));
	if (_print)
	    fprintf(stderr, " %-24.24s(0x%08x)", ctime(&t), (unsigned)t);
	pgpPrtNL();
	pgpPrtHex(" signer keyid", v->signid, sizeof(v->signid));
	plen = pgpGrab(v->signhash16, sizeof(v->signhash16));
	pgpPrtHex(" signhash16", v->signhash16, sizeof(v->signhash16));
	pgpPrtNL();

	if (_digp && _digp->pubkey_algo == 0) {
	    _digp->version = v->version;
	    _digp->hashlen = v->hashlen;
	    _digp->sigtype = v->sigtype;
	    _digp->hash = memcpy(xmalloc(v->hashlen), &v->sigtype, v->hashlen);
	    memcpy(_digp->time, v->time, sizeof(_digp->time));
	    memcpy(_digp->signid, v->signid, sizeof(_digp->signid));
	    _digp->pubkey_algo = v->pubkey_algo;
	    _digp->hash_algo = v->hash_algo;
	    memcpy(_digp->signhash16, v->signhash16, sizeof(_digp->signhash16));
	}

	p = ((uint8_t *)v) + sizeof(*v);
	rc = pgpPrtSigParams(tag, v->pubkey_algo, v->sigtype, p, h, hlen);
    }	break;
    case 4:
    {   pgpPktSigV4 v = (pgpPktSigV4)h;

	pgpPrtVal("V4 ", pgpTagTbl, tag);
	pgpPrtVal(" ", pgpPubkeyTbl, v->pubkey_algo);
	pgpPrtVal(" ", pgpHashTbl, v->hash_algo);
	pgpPrtVal(" ", pgpSigTypeTbl, v->sigtype);
	pgpPrtNL();

	p = &v->hashlen[0];
	plen = pgpGrab(v->hashlen, sizeof(v->hashlen));
	p += sizeof(v->hashlen);

	if ((p + plen) > (h + hlen))
	    return 1;

if (_debug && _print)
fprintf(stderr, "   hash[%zu] -- %s\n", plen, pgpHexStr(p, plen));
	if (_digp && _digp->pubkey_algo == 0) {
	    _digp->hashlen = sizeof(*v) + plen;
	    _digp->hash = memcpy(xmalloc(_digp->hashlen), v, _digp->hashlen);
	}
	(void) pgpPrtSubType(p, plen, v->sigtype);
	p += plen;

	plen = pgpGrab(p,2);
	p += 2;

	if ((p + plen) > (h + hlen))
	    return 1;

if (_debug && _print)
fprintf(stderr, " unhash[%zu] -- %s\n", plen, pgpHexStr(p, plen));
	(void) pgpPrtSubType(p, plen, v->sigtype);
	p += plen;

	plen = pgpGrab(p,2);
	pgpPrtHex(" signhash16", p, 2);
	pgpPrtNL();

	if (_digp && _digp->pubkey_algo == 0) {
	    _digp->version = v->version;
	    _digp->sigtype = v->sigtype;
	    _digp->pubkey_algo = v->pubkey_algo;
	    _digp->hash_algo = v->hash_algo;
	    memcpy(_digp->signhash16, p, sizeof(_digp->signhash16));
	}

	p += 2;
	if (p > (h + hlen))
	    return 1;

	rc = pgpPrtSigParams(tag, v->pubkey_algo, v->sigtype, p, h, hlen);
    }	break;
    default:
	rc = 1;
	break;
    }
    return rc;
}

static const char * pgpPublicRSA[] = {
    "    n =",
    "    e =",
    NULL,
};

#ifdef NOTYET
static const char * pgpSecretRSA[] = {
    "    d =",
    "    p =",
    "    q =",
    "    u =",
    NULL,
};
#endif

static const char * pgpPublicDSA[] = {
    "    p =",
    "    q =",
    "    g =",
    "    y =",
    NULL,
};

#ifdef NOTYET
static const char * pgpSecretDSA[] = {
    "    x =",
    NULL,
};
#endif

static const char * pgpPublicELGAMAL[] = {
    "    p =",
    "    g =",
    "    y =",
    NULL,
};

#ifdef NOTYET
static const char * pgpSecretELGAMAL[] = {
    "    x =",
    NULL,
};
#endif

static const uint8_t * pgpPrtPubkeyParams(uint8_t pubkey_algo,
		const uint8_t *p, const uint8_t *h, size_t hlen)
{
    size_t i;

    for (i = 0; p < &h[hlen]; i++, p += pgpMpiLen(p)) {
	if (pubkey_algo == PGPPUBKEYALGO_RSA) {
	    if (i >= 2) break;
	    if (_dig) {
		if (_dig->rsa == NULL) {
		    _dig->rsa = pgpNewRSAKey();
		    if (_dig->rsa == NULL)
			break; /* error abort? */
		}
		switch (i) {
		case 0:		/* n */
		    pgpMpiItem(_dig->rsa->arena, &_dig->rsa->u.rsa.modulus, p);
		    break;
		case 1:		/* e */
		    pgpMpiItem(_dig->rsa->arena, &_dig->rsa->u.rsa.publicExponent, p);
		    break;
		default:
		    break;
		}
	    }
	    pgpPrtStr("", pgpPublicRSA[i]);
	} else if (pubkey_algo == PGPPUBKEYALGO_DSA) {
	    if (i >= 4) break;
	    if (_dig) {
		if (_dig->dsa == NULL) {
		    _dig->dsa = pgpNewDSAKey();
		    if (_dig->dsa == NULL)
			break; /* error abort? */
		}
		switch (i) {
		case 0:		/* p */
		    pgpMpiItem(_dig->dsa->arena, &_dig->dsa->u.dsa.params.prime, p);
		    break;
		case 1:		/* q */
		    pgpMpiItem(_dig->dsa->arena, &_dig->dsa->u.dsa.params.subPrime, p);
		    break;
		case 2:		/* g */
		    pgpMpiItem(_dig->dsa->arena, &_dig->dsa->u.dsa.params.base, p);
		    break;
		case 3:		/* y */
		    pgpMpiItem(_dig->dsa->arena, &_dig->dsa->u.dsa.publicValue, p);
		    break;
		default:
		    break;
		}
	    }
	    pgpPrtStr("", pgpPublicDSA[i]);
	} else if (pubkey_algo == PGPPUBKEYALGO_ELGAMAL_ENCRYPT) {
	    if (i >= 3) break;
	    pgpPrtStr("", pgpPublicELGAMAL[i]);
	} else {
	    if (_print)
		fprintf(stderr, "%7zd", i);
	}
	pgpPrtStr("", pgpMpiStr(p));
	pgpPrtNL();
    }

    return p;
}

static const uint8_t * pgpPrtSeckeyParams(uint8_t pubkey_algo,
		const uint8_t *p, const uint8_t *h, size_t hlen)
{
    size_t i;

    switch (*p) {
    case 0:
	pgpPrtVal(" ", pgpSymkeyTbl, *p);
	break;
    case 255:
	p++;
	pgpPrtVal(" ", pgpSymkeyTbl, *p);
	switch (p[1]) {
	case 0x00:
	    pgpPrtVal(" simple ", pgpHashTbl, p[2]);
	    p += 2;
	    break;
	case 0x01:
	    pgpPrtVal(" salted ", pgpHashTbl, p[2]);
	    pgpPrtHex("", p+3, 8);
	    p += 10;
	    break;
	case 0x03:
	    pgpPrtVal(" iterated/salted ", pgpHashTbl, p[2]);
	    /* FIX: unsigned cast */
	    i = (16 + (p[11] & 0xf)) << ((p[11] >> 4) + 6);
	    pgpPrtHex("", p+3, 8);
	    pgpPrtInt(" iter", i);
	    p += 11;
	    break;
	}
	break;
    default:
	pgpPrtVal(" ", pgpSymkeyTbl, *p);
	pgpPrtHex(" IV", p+1, 8);
	p += 8;
	break;
    }
    pgpPrtNL();

    p++;

#ifdef	NOTYET	/* XXX encrypted MPI's need to be handled. */
    for (i = 0; p < &h[hlen]; i++, p += pgpMpiLen(p)) {
	if (pubkey_algo == PGPPUBKEYALGO_RSA) {
	    if (pgpSecretRSA[i] == NULL) break;
	    pgpPrtStr("", pgpSecretRSA[i]);
	} else if (pubkey_algo == PGPPUBKEYALGO_DSA) {
	    if (pgpSecretDSA[i] == NULL) break;
	    pgpPrtStr("", pgpSecretDSA[i]);
	} else if (pubkey_algo == PGPPUBKEYALGO_ELGAMAL_ENCRYPT) {
	    if (pgpSecretELGAMAL[i] == NULL) break;
	    pgpPrtStr("", pgpSecretELGAMAL[i]);
	} else {
	    if (_print)
		fprintf(stderr, "%7d", i);
	}
	pgpPrtStr("", pgpMpiStr(p));
	pgpPrtNL();
    }
#else
    pgpPrtHex(" secret", p, (hlen - (p - h) - 2));
    pgpPrtNL();
    p += (hlen - (p - h) - 2);
#endif
    pgpPrtHex(" checksum", p, 2);
    pgpPrtNL();

    return p;
}

int pgpPrtKey(pgpTag tag, const uint8_t *h, size_t hlen)
{
    uint8_t version = *h;
    const uint8_t * p;
    size_t plen;
    time_t t;
    int rc;

    switch (version) {
    case 3:
    {   pgpPktKeyV3 v = (pgpPktKeyV3)h;
	pgpPrtVal("V3 ", pgpTagTbl, tag);
	pgpPrtVal(" ", pgpPubkeyTbl, v->pubkey_algo);
	t = pgpGrab(v->time, sizeof(v->time));
	if (_print)
	    fprintf(stderr, " %-24.24s(0x%08x)", ctime(&t), (unsigned)t);
	plen = pgpGrab(v->valid, sizeof(v->valid));
	if (plen != 0)
	    fprintf(stderr, " valid %zu days", plen);
	pgpPrtNL();

	if (_digp && _digp->tag == tag) {
	    _digp->version = v->version;
	    memcpy(_digp->time, v->time, sizeof(_digp->time));
	    _digp->pubkey_algo = v->pubkey_algo;
	}

	p = ((uint8_t *)v) + sizeof(*v);
	p = pgpPrtPubkeyParams(v->pubkey_algo, p, h, hlen);
	rc = 0;
    }	break;
    case 4:
    {   pgpPktKeyV4 v = (pgpPktKeyV4)h;
	pgpPrtVal("V4 ", pgpTagTbl, tag);
	pgpPrtVal(" ", pgpPubkeyTbl, v->pubkey_algo);
	t = pgpGrab(v->time, sizeof(v->time));
	if (_print)
	    fprintf(stderr, " %-24.24s(0x%08x)", ctime(&t), (unsigned)t);
	pgpPrtNL();

	if (_digp && _digp->tag == tag) {
	    _digp->version = v->version;
	    memcpy(_digp->time, v->time, sizeof(_digp->time));
	    _digp->pubkey_algo = v->pubkey_algo;
	}

	p = ((uint8_t *)v) + sizeof(*v);
	p = pgpPrtPubkeyParams(v->pubkey_algo, p, h, hlen);
	if (!(tag == PGPTAG_PUBLIC_KEY || tag == PGPTAG_PUBLIC_SUBKEY))
	    p = pgpPrtSeckeyParams(v->pubkey_algo, p, h, hlen);
	rc = 0;
    }	break;
    default:
	rc = 1;
	break;
    }
    return rc;
}

int pgpPrtUserID(pgpTag tag, const uint8_t *h, size_t hlen)
{
    pgpPrtVal("", pgpTagTbl, tag);
    if (_print)
	fprintf(stderr, " \"%.*s\"", (int)hlen, (const char *)h);
    pgpPrtNL();
    if (_digp) {
	char * t;
	_digp->userid = t = memcpy(xmalloc(hlen+1), h, hlen);
	t[hlen] = '\0';
    }
    return 0;
}

int pgpPrtComment(pgpTag tag, const uint8_t *h, size_t hlen)
{
    size_t i = hlen;

    pgpPrtVal("", pgpTagTbl, tag);
    if (_print)
	fprintf(stderr, " ");
    while (i > 0) {
	size_t j;
	if (*h >= ' ' && *h <= 'z') {
	    if (_print)
		fprintf(stderr, "%s", (const char *)h);
	    j = strlen((const char*)h);
	    while (h[j] == '\0')
		j++;
	} else {
	    pgpPrtHex("", h, i);
	    j = i;
	}
	i -= j;
	h += j;
    }
    pgpPrtNL();
    return 0;
}

int pgpPubkeyFingerprint(const uint8_t * pkt, size_t pktlen, pgpKeyID_t keyid)
{
    unsigned int val = *pkt;
    size_t plen, hlen;
    pgpTag tag;
    const uint8_t *se, *h;
    DIGEST_CTX ctx;
    int rc = -1;	/* assume failure. */

    if (!(val & 0x80))
	return rc;

    if (val & 0x40) {
	tag = (val & 0x3f);
	plen = pgpLen(pkt+1, &hlen);
    } else {
	tag = (val >> 2) & 0xf;
	plen = (1 << (val & 0x3));
	hlen = pgpGrab(pkt+1, plen);
    }
    if (pktlen > 0 && 1 + plen + hlen > pktlen)
	return rc;
    
    h = pkt + 1 + plen;

    switch (h[0]) {
    case 3:
      {	pgpPktKeyV3 v = (pgpPktKeyV3) (h);
	se = (uint8_t *)(v + 1);
	switch (v->pubkey_algo) {
	case PGPPUBKEYALGO_RSA:
	    se += pgpMpiLen(se);
	    memmove(keyid, (se-8), 8);
	    rc = 0;
	    break;
	default:	/* TODO: md5 of mpi bodies (i.e. no length) */
	    break;
	}
      }	break;
    case 4:
      {	pgpPktKeyV4 v = (pgpPktKeyV4) (h);
	uint8_t * d = NULL;
	size_t dlen;
	int i;

	se = (uint8_t *)(v + 1);
	switch (v->pubkey_algo) {
	case PGPPUBKEYALGO_RSA:
	    for (i = 0; i < 2; i++)
		se += pgpMpiLen(se);
	    break;
	case PGPPUBKEYALGO_DSA:
	    for (i = 0; i < 4; i++)
		se += pgpMpiLen(se);
	    break;
	}

	ctx = rpmDigestInit(PGPHASHALGO_SHA1, RPMDIGEST_NONE);
	(void) rpmDigestUpdate(ctx, pkt, (se-pkt));
	(void) rpmDigestFinal(ctx, (void **)&d, &dlen, 0);

	memmove(keyid, (d + (dlen-8)), 8);
	if (d) free(d);
	rc = 0;

      }	break;
    }
    return rc;
}

int pgpExtractPubkeyFingerprint(const char * b64pkt, pgpKeyID_t keyid)
{
    uint8_t * pkt;
    size_t pktlen;

    if (b64decode(b64pkt, (void **)&pkt, &pktlen))
       return -1;      /* on error */
    (void) pgpPubkeyFingerprint(pkt, pktlen, keyid);
    pkt = _free(pkt);
    return sizeof(keyid);  /* no. of bytes of pubkey signid */
}

int pgpPrtPkt(const uint8_t *pkt, size_t pleft)
{
    unsigned int val = *pkt;
    size_t pktlen;
    pgpTag tag;
    size_t plen;
    const uint8_t *h;
    size_t hlen = 0;
    int rc = 0;

    /* XXX can't deal with these. */
    if (!(val & 0x80))
	return -1;

    if (val & 0x40) {
	tag = (val & 0x3f);
	plen = pgpLen(pkt+1, &hlen);
    } else {
	tag = (val >> 2) & 0xf;
	plen = (1 << (val & 0x3));
	hlen = pgpGrab(pkt+1, plen);
    }

    pktlen = 1 + plen + hlen;
    if (pktlen > pleft)
	return -1;

    h = pkt + 1 + plen;
    switch (tag) {
    case PGPTAG_SIGNATURE:
	rc = pgpPrtSig(tag, h, hlen);
	break;
    case PGPTAG_PUBLIC_KEY:
	/* Get the public key fingerprint. */
	if (_digp) {
	    if (!pgpPubkeyFingerprint(pkt, pktlen, _digp->signid))
		_digp->saved |= PGPDIG_SAVED_ID;
	    else
		memset(_digp->signid, 0, sizeof(_digp->signid));
	}
    case PGPTAG_PUBLIC_SUBKEY:
	rc = pgpPrtKey(tag, h, hlen);
	break;
    case PGPTAG_SECRET_KEY:
    case PGPTAG_SECRET_SUBKEY:
	rc = pgpPrtKey(tag, h, hlen);
	break;
    case PGPTAG_USER_ID:
	rc = pgpPrtUserID(tag, h, hlen);
	break;
    case PGPTAG_COMMENT:
    case PGPTAG_COMMENT_OLD:
	rc = pgpPrtComment(tag, h, hlen);
	break;

    case PGPTAG_RESERVED:
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
	pgpPrtVal("", pgpTagTbl, tag);
	pgpPrtHex("", h, hlen);
	pgpPrtNL();
	break;
    }

    return (rc ? -1 : pktlen);
}

pgpDig pgpNewDig(void)
{
    pgpDig dig = xcalloc(1, sizeof(*dig));

    return dig;
}

void pgpCleanDig(pgpDig dig)
{
    if (dig != NULL) {
	int i;
	dig->signature.userid = _constfree(dig->signature.userid);
	dig->pubkey.userid = _constfree(dig->pubkey.userid);
	dig->signature.hash = _constfree(dig->signature.hash);
	dig->pubkey.hash = _constfree(dig->pubkey.hash);
	/* FIX: double indirection */
	for (i = 0; i < 4; i++) {
	    dig->signature.params[i] = _constfree(dig->signature.params[i]);
	    dig->pubkey.params[i] = _constfree(dig->pubkey.params[i]);
	}

	memset(&dig->signature, 0, sizeof(dig->signature));
	memset(&dig->pubkey, 0, sizeof(dig->pubkey));

	dig->md5 = _free(dig->md5);
	dig->sha1 = _free(dig->sha1);

	if (dig->dsa != NULL) {
	    SECKEY_DestroyPublicKey(dig->dsa);
	    dig->dsa = NULL;
	}

	if (dig->dsasig != NULL) {
	    SECITEM_ZfreeItem(dig->dsasig, PR_TRUE);
	    dig->dsasig = NULL;
	}

	if (dig->rsa != NULL) {
	    SECKEY_DestroyPublicKey(dig->rsa);
	    dig->rsa = NULL;
	}

	if (dig->rsasig != NULL) {
	    SECITEM_ZfreeItem(dig->rsasig, PR_TRUE);
	    dig->rsasig = NULL;
	}

    }
    return;
}

pgpDig pgpFreeDig(pgpDig dig)
{
    if (dig != NULL) {

	/* DUmp the signature/pubkey data. */
	pgpCleanDig(dig);

	if (dig->hdrsha1ctx != NULL)
	    (void) rpmDigestFinal(dig->hdrsha1ctx, NULL, NULL, 0);
	dig->hdrsha1ctx = NULL;

	if (dig->sha1ctx != NULL)
	    (void) rpmDigestFinal(dig->sha1ctx, NULL, NULL, 0);
	dig->sha1ctx = NULL;

#ifdef	NOTYET
	if (dig->hdrmd5ctx != NULL)
	    (void) rpmDigestFinal(dig->hdrmd5ctx, NULL, NULL, 0);
	dig->hdrmd5ctx = NULL;
#endif

	if (dig->md5ctx != NULL)
	    (void) rpmDigestFinal(dig->md5ctx, NULL, NULL, 0);
	dig->md5ctx = NULL;

	dig = _free(dig);
    }
    return dig;
}

int pgpPrtPkts(const uint8_t * pkts, size_t pktlen, pgpDig dig, int printing)
{
    unsigned int val = *pkts;
    const uint8_t *p;
    size_t pleft;
    int len;

    _print = printing;
    _dig = dig;
    if (dig != NULL && (val & 0x80)) {
	pgpTag tag = (val & 0x40) ? (val & 0x3f) : ((val >> 2) & 0xf);
	_digp = (tag == PGPTAG_SIGNATURE) ? &_dig->signature : &_dig->pubkey;
	_digp->tag = tag;
    } else
	_digp = NULL;

    for (p = pkts, pleft = pktlen; p < (pkts + pktlen); p += len, pleft -= len) {
	len = pgpPrtPkt(p, pleft);
        if (len <= 0)
	    return len;
	if (len > pleft)	/* XXX shouldn't happen */
	    break;
    }
    return 0;
}

pgpArmor pgpReadPkts(const char * fn, uint8_t ** pkt, size_t * pktlen)
{
    uint8_t * b = NULL;
    ssize_t blen;
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
    int rc;

    rc = rpmioSlurp(fn, &b, &blen);
    if (rc || b == NULL || blen <= 0) {
	goto exit;
    }

    if (pgpIsPkt(b)) {
#ifdef NOTYET	/* XXX ASCII Pubkeys only, please. */
	ec = 0;	/* XXX fish out pkt type. */
#endif
	goto exit;
    }

#define	TOKEQ(_s, _tok)	(!strncmp((_s), (_tok), sizeof(_tok)-1))

    for (t = (char *)b; t && *t; t = te) {
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
	    armortype = t;

	    t = te - (sizeof("-----\n")-1);
	    if (!TOKEQ(t, "-----\n"))
		continue;
	    *t = '\0';
	    pstate++;
	    break;
	case 1:
	    enc = NULL;
	    rc = pgpValTok(pgpArmorKeyTbl, t, te);
	    if (rc >= 0)
		continue;
	    if (*t != '\n') {
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
	    rc = strncmp(t, armortype, strlen(armortype));
	    if (rc)
		continue;

	    t += strlen(armortype);
	    if (t >= te) continue;

	    if (!TOKEQ(t, "-----")) {
		ec = PGPARMOR_ERR_NO_END_PGP;
		goto exit;
	    }
	    t += (sizeof("-----")-1);
	    if (t >= te) continue;
	    /* XXX permitting \r here is not RFC-2440 compliant <shrug> */
	    if (!(*t == '\n' || *t == '\r')) continue;

	    crcdec = NULL;
	    crclen = 0;
	    if (b64decode(crcenc, (void **)&crcdec, &crclen) != 0) {
		ec = PGPARMOR_ERR_CRC_DECODE;
		goto exit;
	    }
	    crcpkt = pgpGrab(crcdec, crclen);
	    crcdec = _free(crcdec);
	    dec = NULL;
	    declen = 0;
	    if (b64decode(enc, (void **)&dec, &declen) != 0) {
		ec = PGPARMOR_ERR_BODY_DECODE;
		goto exit;
	    }
	    crc = pgpCRC(dec, declen);
	    if (crcpkt != crc) {
		ec = PGPARMOR_ERR_CRC_CHECK;
		goto exit;
	    }
	    b = _free(b);
	    b = dec;
	    blen = declen;
	    ec = PGPARMOR_PUBKEY;	/* XXX ASCII Pubkeys only, please. */
	    goto exit;
	    break;
	}
    }
    ec = PGPARMOR_NONE;

exit:
    if (ec > PGPARMOR_NONE && pkt)
	*pkt = b;
    else if (b != NULL)
	b = _free(b);
    if (pktlen)
	*pktlen = blen;
    return ec;
}

char * pgpArmorWrap(int atype, const unsigned char * s, size_t ns)
{
    char * enc;
    char * t;
    size_t nt = 0;
    char * val;

    enc = b64encode(s, ns, -1);
    if (enc != NULL) {
    	nt = strlen(enc);
    }

    nt += 512;	/* XXX slop for armor and crc */

    val = t = xmalloc(nt + 1);
    *t = '\0';
    t = stpcpy(t, "-----BEGIN PGP ");
    t = stpcpy(t, pgpValStr(pgpArmorTbl, atype));
    t = stpcpy( stpcpy(t, "-----\nVersion: rpm-"), VERSION);
    t = stpcpy(t, " (NSS-3)\n\n");

    if (enc != NULL) {
	t = stpcpy(t, enc);
	enc = _free(enc);
	if ((enc = b64crc(s, ns)) != NULL) {
	    *t++ = '=';
	    t = stpcpy(t, enc);
	    enc = _free(enc);
	}
    }
	
    t = stpcpy(t, "-----END PGP ");
    t = stpcpy(t, pgpValStr(pgpArmorTbl, atype));
    t = stpcpy(t, "-----\n");

    return val;
}

int rpmInitCrypto(void) {
    int rc = 0;

    if (!_crypto_initialized && NSS_NoDB_Init(NULL) != SECSuccess) {
	rc = -1;
    } else {
    	_crypto_initialized = 1;
    }

    return rc;
}

int rpmFreeCrypto(void) 
{
    int rc = 0;
    if (_crypto_initialized) {
	rc = (NSS_Shutdown() != SECSuccess);
    	_crypto_initialized = 0;
    }
    return rc;
}
	
	
