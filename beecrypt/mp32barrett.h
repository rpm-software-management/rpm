/*
 * mp32barrett.h
 *
 * Barrett modular reduction, header
 *
 * Copyright (c) 1997-2000 Virtual Unlimited B.V.
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

#ifndef _MP32BARRETT_H
#define _MP32BARRETT_H

#include "beecrypt.h"
#include "mp32number.h"

typedef struct
{
	uint32	size;
	uint32*	data;	/* (size) words / allocated on one block of 9*size+5 words and set the other pointers appropriately */
	uint32* modl;	/* (size+1) words */
	uint32* mu;		/* (size+1) words */
	uint32* wksp;	/* (6*size+4) words */
} mp32barrett;

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
void mp32bzero	   (mp32barrett*);
BEEDLLAPI
void mp32binit     (mp32barrett*, uint32);
BEEDLLAPI
void mp32bfree     (mp32barrett*);
BEEDLLAPI
void mp32bset      (mp32barrett*, uint32, const uint32*);

BEEDLLAPI
void mp32bmu       (mp32barrett*);

BEEDLLAPI
void mp32brnd      (const mp32barrett*, randomGeneratorContext*);
BEEDLLAPI
void mp32brndres   (const mp32barrett*, uint32*, randomGeneratorContext*);

BEEDLLAPI
void mp32bmodsubone(const mp32barrett*);
BEEDLLAPI
void mp32bneg      (const mp32barrett*);

BEEDLLAPI
void mp32bmod      (const mp32barrett*, uint32, const uint32*);
BEEDLLAPI
void mp32baddmod   (const mp32barrett*, uint32, const uint32*, uint32, const uint32*);
BEEDLLAPI
void mp32bsubmod   (const mp32barrett*, uint32, const uint32*, uint32, const uint32*);

BEEDLLAPI
void mp32bmulmodres(const mp32barrett*, uint32*, uint32, const uint32*, uint32, const uint32*);
BEEDLLAPI
void mp32bsqrmodres(const mp32barrett*, uint32*, uint32, const uint32*);

BEEDLLAPI
void mp32bmulmod   (const mp32barrett*, uint32, const uint32*, uint32, const uint32*);
BEEDLLAPI
void mp32bsqrmod   (const mp32barrett*, uint32, const uint32*);

BEEDLLAPI
void mp32bpowmod   (const mp32barrett*, uint32, const uint32*, uint32, const uint32*);
BEEDLLAPI
void mp32btwopowmod(const mp32barrett*, uint32, const uint32*);

/* simultaneous multiple exponentiation, for use in dsa and elgamal signature verification */

BEEDLLAPI
void mp32bsm2powmod(const mp32barrett*, const uint32*, const uint32*, const uint32*, const uint32*);
BEEDLLAPI
void mp32bsm3powmod(const mp32barrett*, const uint32*, const uint32*, const uint32*, const uint32*, const uint32*, const uint32*);

BEEDLLAPI
int  mp32binv      (const mp32barrett*, uint32, const uint32*);

BEEDLLAPI
int  mp32bpprime   (const mp32barrett*, randomGeneratorContext*, int);

/* the next routines take mp32numbers as parameters */

BEEDLLAPI
void mp32bnmulmodres(const mp32barrett*, uint32*, const mp32number*, const mp32number*);
BEEDLLAPI
void mp32bnsqrmodres(const mp32barrett*, uint32*, const mp32number*);

BEEDLLAPI
void mp32bnpowmod   (const mp32barrett*, const mp32number*, const mp32number*);
BEEDLLAPI
void mp32bnsqrmod   (const mp32barrett*, const mp32number*);

#ifdef __cplusplus
}
#endif

#endif
