/** \ingroup rpmio signature
 * \file rpmio/rpmpgp.c
 * Routines to handle RFC-2440 detached signatures.
 */

#include "system.h"
#include "rpmpgp.h"
#include "debug.h"

/*@-readonlytrans@*/
/* This is the unarmored RPM-GPG-KEY public key. */
const char * redhatPubKeyDSA = "\
mQGiBDfqVDgRBADBKr3Bl6PO8BQ0H8sJoD6p9U7Yyl7pjtZqioviPwXP+DCWd4u8\n\
HQzcxAZ57m8ssA1LK1Fx93coJhDzM130+p5BG9mYSWShLabR3N1KXdXQYYcowTOM\n\
GxdwYRGr1Spw8QydLhjVfU1VSl4xt6bupPbWJbyjkg5Z3P7BlUOUJmrx3wCgobNV\n\
EDGaWYJcch5z5B1of/41G8kEAKii6q7Gu/vhXXnLS6m15oNnPVybyngiw/23dKjS\n\
ZVG7rKANEK2mxg1VB+vc/uUc4k49UxJJfCZg1gu1sPFV3GSa+Y/7jsiLktQvCiLP\n\
lncQt1dV+ENmHR5BdIDPWDzKBVbgWnSDnqQ6KrZ7T6AlZ74VMpjGxxkWU6vV2xsW\n\
XCLPA/9P/vtImA8CZN3jxGgtK5GGtDNJ/cMhhuv5tnfwFg4b/VGo2Jr8mhLUqoIb\n\
E6zeGAmZbUpdckDco8D5fiFmqTf5+++pCEpJLJkkzel/32N2w4qzPrcRMCiBURES\n\
PjCLd4Y5rPoU8E4kOHc/4BuHN903tiCsCPloCrWsQZ7UdxfQ5LQiUmVkIEhhdCwg\n\
SW5jIDxzZWN1cml0eUByZWRoYXQuY29tPohVBBMRAgAVBQI36lQ4AwsKAwMVAwID\n\
FgIBAheAAAoJECGRgM3bQqYOsBQAnRVtg7B25Hm11PHcpa8FpeddKiq2AJ9aO8sB\n\
XmLDmPOEFI75mpTrKYHF6rkCDQQ36lRyEAgAokgI2xJ+3bZsk8jRA8ORIX8DH05U\n\
lMH27qFYzLbT6npXwXYIOtVn0K2/iMDj+oEB1Aa2au4OnddYaLWp06v3d+XyS0t+\n\
5ab2ZfIQzdh7wCwxqRkzR+/H5TLYbMG+hvtTdylfqIX0WEfoOXMtWEGSVwyUsnM3\n\
Jy3LOi48rQQSCKtCAUdV20FoIGWhwnb/gHU1BnmES6UdQujFBE6EANqPhp0coYoI\n\
hHJ2oIO8ujQItvvNaU88j/s/izQv5e7MXOgVSjKe/WX3s2JtB/tW7utpy12wh1J+\n\
JsFdbLV/t8CozUTpJgx5mVA3RKlxjTA+On+1IEUWioB+iVfT7Ov/0kcAzwADBQf9\n\
E4SKCWRand8K0XloMYgmipxMhJNnWDMLkokvbMNTUoNpSfRoQJ9EheXDxwMpTPwK\n\
ti/PYrrL2J11P2ed0x7zm8v3gLrY0cue1iSba+8glY+p31ZPOr5ogaJw7ZARgoS8\n\
BwjyRymXQp+8Dete0TELKOL2/itDOPGHW07SsVWOR6cmX4VlRRcWB5KejaNvdrE5\n\
4XFtOd04NMgWI63uqZc4zkRa+kwEZtmbz3tHSdRCCE+Y7YVP6IUf/w6YPQFQriWY\n\
FiA6fD10eB+BlIUqIw80VgjsBKmCwvKkn4jg8kibXgj4/TzQSx77uYokw1EqQ2wk\n\
OZoaEtcubsNMquuLCMWijYhGBBgRAgAGBQI36lRyAAoJECGRgM3bQqYOhyYAnj7h\n\
VDY/FJAGqmtZpwVp9IlitW5tAJ4xQApr/jNFZCTksnI+4O1765F7tA==\n\
";

