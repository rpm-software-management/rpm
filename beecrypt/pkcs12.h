/*!\file pkcs12.h
 * \brief PKCS#12 utility routines
 * \ingroup PKCS12_m
 */

#ifndef _PKCS12_H
#define _PKCS12_H

#include "beecrypt/beecrypt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PKCS12_ID_CIPHER	0x1
#define PKCS12_ID_IV		0x2
#define PKCS12_ID_MAC		0x3

BEECRYPTAPI
int pkcs12_derive_key(const hashFunction* h, byte id, const byte* pdata, size_t psize, const byte* sdata, size_t ssize, size_t iterationcount, byte* ndata, size_t nsize);

#ifdef __cplusplus
}
#endif

#endif
