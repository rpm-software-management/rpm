/**
 * \file tests/openpgp.c
 */

static int _debug = 0;

#include "base64.h"

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_TIME_H
# include <time.h>
#endif

#include <stdio.h>

static inline int grab(const byte *s, int nbytes)
{
    int i = 0;
    int nb = (nbytes <= sizeof(i) ? nbytes : sizeof(i));
    while (nb--)
	i = (i << 8) | *s++;
    return i;
}

#define	GRAB(_a)	grab((_a), sizeof(_a))

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

    sprintf(t, "[%d]: ", grab(p,2));
    t += strlen(t);
    t = pr_pfmt(t, p+2, mpi_len(p)-2);
    return prbuf;
}

static const char * pr_sigtype(byte sigtype) {
    switch (sigtype) {
    case 0x00:	return("Signature of a binary document"); break;
    case 0x01:	return("Signature of a canonical text document"); break;
    case 0x02:	return("Standalone signature"); break;
    case 0x10:	return("Generic certification of a User ID and Public Key"); break;
    case 0x11:	return("Persona certification of a User ID and Public Key"); break;
    case 0x12:	return("Casual certification of a User ID and Public Key"); break;
    case 0x13:	return("Positive certification of a User ID and Public Key"); break;
    case 0x18:	return("Subkey Binding Signature"); break;
    case 0x1F:	return("Signature directly on a key"); break;
    case 0x20:	return("Key revocation signature"); break;
    case 0x28:	return("Subkey revocation signature"); break;
    case 0x30:	return("Certification revocation signature"); break;
    case 0x40:	return("Timestamp signature"); break;
    }
    return "Unknown signature type";
}

static const char * pr_pubkey_algo(byte pubkey_algo) {
    switch (pubkey_algo) {
    case 1:	return("RSA");			break;
    case 2:	return("RSA(Encrypt-Only)");	break;
    case 3 :	return("RSA(Sign-Only)");	break;
    case 16:	return("Elgamal(Encrypt-Only)"); break;
    case 17:	return("DSA");			break;
    case 18:	return("Elliptic Curve");	break;
    case 19:	return("ECDSA");		break;
    case 20:	return("Elgamal");		break;
    case 21:	return("Diffie-Hellman (X9.42)"); break;
    }
    return "Unknown public key algorithm";
}

static const char * pr_symkey_algo(byte symkey_algo) {
    switch (symkey_algo) {
    case 0:	return("Plaintext"); break;
    case 1:	return("IDEA"); break;
    case 2:	return("DES-EDE"); break;
    case 3:	return("CAST5"); break;
    case 4:	return("BLOWFISH"); break;
    case 5:	return("SAFER"); break;
    case 10:	return("TWOFISH"); break;
    }
    return "Unknown symmetric key algorithm";
};

static const char * pr_compression_algo(byte compression_algo) {
    switch (compression_algo) {
    case 0:	return("Uncompressed"); break;
    case 1:	return("ZIP"); break;
    case 2:	return("ZLIB"); break;
    }
    return "Unknown compression algorithm";
};

