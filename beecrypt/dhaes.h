/*
 * dhaes.h
 *
 * DHAES, header
 *
 * Copyright (c) 2000, 2001 Virtual Unlimited, B.V.
 *
 * Author: Bob Deblier <bob@virtualunlimited.com>
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
 
#ifndef _DHAES_H
#define _DHAES_H
 
#include "beecrypt.h"
#include "dldp.h"

typedef struct
{
	dldp_p param;
	hashFunctionContext hash;
	blockCipherContext cipher;
	keyedHashFunctionContext mac;
	randomGeneratorContext rng;
} dhaes_p;

BEEDLLAPI
int dhaes_usable(const blockCipher*, const keyedHashFunction*, const hashFunction*);

BEEDLLAPI
int dhaes_pInit(dhaes_p*, const dldp_p*, const blockCipher*, const keyedHashFunction*, const hashFunction*, const randomGenerator*);
BEEDLLAPI
int dhaes_pFree(dhaes_p*);

BEEDLLAPI
memchunk* dhaes_pEncrypt(dhaes_p*, const mp32number*,       mp32number*,       mp32number*, const memchunk*);
BEEDLLAPI
memchunk* dhaes_pDecrypt(dhaes_p*, const mp32number*, const mp32number*, const mp32number*, const memchunk*);

#endif
