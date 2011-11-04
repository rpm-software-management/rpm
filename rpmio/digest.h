#ifndef _RPMDIGEST_H
#define _RPMDIGEST_H

#include <nss.h>
#include <sechash.h>
#include <keyhi.h>
#include <cryptohi.h>

#include <rpm/rpmpgp.h>
#include "rpmio/base64.h"

typedef struct pgpDigAlg_s * pgpDigAlg;

typedef int (*setmpifunc)(pgpDigAlg digp,
                          int num, const uint8_t *p, const uint8_t *pend);
typedef int (*verifyfunc)(pgpDigAlg pgpkey, pgpDigAlg pgpsig,
                          uint8_t *hash, size_t hashlen, int hash_algo);
typedef void (*freefunc)(pgpDigAlg digp);

struct pgpDigAlg_s {
    setmpifunc setmpi;
    verifyfunc verify;
    freefunc free;
    int mpis;
    void *data;			/*!< algorithm specific private data */
};

/** \ingroup rpmio
 * Values parsed from OpenPGP signature/pubkey packet(s).
 */
struct pgpDigParams_s {
    char * userid;
    uint8_t * hash;
    char * params[4];
    uint8_t tag;

    uint8_t version;		/*!< version number. */
    pgpTime_t time;		/*!< time that the key was created. */
    uint8_t pubkey_algo;		/*!< public key algorithm. */

    uint8_t hash_algo;
    uint8_t sigtype;
    uint8_t hashlen;
    uint8_t signhash16[2];
    pgpKeyID_t signid;
    uint8_t saved;
#define	PGPDIG_SAVED_TIME	(1 << 0)
#define	PGPDIG_SAVED_ID		(1 << 1)

    pgpDigAlg alg;
};

/** \ingroup rpmio
 * Container for values parsed from an OpenPGP signature and public key.
 */
struct pgpDig_s {
    struct pgpDigParams_s signature;
    struct pgpDigParams_s pubkey;
};

pgpDigAlg pgpPubkeyNew(int algo);

pgpDigAlg pgpSignatureNew(int algo);

pgpDigAlg pgpDigAlgFree(pgpDigAlg da);

#endif /* _RPMDIGEST_H */