/* This is the unarmored RPM-PGP-KEY public key. */
const char * redhatPubKeyRSA = "\
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\n\
z15FuA==\n\
";

struct pgpValTbl_s pgpSigTypeTbl[] = {
    { PGPSIGTYPE_BINARY,	"Signature of a binary document" },
    { PGPSIGTYPE_TEXT,		"Signature of a canonical text document" },
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
    { PGPSYMKEYALGO_TRIPLE_DES,	"Triple-DES" },
    { PGPSYMKEYALGO_CAST5,	"CAST5" },
    { PGPSYMKEYALGO_BLOWFISH,	"BLOWFISH" },
    { PGPSYMKEYALGO_SAFER,	"SAFER" },
    { PGPSYMKEYALGO_DES_SK,	"DES/SK" },
    { PGPSYMKEYALGO_AES_128,	"AES(128-bit key)" },
    { PGPSYMKEYALGO_AES_192,	"AES(192-bit key)" },
    { PGPSYMKEYALGO_AES_256,	"AES(256-bit key)" },
    { PGPSYMKEYALGO_TWOFISH,	"TWOFISH" },
    { -1,			"Unknown symmetric key algorithm" },
};

struct pgpValTbl_s pgpCompressionTbl[] = {
    { PGPCOMPRESSALGO_NONE,	"Uncompressed" },
    { PGPCOMPRESSALGO_ZIP,	"ZIP" },
    { PGPCOMPRESSALGO_ZLIB, 	"ZLIB" },
    { -1,			"Unknown compression algorithm" },
};

struct pgpValTbl_s pgpHashTbl[] = {
    { PGPHASHALGO_MD5,		"MD5" },
    { PGPHASHALGO_SHA1,		"SHA1" },
    { PGPHASHALGO_RIPEMD160,	"RIPEMD160" },
    { PGPHASHALGO_MD2,		"MD2" },
    { PGPHASHALGO_TIGER192,	"TIGER192" },
    { PGPHASHALGO_HAVAL_5_160,	"HAVAL-5-160" },
    { -1,			"Unknown hash algorithm" },
};

/*@-exportlocal -exportheadervar@*/
struct pgpValTbl_s pgpKeyServerPrefsTbl[] = {
    { 0x80,			"No-modify" },
    { -1,			"Unknown key server preference" },
};
/*@=exportlocal =exportheadervar@*/

