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

typedef struct {
    byte nbits[2];
    byte bits[1];
} MPI_t;

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

static const char * pr_sigtype(pgpSigType sigtype) {
    switch (sigtype) {
    case PGPSIGTYPE_BINARY:
	return("Signature of a binary document");
	break;
    case PGPSIGTYPE_TEXT:
	return("Signature of a canonical text document");
	break;
    case PGPSIGTYPE_STANDALONE:
	return("Standalone signature");
	break;
    case PGPSIGTYPE_GENERIC_CERT:
	return("Generic certification of a User ID and Public Key");
	break;
    case PGPSIGTYPE_PERSONA_CERT:
	return("Persona certification of a User ID and Public Key");
	break;
    case PGPSIGTYPE_CASUAL_CERT:
	return("Casual certification of a User ID and Public Key");
	break;
    case PGPSIGTYPE_POSITIVE_CERT:
	return("Positive certification of a User ID and Public Key");
	break;
    case PGPSIGTYPE_SUBKEY_BINDING:
	return("Subkey Binding Signature");
	break;
    case PGPSIGTYPE_SIGNED_KEY:
	return("Signature directly on a key");
	break;
    case PGPSIGTYPE_KEY_REVOKE:
	return("Key revocation signature");
	break;
    case PGPSIGTYPE_SUBKEY_REVOKE:
	return("Subkey revocation signature");
	break;
    case PGPSIGTYPE_CERT_REVOKE:
	return("Certification revocation signature");
	break;
    case PGPSIGTYPE_TIMESTAMP:
	return("Timestamp signature");
	break;
    }
    return "Unknown signature type";
}

static const char * pr_pubkey_algo(byte pubkey_algo) {
    switch (pubkey_algo) {
    case PGPPUBKEYALGO_RSA:
	return("RSA");
	break;
    case PGPPUBKEYALGO_RSA_ENCRYPT:
	return("RSA(Encrypt-Only)");
	break;
    case PGPPUBKEYALGO_RSA_SIGN :
	return("RSA(Sign-Only)");
	break;
    case PGPPUBKEYALGO_ELGAMAL_ENCRYPT:
	return("Elgamal(Encrypt-Only)");
	break;
    case PGPPUBKEYALGO_DSA:
	return("DSA");
	break;
    case PGPPUBKEYALGO_EC:
	return("Elliptic Curve");
	break;
    case PGPPUBKEYALGO_ECDSA:
	return("ECDSA");
	break;
    case PGPPUBKEYALGO_ELGAMAL:
	return("Elgamal");
	break;
    case PGPPUBKEYALGO_DH:
	return("Diffie-Hellman (X9.42)");
	break;
    }
    return "Unknown public key algorithm";
}

static const char * pr_symkey_algo(byte symkey_algo) {
    switch (symkey_algo) {
    case PGPSYMKEYALGO_PLAINTEXT:
	return("Plaintext");
	break;
    case PGPSYMKEYALGO_IDEA:
	return("IDEA");
	break;
    case PGPSYMKEYALGO_TRIPLE_DES:
	return("Triple-DES");
	break;
    case PGPSYMKEYALGO_CAST5:
	return("CAST5");
	break;
    case PGPSYMKEYALGO_BLOWFISH:
	return("BLOWFISH");
	break;
    case PGPSYMKEYALGO_SAFER:
	return("SAFER");
	break;
    case PGPSYMKEYALGO_DES_SK:
	return("DES/SK");
	break;
    case PGPSYMKEYALGO_AES_128:
	return("AES(128-bit key)");
	break;
    case PGPSYMKEYALGO_AES_192:
	return("AES(192-bit key)");
	break;
    case PGPSYMKEYALGO_AES_256:
	return("AES(256-bit key)");
	break;
    case PGPSYMKEYALGO_TWOFISH:
	return("TWOFISH");
	break;
    }
    return "Unknown symmetric key algorithm";
};