static const char * pr_hash_algo(byte hash_algo) {
    switch (hash_algo) {
    case 1:	return("MD5");		break;
    case 2:	return("SHA1");		break;
    case 3:	return("RIPEMD160");	break;
    case 5:	return("MD2");		break;
    case 6:	return("TIGER192");	break;
    case 7:	return("HAVAL-5-160");	break;
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
    case 2:	return("signature creation time"); break;
    case 3:	return("signature expiration time"); break;
    case 4:	return("exportable certification"); break;
    case 5:	return("trust signature"); break;
    case 6:	return("regular expression"); break;
    case 7:	return("revocable"); break;
    case 9:	return("key expiration time"); break;
    case 10:	return("placeholder for backward compatibility"); break;
    case 11:	return("preferred symmetric algorithms"); break;
    case 12:	return("revocation key"); break;
    case 16:	return("issuer key ID"); break;
    case 20:	return("notation data"); break;
    case 21:	return("preferred hash algorithms"); break;
    case 22:	return("preferred compression algorithms"); break;
    case 23:	return("key server preferences"); break;
    case 24:	return("preferred key server"); break;
    case 25:	return("primary user id"); break;
    case 26:	return("policy URL"); break;
    case 27:	return("key flags"); break;
    case 28:	return("signer's user id"); break;
    case 29:	return("reason for revocation"); break;
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

typedef enum {
    RPMKEYPKT_SIGNATURE		=  2,
    RPMKEYPKT_SECRET_KEY	=  5,
    RPMKEYPKT_PUBLIC_KEY	=  6,
    RPMKEYPKT_SECRET_SUBKEY	=  7,
    RPMKEYPKT_USER_ID		= 13,
    RPMKEYPKT_PUBLIC_SUBKEY	= 14
} rpmKeyPkt;

/*
5.2.2. Version 3 Signature Packet Format
   The body of a version 3 Signature Packet contains:
     - One-octet version number (3).
     - One-octet length of following hashed material.  MUST be 5.
         - One-octet signature type.
         - Four-octet creation time.
     - Eight-octet key ID of signer.
     - One-octet public key algorithm.
     - One-octet hash algorithm.
     - Two-octet field holding left 16 bits of signed hash value.
     - One or more multi-precision integers comprising the signature.
       This portion is algorithm specific, as described below.
*/

struct signature_v3 {
    byte version;	/*!< version number (3). */
    byte hashlen;	/*!< length of following hashed material. MUST be 5. */
    byte sigtype;	/*!< signature type. */
    byte time[4];	/*!< 4 byte creation time. */
    byte signer[8];	/*!< key ID of signer. */
    byte pubkey_algo;	/*!< public key algorithm. */
    byte hash_algo;	/*!< hash algorithm. */
    byte signhash16[2];	/*!< left 16 bits of signed hash value. */
    byte data[1];	/*!< One or more multi-precision integers. */
};

static int pr_signature_v3(rpmKeyPkt ptag, const byte *h, unsigned hlen)
{
    struct signature_v3 *v = (struct signature_v3 *)h;
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

    plen = GRAB(v->time);
fprintf(stderr, " time %08x", plen);
fprintf(stderr, " signer keyid %02x%02x%02x%02x%02x%02x%02x%02x",
	v->signer[0], v->signer[1], v->signer[2], v->signer[3],
	v->signer[4], v->signer[5], v->signer[6], v->signer[7]);
    plen = GRAB(v->signhash16);
fprintf(stderr, " signhash16 %04x", plen);
fprintf(stderr, "\n");

    p = &v->data[0];
    for (i = 0; p < &h[hlen]; i++, p += mpi_len(p))
	fprintf(stderr, "%7d %s\n", i, pr_mpi(p));

    return 0;
}

/*
5.2.3.1. Signature Subpacket Specification

   The subpacket fields consist of zero or more signature subpackets.
   Each set of subpackets is preceded by a two-octet scalar count of the
   length of the set of subpackets.

   Each subpacket consists of a subpacket header and a body.  The header
   consists of:

     - the subpacket length (1,  2, or 5 octets)

     - the subpacket type (1 octet)

   and is followed by the subpacket specific data.

   The length includes the type octet but not this length. Its format is
   similar to the "new" format packet header lengths, but cannot have
   partial body lengths. That is:

       if the 1st octet <  192, then
           lengthOfLength = 1
           subpacketLen = 1st_octet

       if the 1st octet >= 192 and < 255, then
           lengthOfLength = 2
           subpacketLen = ((1st_octet - 192) << 8) + (2nd_octet) + 192

       if the 1st octet = 255, then
           lengthOfLength = 5
           subpacket length = [four-octet scalar starting at 2nd_octet]

*/

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
	case 11:	/* preferred symmetric algorithms */
	    for (i = 1; i < plen; i++)
		fprintf(stderr, " %s(%d)", pr_symkey_algo(p[i]), p[i]);
	    fprintf(stderr, "\n");
	    break;
	case 21:	/* preferred hash algorithms */
	    for (i = 1; i < plen; i++)
		fprintf(stderr, " %s(%d)", pr_hash_algo(p[i]), p[i]);
	    fprintf(stderr, "\n");
	    break;
	case 22:	/* preferred compression algorithms */
	    for (i = 1; i < plen; i++)
		fprintf(stderr, " %s(%d)", pr_compression_algo(p[i]), p[i]);
	    fprintf(stderr, "\n");
	    break;
	case 23:	/* key server preferences */
	    for (i = 1; i < plen; i++)
		fprintf(stderr, " %s(%d)", pr_keyserv_pref(p[i]), p[i]);
	    fprintf(stderr, "\n");
	    break;
	case 16:	/* issuer key ID */
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

/*
5.2.3. Version 4 Signature Packet Format
   The body of a version 4 Signature Packet contains:
     - One-octet version number (4).
     - One-octet signature type.
     - One-octet public key algorithm.
     - One-octet hash algorithm.
     - Two-octet scalar octet count for following hashed subpacket
       data. Note that this is the length in octets of all of the hashed
       subpackets; a pointer incremented by this number will skip over
       the hashed subpackets.
     - Hashed subpacket data. (zero or more subpackets)
     - Two-octet scalar octet count for following unhashed subpacket
       data. Note that this is the length in octets of all of the
       unhashed subpackets; a pointer incremented by this number will
       skip over the unhashed subpackets.
     - Unhashed subpacket data. (zero or more subpackets)
     - Two-octet field holding left 16 bits of signed hash value.
     - One or more multi-precision integers comprising the signature.
       This portion is algorithm specific, as described above.
*/

struct signature_v4 {
    byte version;	/*!< version number (4). */
    byte sigtype;	/*!< signature type. */
    byte pubkey_algo;	/*!< public key algorithm. */
    byte hash_algo;	/*!< hash algorithm. */
    byte hashlen[2];	/*!< length of following hashed material. */
    byte data[1];	/*!< Hashed subpacket data. (zero or more subpackets) */
};

static int pr_signature_v4(rpmKeyPkt ptag, const byte *h, unsigned hlen)
{
    struct signature_v4 *v = (struct signature_v4 *)h;
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
    plen = GRAB(v->hashlen);
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

static int pr_signature(rpmKeyPkt ptag, const byte *h, unsigned hlen)
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

/*
   A version 3 public key or public subkey packet contains:
     - A one-octet version number (3).
     - A four-octet number denoting the time that the key was created.
     - A two-octet number denoting the time in days that this key is
       valid. If this number is zero, then it does not expire.
     - A one-octet number denoting the public key algorithm of this key
     - A series of multi-precision integers comprising the key
       material:
         - MPI of RSA public modulus n;
         - MPI of RSA public encryption exponent e.

   Algorithm Specific Fields for RSA signatures:
     - multiprecision integer (MPI) of RSA signature value m**d.

   Algorithm Specific Fields for DSA signatures:
     - MPI of DSA value r.
     - MPI of DSA value s.

*/

struct key_v3 {
    byte version;	/*!< version number (3). */
    byte time[4];	/*!< time that the key was created. */
    byte valid[2];	/*!< time in days that this key is valid. */
    byte pubkey_algo;	/*!< public key algorithm. */
    byte data[1];	/*!< One or more multi-precision integers. */
};

static int pr_key_v3(rpmKeyPkt ptag, const byte *h, unsigned hlen)
{
    struct key_v3 *v = (struct key_v3 *)h;
    byte * p;
    unsigned plen;
    int i;

fprintf(stderr, "%s(%d)", ptags[ptag], ptag);
    if (v->version != 3) {
	fprintf(stderr, " version(%d) != 3\n", v->version);
	return 1;
    }
    plen = GRAB(v->time);
fprintf(stderr, " time %08x", plen);
fprintf(stderr, " %s(%d)", pr_pubkey_algo(v->pubkey_algo), v->pubkey_algo);

    plen = GRAB(v->valid);
    if (plen != 0)
	fprintf(stderr, " valid %d days", plen);

fprintf(stderr, "\n");

    p = &v->data[0];
    for (i = 0; p < &h[hlen]; i++, p += mpi_len(p))
	fprintf(stderr, "%7d %s\n", i, pr_mpi(p));

    return 0;
}

/*
   A version 4 packet contains:
     - A one-octet version number (4).
     - A four-octet number denoting the time that the key was created.
     - A one-octet number denoting the public key algorithm of this key
     - A series of multi-precision integers comprising the key
       material.  This algorithm-specific portion is:

       Algorithm Specific Fields for RSA public keys:
         - MPI of RSA public modulus n;
         - MPI of RSA public encryption exponent e.

       Algorithm Specific Fields for DSA public keys:
         - MPI of DSA prime p;
         - MPI of DSA group order q (q is a prime divisor of p-1);
         - MPI of DSA group generator g;
         - MPI of DSA public key value y (= g**x where x is secret).

       Algorithm Specific Fields for Elgamal public keys:
         - MPI of Elgamal prime p;
         - MPI of Elgamal group generator g;
         - MPI of Elgamal public key value y (= g**x where x is
           secret).
*/

struct key_v4 {
    byte version;	/*!< version number (4). */
    byte time[4];	/*!< time that the key was created. */
    byte pubkey_algo;	/*!< public key algorithm. */
    byte data[1];	/*!< One or more multi-precision integers. */
};

static int pr_key_v4(rpmKeyPkt ptag, const byte *h, unsigned hlen)
{
    struct key_v4 *v = (struct key_v4 *)h;
    byte * p;
    unsigned plen;
    int i;

fprintf(stderr, "%s(%d)", ptags[ptag], ptag);
    if (v->version != 4) {
	fprintf(stderr, " version(%d) != 4\n", v->version);
	return 1;
    }
    plen = GRAB(v->time);
fprintf(stderr, " time %08x", plen);
fprintf(stderr, " %s(%d)", pr_pubkey_algo(v->pubkey_algo), v->pubkey_algo);
fprintf(stderr, "\n");

    p = &v->data[0];
    for (i = 0; p < &h[hlen]; i++, p += mpi_len(p))
	fprintf(stderr, "%7d %s\n", i, pr_mpi(p));

    return 0;
}

static int pr_key(rpmKeyPkt ptag, const byte *h, unsigned hlen)
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

/*
5.11. User ID Packet (Tag 13)

   A User ID packet consists of data that is intended to represent the
   name and email address of the key holder.  By convention, it includes
   an RFC 822 mail name, but there are no restrictions on its content.
   The packet length in the header specifies the length of the user id.
   If it is text, it is encoded in UTF-8.
*/

static int pr_user_id(rpmKeyPkt ptag, const byte *h, unsigned hlen)
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
    rpmKeyPkt ptag = (val >> 2) & 0xf;
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
    case RPMKEYPKT_SIGNATURE:
	pr_signature(ptag, h, hlen);
	break;
    case RPMKEYPKT_PUBLIC_KEY:
    case RPMKEYPKT_PUBLIC_SUBKEY:
    case RPMKEYPKT_SECRET_KEY:
    case RPMKEYPKT_SECRET_SUBKEY:
	pr_key(ptag, h, hlen);
	break;
    case RPMKEYPKT_USER_ID:
	pr_user_id(ptag, h, hlen);
	break;
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
