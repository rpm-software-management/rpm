/*
 * Copyright (c) 2000, 2001, 2002 Virtual Unlimited, B.V.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*!\file dhaes.h
 * \brief DHAES encryption scheme.
 *
 * This code implements the encryption scheme from the paper:
 *
 * "DHAES: An Encryption Scheme Based on the Diffie-Hellman Problem"
 * Michel Abdalla, Mihir Bellare, Phillip Rogaway
 * September 1998
 *
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup DL_m DL_dh_m
 */

#ifndef _DHAES_H
#define _DHAES_H
 
#include "beecrypt/beecrypt.h"
#include "beecrypt/dldp.h"

typedef struct
{
	const dldp_p*				param;
	const hashFunction*			hash;
	const blockCipher*			cipher;
	const keyedHashFunction*	mac;
	size_t						cipherkeybits;
	size_t						mackeybits;
} dhaes_pParameters;

typedef struct
{
	dldp_p						param;
	mpnumber					pub;
	mpnumber					pri;
	hashFunctionContext			hash;
	blockCipherContext			cipher;
	keyedHashFunctionContext	mac;
	size_t						cipherkeybits;
	size_t						mackeybits;
} dhaes_pContext;

#ifdef __cplusplus
extern "C" {
#endif

BEECRYPTAPI
int dhaes_pUsable(const dhaes_pParameters*);

BEECRYPTAPI
int dhaes_pContextInit       (dhaes_pContext*, const dhaes_pParameters*);
BEECRYPTAPI
int dhaes_pContextInitDecrypt(dhaes_pContext*, const dhaes_pParameters*, const mpnumber*);
BEECRYPTAPI
int dhaes_pContextInitEncrypt(dhaes_pContext*, const dhaes_pParameters*, const mpnumber*);
BEECRYPTAPI
int dhaes_pContextFree       (dhaes_pContext*);

BEECRYPTAPI
memchunk* dhaes_pContextEncrypt(dhaes_pContext*,       mpnumber*,       mpnumber*, const memchunk*, randomGeneratorContext*);
BEECRYPTAPI
memchunk* dhaes_pContextDecrypt(dhaes_pContext*, const mpnumber*, const mpnumber*, const memchunk*);

#ifdef __cplusplus
}
#endif

#endif
