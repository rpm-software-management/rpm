/*!\file pkcs1.h
 * \brief PKCS#1 utility routines
 * \ingroup PKCS1_m
 */

#ifndef _PKCS1_H
#define _PKCS1_H

#include "beecrypt/beecrypt.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!\brief This function computes the digest, and encodes it it according to PKCS#1 for signing
 * \param ctxt The hash function context
 * \param emdata
 * \param emsize
 */
BEECRYPTAPI
int pkcs1_emsa_encode_digest(hashFunctionContext* ctxt, byte* emdata, size_t emsize);

#ifdef __cplusplus
}
#endif

#endif
