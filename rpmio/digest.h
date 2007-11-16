#ifndef _RPMDIGEST_H
#define _RPMDIGEST_H

#include "base64.h"
#include "rpmpgp.h"

#include <nss.h>
#include <sechash.h>
#include <keyhi.h>
#include <cryptohi.h>

/** \ingroup rpmio
 * Values parsed from OpenPGP signature/pubkey packet(s).
 */
struct pgpDigParams_s {
    const char * userid;
    const byte * hash;
    const char * params[4];
    byte tag;

    byte version;		/*!< version number. */
    byte time[4];		/*!< time that the key was created. */
    byte pubkey_algo;		/*!< public key algorithm. */

    byte hash_algo;
    byte sigtype;
    byte hashlen;
    byte signhash16[2];
    byte signid[8];
    byte saved;
#define	PGPDIG_SAVED_TIME	(1 << 0)
#define	PGPDIG_SAVED_ID		(1 << 1)

};

/** \ingroup rpmio
 * Container for values parsed from an OpenPGP signature and public key.
 */
struct pgpDig_s {
    struct pgpDigParams_s signature;
    struct pgpDigParams_s pubkey;

    size_t nbytes;		/*!< No. bytes of plain text. */

    DIGEST_CTX sha1ctx;		/*!< (dsa) sha1 hash context. */
    DIGEST_CTX hdrsha1ctx;	/*!< (dsa) header sha1 hash context. */
    void * sha1;		/*!< (dsa) V3 signature hash. */
    size_t sha1len;		/*!< (dsa) V3 signature hash length. */

    DIGEST_CTX md5ctx;		/*!< (rsa) md5 hash context. */
    DIGEST_CTX hdrmd5ctx;	/*!< (rsa) header md5 hash context. */
    void * md5;			/*!< (rsa) V3 signature hash. */
    size_t md5len;		/*!< (rsa) V3 signature hash length. */

    /* DSA parameters */
    SECKEYPublicKey *dsa;
    SECItem *dsasig;

    /* RSA parameters */
    SECKEYPublicKey *rsa;
    SECItem *rsasig;
};

#endif /* _RPMDIGEST_H */
