/** \ingroup rpmio signature
 * \file rpmio/tkey.c
 */

static int _debug = 0;

#include "system.h"
#include "rpmpgp.h"
#include "base64.h"
#include "debug.h"

#include <stdio.h>

static inline int grab(const byte *s, int nbytes)
{
    int i = 0;
    int nb = (nbytes <= sizeof(i) ? nbytes : sizeof(i));
    while (nb--)
	i = (i << 8) | *s++;
    return i;
}

static inline int mpi_nbits(const byte *p) { return ((p[0] << 8) | p[1]); }
static inline int mpi_len(const byte *p) { return (2 + ((mpi_nbits(p)+7)>>3)); }
	
static char * pr_pfmt(char *t, const byte *s, int nbytes)
{
    static char hex[] = "0123456789abcdef";
    while (nbytes-- > 0) {
	*t++ = hex[ (*s >> 4) & 0xf ];
	*t++ = hex[ (*s++   ) & 0xf ];
    }
    *t = '\0';
    return t;
}

static char prbuf[2048];

static char * pr_hex(const byte *p, unsigned plen)
{
    char *t = prbuf;

    t = pr_pfmt(t, p, plen);
    return prbuf;
}

static const char * pr_mpi(const byte *p)
{
    char *t = prbuf;

    sprintf(t, "[%d]: ", grab(p, 2));
    t += strlen(t);
    t = pr_pfmt(t, p+2, mpi_len(p)-2);
    return prbuf;
}

static const char * valstr(pgpValStr vs, byte val)
{
    do {
	if (vs->val == val)
	    break;
    } while ((++vs)->val != -1);
    return vs->str;
}

static void pr_valstr(const char * pre, pgpValStr vs, byte val)
{
    if (pre && *pre)
	fprintf(stderr, "%s", pre);
    fprintf(stderr, "%s(%d)", valstr(vs, val), val);
}