struct pgpValTbl_s pgpSubTypeTbl[] = {
    { PGPSUBTYPE_SIG_CREATE_TIME,"signature creation time" },
    { PGPSUBTYPE_SIG_EXPIRE_TIME,"signature expiration time" },
    { PGPSUBTYPE_EXPORTABLE_CERT,"exportable certification" },
    { PGPSUBTYPE_TRUST_SIG,	"trust signature" },
    { PGPSUBTYPE_REGEX,		"regular expression" },
    { PGPSUBTYPE_REVOCABLE,	"revocable" },
    { PGPSUBTYPE_KEY_EXPIRE_TIME,"key expiration time" },
    { PGPSUBTYPE_BACKWARD_COMPAT,"placeholder for backward compatibility" },
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

struct pgpValTbl_s pgpPktTbl[] = {
    { PGPPKT_PUBLIC_SESSION_KEY,"Public-Key Encrypted Session Key" },
    { PGPPKT_SIGNATURE,		"Signature" },
    { PGPPKT_SYMMETRIC_SESSION_KEY,"Symmetric-Key Encrypted Session Key" },
    { PGPPKT_ONEPASS_SIGNATURE,	"One-Pass Signature" },
    { PGPPKT_SECRET_KEY,	"Secret Key" },
    { PGPPKT_PUBLIC_KEY,	"Public Key" },
    { PGPPKT_SECRET_SUBKEY,	"Secret Subkey" },
    { PGPPKT_COMPRESSED_DATA,	"Compressed Data" },
    { PGPPKT_SYMMETRIC_DATA,	"Symmetrically Encrypted Data" },
    { PGPPKT_MARKER,		"Marker" },
    { PGPPKT_LITERAL_DATA,	"Literal Data" },
    { PGPPKT_TRUST,		"Trust" },
    { PGPPKT_USER_ID,		"User ID" },
    { PGPPKT_PUBLIC_SUBKEY,	"Public Subkey" },
    { PGPPKT_PRIVATE_60,	"Private #60" },
    { PGPPKT_PRIVATE_61,	"Private #61" },
    { PGPPKT_PRIVATE_62,	"Private #62" },
    { PGPPKT_PRIVATE_63,	"Private #63" },
    { -1,			"Unknown packet tag" },
};
/*@=readonlytrans@*/

void pgpPrtVal(const char * pre, pgpValTbl vs, byte val)
{
    if (pre && *pre)
	fprintf(stderr, "%s", pre);
    fprintf(stderr, "%s(%u)", pgpValStr(vs, val), (unsigned)val);
}


int pgpPrtPktSigV3(pgpPkt pkt, const byte *h, unsigned int hlen)
{
    pgpPktSigV3 v = (pgpPktSigV3)h;
    byte *p;
    unsigned plen;
    time_t t;
    int i;

    pgpPrtVal("", pgpPktTbl, pkt);
    if (v->version != 3) {
	fprintf(stderr, " version(%u) != 3\n", (unsigned)v->version);
	return 1;
    }
    if (v->hashlen != 5) {
	fprintf(stderr, " hashlen(%u) != 5\n", (unsigned)v->hashlen);
	return 1;
    }
    pgpPrtVal(" ", pgpPubkeyTbl, v->pubkey_algo);
    pgpPrtVal(" ", pgpHashTbl, v->hash_algo);

    pgpPrtVal(" ", pgpSigTypeTbl, v->sigtype);

    t = pgpGrab(v->time, sizeof(v->time));
fprintf(stderr, " time %08x %-24.24s", (unsigned)t, ctime(&t));
fprintf(stderr, " signer keyid %02x%02x%02x%02x%02x%02x%02x%02x",
	(unsigned)v->signer[0], (unsigned)v->signer[1],
	(unsigned)v->signer[2], (unsigned)v->signer[3],
	(unsigned)v->signer[4], (unsigned)v->signer[5],
	(unsigned)v->signer[6], (unsigned)v->signer[7]);
    plen = pgpGrab(v->signhash16, sizeof(v->signhash16));
fprintf(stderr, " signhash16 %04x", plen);
fprintf(stderr, "\n");

    p = &v->data[0];
    for (i = 0; p < &h[hlen]; i++, p += pgpMpiLen(p))
	fprintf(stderr, "%7d %s\n", i, pgpMpiStr(p));

    return 0;
}

int pgpPrtSubType(const byte *h, unsigned int hlen)
{
    const byte *p = h;
    unsigned plen;
    int i;

    while (hlen > 0) {
	if (*p < 192) {
	    plen = *p++;
	    hlen -= 1;
	} else if (*p < 255) {
	    plen = ((p[0] - 192) << 8) + p[1] + 192;
	    p += 2;
	    hlen -= 2;
	} else {
	    p++;
	    plen = pgpGrab(p, 4);
	    p += 4;
	    hlen -= 5;
	}
	pgpPrtVal("    ", pgpSubTypeTbl, p[0]);
	switch (*p) {
	case PGPSUBTYPE_PREFER_SYMKEY:	/* preferred symmetric algorithms */
	    for (i = 1; i < plen; i++)
		pgpPrtVal(" ", pgpSymkeyTbl, p[i]);
	    fprintf(stderr, "\n");
	    break;
	case PGPSUBTYPE_PREFER_HASH:	/* preferred hash algorithms */
	    for (i = 1; i < plen; i++)
		pgpPrtVal(" ", pgpHashTbl, p[i]);
	    fprintf(stderr, "\n");
	    break;
	case PGPSUBTYPE_PREFER_COMPRESS:/* preferred compression algorithms */
	    for (i = 1; i < plen; i++)
		pgpPrtVal(" ", pgpCompressionTbl, p[i]);
	    fprintf(stderr, "\n");
	    break;
	case PGPSUBTYPE_KEYSERVER_PREFERS:/* key server preferences */
	    for (i = 1; i < plen; i++)
		pgpPrtVal(" ", pgpKeyServerPrefsTbl, p[i]);
	    fprintf(stderr, "\n");
	    break;
	case PGPSUBTYPE_SIG_CREATE_TIME:
	case PGPSUBTYPE_SIG_EXPIRE_TIME:
	case PGPSUBTYPE_KEY_EXPIRE_TIME:
	    fprintf(stderr, " %s", pgpHexStr(p+1, plen-1));
	    if ((plen - 1) == 4) {
		time_t t = pgpGrab(p+1, plen-1);
		fprintf(stderr, " %-24.24s", ctime(&t));
	    }
	    fprintf(stderr, "\n");
	    break;

	case PGPSUBTYPE_ISSUER_KEYID:	/* issuer key ID */
	case PGPSUBTYPE_EXPORTABLE_CERT:
	case PGPSUBTYPE_TRUST_SIG:
	case PGPSUBTYPE_REGEX:
	case PGPSUBTYPE_REVOCABLE:
	case PGPSUBTYPE_BACKWARD_COMPAT:
	case PGPSUBTYPE_REVOKE_KEY:
	case PGPSUBTYPE_NOTATION:
	case PGPSUBTYPE_PREFER_KEYSERVER:
	case PGPSUBTYPE_PRIMARY_USERID:
	case PGPSUBTYPE_POLICY_URL:
	case PGPSUBTYPE_KEY_FLAGS:
	case PGPSUBTYPE_SIGNER_USERID:
	case PGPSUBTYPE_REVOKE_REASON:
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
	    fprintf(stderr, " %s", pgpHexStr(p+1, plen-1));
	    fprintf(stderr, "\n");
	    break;
	}
	p += plen;
	hlen -= plen;
    }
    return 0;
}

int pgpPrtPktSigV4(pgpPkt pkt, const byte *h, unsigned int hlen)
{
    pgpPktSigV4 v = (pgpPktSigV4)h;
    byte * p;
    unsigned plen;
    int i;

    pgpPrtVal("", pgpPktTbl, pkt);
    if (v->version != 4) {
	fprintf(stderr, " version(%u) != 4\n", (unsigned)v->version);
	return 1;
    }
    pgpPrtVal(" ", pgpPubkeyTbl, v->pubkey_algo);
    pgpPrtVal(" ", pgpHashTbl, v->hash_algo);

    pgpPrtVal(" ", pgpSigTypeTbl, v->sigtype);
fprintf(stderr, "\n");

    p = &v->hashlen[0];
    plen = pgpGrab(v->hashlen, sizeof(v->hashlen));
    p += 2;
fprintf(stderr, "   hash[%u] -- %s\n", plen, pgpHexStr(p, plen));
    (void) pgpPrtSubType(p, plen);
    p += plen;
    plen = pgpGrab(p,2);
    p += 2;
fprintf(stderr, " unhash[%u] -- %s\n", plen, pgpHexStr(p, plen));
    (void) pgpPrtSubType(p, plen);
    p += plen;
    plen = pgpGrab(p,2);
    p += 2;
fprintf(stderr, " signhash16 %04x\n", plen);

    for (i = 0; p < &h[hlen]; i++, p += pgpMpiLen(p))
	fprintf(stderr, "%7d %s\n", i, pgpMpiStr(p));

    return 0;
}

int pgpPrtPktSig(pgpPkt pkt, const byte *h, unsigned int hlen)
{
    byte version = *h;
    switch (version) {
    case 3:
	(void) pgpPrtPktSigV3(pkt, h, hlen);
	break;
    case 4:
	(void) pgpPrtPktSigV4(pkt, h, hlen);
	break;
    }
    return 0;
}

int pgpPrtKeyV3(pgpPkt pkt, const byte *h, unsigned int hlen)
{
    pgpPktKeyV3 v = (pgpPktKeyV3)h;
    byte * p;
    unsigned plen;
    time_t t;
    int i;

    pgpPrtVal("", pgpPktTbl, pkt);
    if (v->version != 3) {
	fprintf(stderr, " version(%u) != 3\n", (unsigned)v->version);
	return 1;
    }
    t = pgpGrab(v->time, sizeof(v->time));
fprintf(stderr, " time %08x %-24.24s", (unsigned)t, ctime(&t));
    pgpPrtVal(" ", pgpPubkeyTbl, v->pubkey_algo);

    plen = pgpGrab(v->valid, sizeof(v->valid));
    if (plen != 0)
	fprintf(stderr, " valid %u days", plen);

fprintf(stderr, "\n");

    p = &v->data[0];
    for (i = 0; p < &h[hlen]; i++, p += pgpMpiLen(p))
	fprintf(stderr, "%7d %s\n", i, pgpMpiStr(p));

    return 0;
}

int pgpPrtKeyV4(pgpPkt pkt, const byte *h, unsigned int hlen)
{
    pgpPktKeyV4 v = (pgpPktKeyV4)h;
    byte * p;
    time_t t;
    int i;

    pgpPrtVal("", pgpPktTbl, pkt);
    if (v->version != 4) {
	fprintf(stderr, " version(%u) != 4\n", (unsigned)v->version);
	return 1;
    }
    t = pgpGrab(v->time, sizeof(v->time));
fprintf(stderr, " time %08x %-24.24s", (unsigned)t, ctime(&t));
    pgpPrtVal(" ", pgpPubkeyTbl, v->pubkey_algo);
fprintf(stderr, "\n");

    p = &v->data[0];
    for (i = 0; p < &h[hlen]; i++, p += pgpMpiLen(p))
	fprintf(stderr, "%7d %s\n", i, pgpMpiStr(p));

    return 0;
}

int pgpPrtKey(pgpPkt pkt, const byte *h, unsigned int hlen)
{
    byte version = *h;
    switch (version) {
    case 3:
	(void) pgpPrtKeyV3(pkt, h, hlen);
	break;
    case 4:
	(void) pgpPrtKeyV4(pkt, h, hlen);
	break;
    }
    return 0;
}

int pgpPrtUserID(pgpPkt pkt, const byte *h, unsigned int hlen)
{
    pgpPrtVal("", pgpPktTbl, pkt);
fprintf(stderr, " \"%*s\"\n", (int)hlen, (const char *)h);
    return 0;
}

int pgpPrtPkt(const byte *p)
{
    unsigned int val = *p;
    unsigned int mark = (val >> 7) & 0x1;
    unsigned int nmark = (val >> 6) & 0x1;
    pgpPkt pkt = (val >> 2) & 0xf;
    unsigned int plen = (1 << (val & 0x3));
    const byte *h;
    unsigned int hlen = 0;
    unsigned int i;

    /* XXX can't deal with these. */
    if (!mark || nmark || plen > 8)
	return -1;

    for (i = 1; i <= plen; i++)
	hlen = (hlen << 8) | p[i];

    h = p + plen + 1;
    switch (pkt) {
    case PGPPKT_SIGNATURE:
	(void) pgpPrtPktSig(pkt, h, hlen);
	break;
    case PGPPKT_PUBLIC_KEY:
    case PGPPKT_PUBLIC_SUBKEY:
    case PGPPKT_SECRET_KEY:
    case PGPPKT_SECRET_SUBKEY:
	(void) pgpPrtKey(pkt, h, hlen);
	break;
    case PGPPKT_USER_ID:
	(void) pgpPrtUserID(pkt, h, hlen);
	break;
    case PGPPKT_RESERVED:
    case PGPPKT_PUBLIC_SESSION_KEY:
    case PGPPKT_SYMMETRIC_SESSION_KEY:
    case PGPPKT_COMPRESSED_DATA:
    case PGPPKT_SYMMETRIC_DATA:
    case PGPPKT_MARKER:
    case PGPPKT_LITERAL_DATA:
    case PGPPKT_TRUST:
    case PGPPKT_PRIVATE_60:
    case PGPPKT_PRIVATE_61:
    case PGPPKT_PRIVATE_62:
    case PGPPKT_PRIVATE_63:
    default:
	pgpPrtVal("", pgpPktTbl, pkt);
	fprintf(stderr, " plen %02x hlen %x\n", plen, hlen);
	break;
    }

    return plen+hlen+1;
}
