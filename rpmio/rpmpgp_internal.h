#ifndef _RPMPGP_INTERNAL_H
#define _RPMPGP_INTERNAL_H

#include "rpmio/rpmpgpval.h"

typedef struct pgpDigAlg_s * pgpDigAlg;

typedef int (*setmpifunc)(pgpDigAlg digp, int num, const uint8_t *p);
typedef int (*verifyfunc)(pgpDigAlg pgpkey, pgpDigAlg pgpsig,
                          uint8_t *hash, size_t hashlen, int hash_algo);
typedef void (*freefunc)(pgpDigAlg digp);

struct pgpDigAlg_s {
    setmpifunc setmpi;
    verifyfunc verify;
    freefunc free;
    int curve;
    int mpis;
    void *data;			/*!< algorithm specific private data */
};

pgpDigAlg pgpPubkeyNew(int algo, int curve);

pgpDigAlg pgpSignatureNew(int algo);

pgpDigAlg pgpDigAlgFree(pgpDigAlg alg);

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

#endif /* _RPMPGP_INTERNAL_H */