static const char * pr_compression_algo(byte compression_algo) {
    switch (compression_algo) {
    case PGPCOMPRESSALGO_NONE:
	return("Uncompressed");
	break;
    case PGPCOMPRESSALGO_ZIP:
	return("ZIP");
	break;
    case PGPCOMPRESSALGO_ZLIB:
	return("ZLIB");
	break;
    }
    return "Unknown compression algorithm";
};

static const char * pr_hash_algo(byte hash_algo) {
    switch (hash_algo) {
    case PGPHASHALGO_MD5:
	return("MD5");
	break;
    case PGPHASHALGO_SHA1:
	return("SHA1");
	break;
    case PGPHASHALGO_RIPEMD160:
	return("RIPEMD160");
	break;
    case PGPHASHALGO_MD2:
	return("MD2");
	break;
    case PGPHASHALGO_TIGER192:
	return("TIGER192");
	break;
    case PGPHASHALGO_HAVAL_5_160:
	return("HAVAL-5-160");
	break;
    }
    return "Unknown hash algorithm";
}

static const char * pr_keyserv_pref (byte keyserv_pref) {
    switch(keyserv_pref) {
    case 0x80:	return("No-modify"); break;
    }
    return "Unknown key server preference";
};

static const char * pr_sigsubkeytype (byte sigsubkeytype) {
    switch(sigsubkeytype) {
    case PGPSUBTYPE_SIG_CREATE_TIME:
	return("signature creation time");
	break;
    case PGPSUBTYPE_SIG_EXPIRE_TIME:
	return("signature expiration time");
	break;
    case PGPSUBTYPE_EXPORTABLE_CERT:
	return("exportable certification");
	break;
    case PGPSUBTYPE_TRUST_SIG:
	return("trust signature");
	break;
    case PGPSUBTYPE_REGEX:
	return("regular expression");
	break;
    case PGPSUBTYPE_REVOCABLE:
	return("revocable");
	break;
    case PGPSUBTYPE_KEY_EXPIRE_TIME:
	return("key expiration time");
	break;
    case PGPSUBTYPE_BACKWARD_COMPAT:
	return("placeholder for backward compatibility");
	break;
    case PGPSUBTYPE_PREFER_SYMKEY:
	return("preferred symmetric algorithms");
	break;
    case PGPSUBTYPE_REVOKE_KEY:
	return("revocation key");
	break;
    case PGPSUBTYPE_ISSUER_KEYID:
	return("issuer key ID");
	break;
    case PGPSUBTYPE_NOTATION:
	return("notation data");
	break;
    case PGPSUBTYPE_PREFER_HASH:
	return("preferred hash algorithms");
	break;
    case PGPSUBTYPE_PREFER_COMPRESS:
	return("preferred compression algorithms");
	break;
    case PGPSUBTYPE_KEYSERVER_PREFERS:
	return("key server preferences");
	break;
    case PGPSUBTYPE_PREFER_KEYSERVER:
	return("preferred key server");
	break;
    case PGPSUBTYPE_PRIMARY_USERID:
	return("primary user id");
	break;
    case PGPSUBTYPE_POLICY_URL:
	return("policy URL");
	break;
    case PGPSUBTYPE_KEY_FLAGS:
	return("key flags");
	break;
    case PGPSUBTYPE_SIGNER_USERID:
	return("signer's user id");
	break;
    case PGPSUBTYPE_REVOKE_REASON:
	return("reason for revocation");
	break;
    case PGPSUBTYPE_INTERNAL_100:
	return("internal subpkt type 100");
	break;
    case PGPSUBTYPE_INTERNAL_101:
	return("internal subpkt type 101");
	break;
    case PGPSUBTYPE_INTERNAL_102:
	return("internal subpkt type 102");
	break;
    case PGPSUBTYPE_INTERNAL_103:
	return("internal subpkt type 103");
	break;
    case PGPSUBTYPE_INTERNAL_104:
	return("internal subpkt type 104");
	break;
    case PGPSUBTYPE_INTERNAL_105:
	return("internal subpkt type 105");
	break;
    case PGPSUBTYPE_INTERNAL_106:
	return("internal subpkt type 106");
	break;
    case PGPSUBTYPE_INTERNAL_107:
	return("internal subpkt type 107");
	break;
    case PGPSUBTYPE_INTERNAL_108:
	return("internal subpkt type 108");
	break;
    case PGPSUBTYPE_INTERNAL_109:
	return("internal subpkt type 109");
	break;
    case PGPSUBTYPE_INTERNAL_110:
	return("internal subpkt type 110");
	break;
    }
    return "Unknown signature subkey type";
}

