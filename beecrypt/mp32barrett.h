/*
 * mp32barrett.h
 *
 * Barrett modular reduction, header
 *
 * Copyright (c) 1997, 1998, 1999, 2000, 2001 Virtual Unlimited B.V.
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
/*@owned@*/ uint32* modl;	/* (size) words */
/*@dependent@*/ uint32* mu;	/* (size+1) words */
} mp32barrett;

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
void mp32bzero(mp32barrett* b)
	/*@modifies b @*/;
BEEDLLAPI
void mp32binit(mp32barrett* b, uint32 size)
	/*@modifies b @*/;
BEEDLLAPI
void mp32bfree(mp32barrett* b)
	/*@modifies b @*/;
BEEDLLAPI
void mp32bcopy(mp32barrett* b, const mp32barrett* copy)
	/*@modifies b @*/;

BEEDLLAPI
void mp32bset(mp32barrett* b, uint32 size, const uint32* data)
	/*@modifies b @*/;
BEEDLLAPI
void mp32bsethex(mp32barrett* b, const char* hex)
	/*@modifies b @*/;

BEEDLLAPI
void mp32bsubone(const mp32barrett* b, /*@out@*/ uint32* result)
	/*@modifies result @*/;

BEEDLLAPI
void mp32bmu_w(mp32barrett* b, /*@out@*/ uint32* wksp)
	/*@modifies b, wksp @*/;

BEEDLLAPI
void mp32brnd_w   (const mp32barrett* b, randomGeneratorContext* rc, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;
BEEDLLAPI
void mp32brndodd_w(const mp32barrett* b, randomGeneratorContext* rc, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;
BEEDLLAPI
void mp32brndinv_w(const mp32barrett* b, randomGeneratorContext* rc, /*@out@*/ uint32* result, uint32* inverse, /*@out@*/ uint32* wksp)
	/*@modifies result, inverse, wksp @*/;

BEEDLLAPI
void mp32bneg_w(const mp32barrett* b, const uint32* xdata, uint32* result)
	/*@modifies result @*/;
BEEDLLAPI
void mp32bmod_w(const mp32barrett* b, const uint32* xdata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;

BEEDLLAPI
void mp32baddmod_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;
BEEDLLAPI
void mp32bsubmod_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;
BEEDLLAPI
void mp32bmulmod_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;
BEEDLLAPI
void mp32bsqrmod_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;
BEEDLLAPI
void mp32bpowmod_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32 psize, const uint32* pdata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;
BEEDLLAPI
void mp32bpowmodsld_w(const mp32barrett* b, const uint32* slide, uint32 psize, const uint32* pdata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;
BEEDLLAPI
void mp32btwopowmod_w(const mp32barrett* b, uint32 psize, const uint32* pdata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;

BEEDLLAPI
int  mp32binv_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;


/* To be added:
 * simultaneous multiple exponentiation, for use in dsa and elgamal signature verification
 */
BEEDLLAPI
void mp32bsm2powmod(const mp32barrett* b, const uint32*, const uint32*, const uint32*, const uint32*);
BEEDLLAPI
void mp32bsm3powmod(const mp32barrett* b, const uint32*, const uint32*, const uint32*, const uint32*, const uint32*, const uint32*);


BEEDLLAPI
int  mp32bpprime_w(const mp32barrett* b, randomGeneratorContext* rc, int t, /*@out@*/ uint32* wksp)
	/*@modifies wksp @*/;

/* the next routines take mp32numbers as parameters */

BEEDLLAPI
void mp32bnrnd(const mp32barrett* b, randomGeneratorContext* rc, /*@out@*/ mp32number* result)
	/*@modifies result @*/;

BEEDLLAPI
void mp32bnmulmod(const mp32barrett* b, const mp32number* x, const mp32number* y, /*@out@*/ mp32number* result)
	/*@modifies result @*/;
BEEDLLAPI
void mp32bnsqrmod(const mp32barrett* b, const mp32number* x, /*@out@*/ mp32number* result)
	/*@modifies result @*/;

BEEDLLAPI
void mp32bnpowmod   (const mp32barrett* b, const mp32number* x, const mp32number* pow, mp32number* y)
	/*@modifies y @*/;
BEEDLLAPI
void mp32bnpowmodsld(const mp32barrett* b, const uint32* slide, const mp32number* pow, mp32number* y)
	/*@modifies y @*/;

#ifdef __cplusplus
}
#endif

#endif
