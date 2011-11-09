#ifndef _RPMDIGEST_H
#define _RPMDIGEST_H

#include <rpm/rpmpgp.h>

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

pgpDigAlg pgpPubkeyNew(int algo);

pgpDigAlg pgpSignatureNew(int algo);

pgpDigAlg pgpDigAlgFree(pgpDigAlg da);

/** \ingroup rpmpgp
 * Return no. of bits in a multiprecision integer.
 * @param p		pointer to multiprecision integer
 * @return		no. of bits
 */
static inline
unsigned int pgpMpiBits(const uint8_t *p)
{
    return ((p[0] << 8) | p[1]);
}

/** \ingroup rpmpgp
 * Return no. of bytes in a multiprecision integer.
 * @param p		pointer to multiprecision integer
 * @return		no. of bytes
 */
static inline
size_t pgpMpiLen(const uint8_t *p)
{
    return (2 + ((pgpMpiBits(p)+7)>>3));
}
	
#endif /* _RPMDIGEST_H */