const char *ptags[] = {
       "Reserved - a packet tag must not have this value",
       "Public-Key Encrypted Session Key",
       "Signature",
       "Symmetric-Key Encrypted Session Key",
       "One-Pass Signature",
       "Secret Key",
       "Public Key",
       "Secret Subkey",
       "Compressed Data",
       "Symmetrically Encrypted Data",
       "Marker",
       "Literal Data",
       "Trust",
       "User ID",
       "Public Subkey",
	"??? TAG15 ???",
};

static int pr_signature_v3(pgpKeyPkt ptag, const byte *h, unsigned hlen)
{
    pgpSigPktV3 v = (pgpSigPktV3)h;
    byte *p;
    unsigned plen;
    int i;

fprintf(stderr, "%s(%d)", ptags[ptag], ptag);
    if (v->version != 3) {
	fprintf(stderr, " version(%d) != 3\n", v->version);
	return 1;
    }
    if (v->hashlen != 5) {
	fprintf(stderr, " hashlen(%d) != 5\n", v->hashlen);
	return 1;
    }
fprintf(stderr, " %s(%d)", pr_pubkey_algo(v->pubkey_algo), v->pubkey_algo);
fprintf(stderr, " %s(%d)", pr_hash_algo(v->hash_algo), v->hash_algo);

fprintf(stderr, " %s(%d)", pr_sigtype(v->sigtype), v->sigtype);

    plen = grab(v->time, sizeof(v->time));
fprintf(stderr, " time %08x", plen);
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
fprintf(stderr, "    %s(%d)", pr_sigsubkeytype(*p), *p);
	switch (*p) {
	case PGPSUBTYPE_PREFER_SYMKEY:	/* preferred symmetric algorithms */
	    for (i = 1; i < plen; i++)
		fprintf(stderr, " %s(%d)", pr_symkey_algo(p[i]), p[i]);
	    fprintf(stderr, "\n");
	    break;
	case PGPSUBTYPE_PREFER_HASH:	/* preferred hash algorithms */
	    for (i = 1; i < plen; i++)
		fprintf(stderr, " %s(%d)", pr_hash_algo(p[i]), p[i]);
	    fprintf(stderr, "\n");
	    break;
	case PGPSUBTYPE_PREFER_COMPRESS:/* preferred compression algorithms */
	    for (i = 1; i < plen; i++)
		fprintf(stderr, " %s(%d)", pr_compression_algo(p[i]), p[i]);
	    fprintf(stderr, "\n");
	    break;
	case PGPSUBTYPE_KEYSERVER_PREFERS:/* key server preferences */
	    for (i = 1; i < plen; i++)
		fprintf(stderr, " %s(%d)", pr_keyserv_pref(p[i]), p[i]);
	    fprintf(stderr, "\n");
	    break;
	case PGPSUBTYPE_ISSUER_KEYID:	/* issuer key ID */
	case PGPSUBTYPE_SIG_CREATE_TIME:
	case PGPSUBTYPE_SIG_EXPIRE_TIME:
	case PGPSUBTYPE_EXPORTABLE_CERT:
	case PGPSUBTYPE_TRUST_SIG:
	case PGPSUBTYPE_REGEX:
	case PGPSUBTYPE_REVOCABLE:
	case PGPSUBTYPE_KEY_EXPIRE_TIME:
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

static int pr_signature_v4(pgpKeyPkt ptag, const byte *h, unsigned hlen)
{
    pgpSigPktV4 v = (pgpSigPktV4)h;
    byte * p;
    unsigned plen;
    int i;

fprintf(stderr, "%s(%d)", ptags[ptag], ptag);
    if (v->version != 4) {
	fprintf(stderr, " version(%d) != 4\n", v->version);
	return 1;
    }
fprintf(stderr, " %s(%d)", pr_pubkey_algo(v->pubkey_algo), v->pubkey_algo);
fprintf(stderr, " %s(%d)", pr_hash_algo(v->hash_algo), v->hash_algo);

fprintf(stderr, " %s(%d)", pr_sigtype(v->sigtype), v->sigtype);
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

static int pr_signature(pgpKeyPkt ptag, const byte *h, unsigned hlen)
{
    byte version = *h;
    switch (version) {
    case 3:
	pr_signature_v3(ptag, h, hlen);
	break;
    case 4:
	pr_signature_v4(ptag, h, hlen);
	break;
    }
    return 0;
}

static int pr_key_v3(pgpKeyPkt ptag, const byte *h, unsigned hlen)
{
    pgpKeyV3 v = (pgpKeyV3)h;
    byte * p;
    unsigned plen;
    int i;

fprintf(stderr, "%s(%d)", ptags[ptag], ptag);
    if (v->version != 3) {
	fprintf(stderr, " version(%d) != 3\n", v->version);
	return 1;
    }
    plen = grab(v->time, sizeof(v->time));
fprintf(stderr, " time %08x", plen);
fprintf(stderr, " %s(%d)", pr_pubkey_algo(v->pubkey_algo), v->pubkey_algo);

    plen = grab(v->valid, sizeof(v->valid));
    if (plen != 0)
	fprintf(stderr, " valid %d days", plen);

fprintf(stderr, "\n");

    p = &v->data[0];
    for (i = 0; p < &h[hlen]; i++, p += mpi_len(p))
	fprintf(stderr, "%7d %s\n", i, pr_mpi(p));

    return 0;
}

static int pr_key_v4(pgpKeyPkt ptag, const byte *h, unsigned hlen)
{
    pgpKeyV4 v = (pgpKeyV4)h;
    byte * p;
    unsigned plen;
    int i;

fprintf(stderr, "%s(%d)", ptags[ptag], ptag);
    if (v->version != 4) {
	fprintf(stderr, " version(%d) != 4\n", v->version);
	return 1;
    }
    plen = grab(v->time, sizeof(v->time));
fprintf(stderr, " time %08x", plen);
fprintf(stderr, " %s(%d)", pr_pubkey_algo(v->pubkey_algo), v->pubkey_algo);
fprintf(stderr, "\n");

    p = &v->data[0];
    for (i = 0; p < &h[hlen]; i++, p += mpi_len(p))
	fprintf(stderr, "%7d %s\n", i, pr_mpi(p));

    return 0;
}

static int pr_key(pgpKeyPkt ptag, const byte *h, unsigned hlen)
{
    byte version = *h;
    switch (version) {
    case 3:
	pr_key_v3(ptag, h, hlen);
	break;
    case 4:
	pr_key_v4(ptag, h, hlen);
	break;
    }
    return 0;
}

static int pr_user_id(pgpKeyPkt ptag, const byte *h, unsigned hlen)
{
fprintf(stderr, "%s(%d)", ptags[ptag], ptag);
fprintf(stderr, " \"%*s\"\n", hlen, h);
    return 0;
}

static int pr_keypkt(const byte *p)
{
    unsigned int val = *p;
    unsigned int mark = (val >> 7) & 0x1;
    unsigned int new = (val >> 6) & 0x1;
    pgpKeyPkt ptag = (val >> 2) & 0xf;
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
    switch (ptag) {
    case PGPKEYPKT_SIGNATURE:
	pr_signature(ptag, h, hlen);
	break;
    case PGPKEYPKT_PUBLIC_KEY:
    case PGPKEYPKT_PUBLIC_SUBKEY:
    case PGPKEYPKT_SECRET_KEY:
    case PGPKEYPKT_SECRET_SUBKEY:
	pr_key(ptag, h, hlen);
	break;
    case PGPKEYPKT_USER_ID:
	pr_user_id(ptag, h, hlen);
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
	fprintf(stderr, "%s(%d) plen %02x hlen %x\n",
		ptags[ptag], ptag, plen, hlen);
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