static struct pgpValStr_s sigtypeVals[] = {
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

static struct pgpValStr_s pubkeyVals[] = {
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

static struct pgpValStr_s symkeyVals[] = {
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

static struct pgpValStr_s compressionVals[] = {
    { PGPCOMPRESSALGO_NONE,	"Uncompressed" },
    { PGPCOMPRESSALGO_ZIP,	"ZIP" },
    { PGPCOMPRESSALGO_ZLIB, 	"ZLIB" },
    { -1,			"Unknown compression algorithm" },
};

static struct pgpValStr_s hashVals[] = {
    { PGPHASHALGO_MD5,		"MD5" },
    { PGPHASHALGO_SHA1,		"SHA1" },
    { PGPHASHALGO_RIPEMD160,	"RIPEMD160" },
    { PGPHASHALGO_MD2,		"MD2" },
    { PGPHASHALGO_TIGER192,	"TIGER192" },
    { PGPHASHALGO_HAVAL_5_160,	"HAVAL-5-160" },
    { -1,			"Unknown hash algorithm" },
};

static struct pgpValStr_s keyservPrefVals[] = {
    { 0x80,			"No-modify" },
    { -1,			"Unknown key server preference" },
};

static struct pgpValStr_s subtypeVals[] = {
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

static struct pgpValStr_s keypktVals[] = {
    { PGPKEYPKT_PUBLIC_SESSION_KEY,"Public-Key Encrypted Session Key" },
    { PGPKEYPKT_SIGNATURE,	"Signature" },
    { PGPKEYPKT_SYMMETRIC_SESSION_KEY,"Symmetric-Key Encrypted Session Key" },
    { PGPKEYPKT_ONEPASS_SIGNATURE,	"One-Pass Signature" },
    { PGPKEYPKT_SECRET_KEY,	"Secret Key" },
    { PGPKEYPKT_PUBLIC_KEY,	"Public Key" },
    { PGPKEYPKT_SECRET_SUBKEY,	"Secret Subkey" },
    { PGPKEYPKT_COMPRESSED_DATA,"Compressed Data" },
    { PGPKEYPKT_SYMMETRIC_DATA,	"Symmetrically Encrypted Data" },
    { PGPKEYPKT_MARKER,	       "Marker" },
    { PGPKEYPKT_LITERAL_DATA,	"Literal Data" },
    { PGPKEYPKT_TRUST,		"Trust" },
    { PGPKEYPKT_USER_ID,	"User ID" },
    { PGPKEYPKT_PUBLIC_SUBKEY,	"Public Subkey" },
    { PGPKEYPKT_PRIVATE_60,	"Private #60" },
    { PGPKEYPKT_PRIVATE_61,	"Private #61" },
    { PGPKEYPKT_PRIVATE_62,	"Private #62" },
    { PGPKEYPKT_PRIVATE_63,	"Private #63" },
    { -1,			"Unknown packet tag" },
};

static int pr_signature_v3(pgpKeyPkt keypkt, const byte *h, unsigned hlen)
{
    pgpSigPktV3 v = (pgpSigPktV3)h;
    byte *p;
    unsigned plen;
    time_t t;
    int i;

    pr_valstr("", keypktVals, keypkt);
    if (v->version != 3) {
	fprintf(stderr, " version(%d) != 3\n", v->version);
	return 1;
    }
    if (v->hashlen != 5) {
	fprintf(stderr, " hashlen(%d) != 5\n", v->hashlen);
	return 1;
    }
    pr_valstr(" ", pubkeyVals, v->pubkey_algo);
    pr_valstr(" ", hashVals, v->hash_algo);

    pr_valstr(" ", sigtypeVals, v->sigtype);

    t = grab(v->time, sizeof(v->time));
fprintf(stderr, " time %08x %-24.24s", (unsigned)t, ctime(&t));
fprintf(stderr, " signer keyid %02x%02x%02x%02x%02x%02x%02x%02x",
	v->signer[0], v->signer[1], v->signer[2], v->signer[3],
	v->signer[4], v->signer[5], v->signer[6], v->signer[7]);
    plen = grab(v->signhash16, sizeof(v->signhash16));
fprintf(stderr, " signhash16 %04x", plen);
fprintf(stderr, "\n");

    p = &v->data[0];
    for (i = 0; p < &h[hlen]; i++, p += mpi_len(p))
	fprintf(stderr, "%7d %s\n", i, pr_mpi(p));

    return 0;
}

static int pr_sigsubkeys(const byte *h, unsigned hlen)
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
	    plen = grab(p, 4);
	    p += 4;
	    hlen -= 5;
	}
	pr_valstr("    ", subtypeVals, p[0]);
	switch (*p) {
	case PGPSUBTYPE_PREFER_SYMKEY:	/* preferred symmetric algorithms */
	    for (i = 1; i < plen; i++)
		pr_valstr(" ", symkeyVals, p[i]);
	    fprintf(stderr, "\n");
	    break;
	case PGPSUBTYPE_PREFER_HASH:	/* preferred hash algorithms */
	    for (i = 1; i < plen; i++)
		pr_valstr(" ", hashVals, p[i]);
	    fprintf(stderr, "\n");
	    break;
	case PGPSUBTYPE_PREFER_COMPRESS:/* preferred compression algorithms */
	    for (i = 1; i < plen; i++)
		pr_valstr(" ", compressionVals, p[i]);
	    fprintf(stderr, "\n");
	    break;
	case PGPSUBTYPE_KEYSERVER_PREFERS:/* key server preferences */
	    for (i = 1; i < plen; i++)
		pr_valstr(" ", keyservPrefVals, p[i]);
	    fprintf(stderr, "\n");
	    break;
	case PGPSUBTYPE_SIG_CREATE_TIME:
	case PGPSUBTYPE_SIG_EXPIRE_TIME:
	case PGPSUBTYPE_KEY_EXPIRE_TIME:
	    fprintf(stderr, " %s", pr_hex(p+1, plen-1));
	    if ((plen - 1) == 4) {
		time_t t = grab(p+1, plen-1);
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
	    fprintf(stderr, " %s", pr_hex(p+1, plen-1));
	    fprintf(stderr, "\n");
	    break;
	}
	p += plen;
	hlen -= plen;
    }
    return 0;
}

static int pr_signature_v4(pgpKeyPkt keypkt, const byte *h, unsigned hlen)
{
    pgpSigPktV4 v = (pgpSigPktV4)h;
    byte * p;
    unsigned plen;
    int i;

    pr_valstr("", keypktVals, keypkt);
    if (v->version != 4) {
	fprintf(stderr, " version(%d) != 4\n", v->version);
	return 1;
    }
    pr_valstr(" ", pubkeyVals, v->pubkey_algo);
    pr_valstr(" ", hashVals, v->hash_algo);

    pr_valstr(" ", sigtypeVals, v->sigtype);
fprintf(stderr, "\n");

    p = &v->hashlen[0];
    plen = grab(v->hashlen, sizeof(v->hashlen));
    p += 2;
fprintf(stderr, "   hash[%d] -- %s\n", plen, pr_hex(p, plen));
    pr_sigsubkeys(p, plen);
    p += plen;
    plen = grab(p,2);
    p += 2;
fprintf(stderr, " unhash[%d] -- %s\n", plen, pr_hex(p, plen));
    pr_sigsubkeys(p, plen);
    p += plen;
    plen = grab(p,2);
    p += 2;
fprintf(stderr, " signhash16 %04x\n", plen);

    for (i = 0; p < &h[hlen]; i++, p += mpi_len(p))
	fprintf(stderr, "%7d %s\n", i, pr_mpi(p));

    return 0;
}

static int pr_signature(pgpKeyPkt keypkt, const byte *h, unsigned hlen)
{
    byte version = *h;
    switch (version) {
    case 3:
	pr_signature_v3(keypkt, h, hlen);
	break;
    case 4:
	pr_signature_v4(keypkt, h, hlen);
	break;
    }
    return 0;
}

static int pr_key_v3(pgpKeyPkt keypkt, const byte *h, unsigned hlen)
{
    pgpKeyV3 v = (pgpKeyV3)h;
    byte * p;
    unsigned plen;
    time_t t;
    int i;

    pr_valstr("", keypktVals, keypkt);
    if (v->version != 3) {
	fprintf(stderr, " version(%d) != 3\n", v->version);
	return 1;
    }
    plen = grab(v->time, sizeof(v->time));
fprintf(stderr, " time %08x %-24.24s", (unsigned)t, ctime(&t));
    pr_valstr(" ", pubkeyVals, v->pubkey_algo);

    plen = grab(v->valid, sizeof(v->valid));
    if (plen != 0)
	fprintf(stderr, " valid %d days", plen);

fprintf(stderr, "\n");

    p = &v->data[0];
    for (i = 0; p < &h[hlen]; i++, p += mpi_len(p))
	fprintf(stderr, "%7d %s\n", i, pr_mpi(p));

    return 0;
}

static int pr_key_v4(pgpKeyPkt keypkt, const byte *h, unsigned hlen)
{
    pgpKeyV4 v = (pgpKeyV4)h;
    byte * p;
    time_t t;
    int i;

    pr_valstr("", keypktVals, keypkt);
    if (v->version != 4) {
	fprintf(stderr, " version(%d) != 4\n", v->version);
	return 1;
    }
    t = grab(v->time, sizeof(v->time));
fprintf(stderr, " time %08x %-24.24s", (unsigned)t, ctime(&t));
    pr_valstr(" ", pubkeyVals, v->pubkey_algo);
fprintf(stderr, "\n");

    p = &v->data[0];
    for (i = 0; p < &h[hlen]; i++, p += mpi_len(p))
	fprintf(stderr, "%7d %s\n", i, pr_mpi(p));

    return 0;
}

static int pr_key(pgpKeyPkt keypkt, const byte *h, unsigned hlen)
{
    byte version = *h;
    switch (version) {
    case 3:
	pr_key_v3(keypkt, h, hlen);
	break;
    case 4:
	pr_key_v4(keypkt, h, hlen);
	break;
    }
    return 0;
}

static int pr_user_id(pgpKeyPkt keypkt, const byte *h, unsigned hlen)
{
    pr_valstr("", keypktVals, keypkt);
fprintf(stderr, " \"%*s\"\n", hlen, h);
    return 0;
}

static int pr_keypkt(const byte *p)
{
    unsigned int val = *p;
    unsigned int mark = (val >> 7) & 0x1;
    unsigned int new = (val >> 6) & 0x1;
    pgpKeyPkt keypkt = (val >> 2) & 0xf;
    unsigned int plen = (1 << (val & 0x3));
    const byte *h;
    unsigned int hlen = 0;
    unsigned int i;

    /* XXX can't deal with these. */
    if (!mark || new || plen > 8)
	return -1;

    for (i = 1; i <= plen; i++)
	hlen = (hlen << 8) | p[i];

    h = p + plen + 1;
    switch (keypkt) {
    case PGPKEYPKT_SIGNATURE:
	pr_signature(keypkt, h, hlen);
	break;
    case PGPKEYPKT_PUBLIC_KEY:
    case PGPKEYPKT_PUBLIC_SUBKEY:
    case PGPKEYPKT_SECRET_KEY:
    case PGPKEYPKT_SECRET_SUBKEY:
	pr_key(keypkt, h, hlen);
	break;
    case PGPKEYPKT_USER_ID:
	pr_user_id(keypkt, h, hlen);
	break;
    case PGPKEYPKT_RESERVED:
    case PGPKEYPKT_PUBLIC_SESSION_KEY:
    case PGPKEYPKT_SYMMETRIC_SESSION_KEY:
    case PGPKEYPKT_COMPRESSED_DATA:
    case PGPKEYPKT_SYMMETRIC_DATA:
    case PGPKEYPKT_MARKER:
    case PGPKEYPKT_LITERAL_DATA:
    case PGPKEYPKT_TRUST:
    case PGPKEYPKT_PRIVATE_60:
    case PGPKEYPKT_PRIVATE_61:
    case PGPKEYPKT_PRIVATE_62:
    case PGPKEYPKT_PRIVATE_63:
    default:
	pr_valstr("", keypktVals, keypkt);
	fprintf(stderr, " plen %02x hlen %x\n", plen, hlen);
	break;
    }

    return plen+hlen+1;
}

/* This is the unarmored RPM-GPG-KEY public key. */
const char * gpgsig = "\
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
const char * pgpsig = "\
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\n\
z15FuA==\n\
";

static int doit(const char *sig)
{
    const char *s, *t;
    unsigned char * dec;
    unsigned char * d;
    size_t declen;
    char * enc;
    int rc;
    int len = 0;
    int i;

if (_debug)
fprintf(stderr, "*** sig is\n%s\n", sig);

    if ((rc = b64decode(sig, (void **)&dec, &declen)) != 0) {
	fprintf(stderr, "*** B64decode returns %d\n", rc);
	exit(rc);
    }

    for (d = dec; d < (dec + declen); d += len) {
	len = pr_keypkt(d);
	if (len <= 0)
	    exit(len);
    }

    if ((enc = b64encode(dec, declen)) == NULL) {
	fprintf(stderr, "*** B64encode returns %d\n", rc);
	exit(4);
    }

if (_debug)
fprintf(stderr, "*** enc is\n%s\n", enc);

rc = 0;
for (i = 0, s = sig, t = enc; *s & *t; i++, s++, t++) {
    if (*s == '\n') s++;
    if (*t == '\n') t++;
    if (*s == *t) continue;
fprintf(stderr, "??? %5d %02x != %02x '%c' != '%c'\n", i, (*s & 0xff), (*t & 0xff), *s, *t);
    rc = 5;
}

    return rc;
}

int
main (int argc, char *argv[])
{
    int rc;

fprintf(stderr, "============================================== RPM-GPG-KEY\n");
    if ((rc = doit(gpgsig)) != 0)
	return rc;

fprintf(stderr, "============================================== RPM-PGP-KEY\n");
    if ((rc = doit(pgpsig)) != 0)
	return rc;

    return rc;
}
