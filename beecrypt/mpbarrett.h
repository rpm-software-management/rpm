/** \ingroup MP_m
 * \file mpbarrett.h
 *
 * Barrett modular reduction, header.
 */

/*
 * Copyright (c) 2003 Bob Deblier
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

#ifndef _MPBARRETT_H
#define _MPBARRETT_H

#include "beecrypt.h"
#include "mpnumber.h"

typedef struct
{
	uint32	size;
/*@owned@*/
	uint32* modl;	/* (size) words */
/*@dependent@*/ /*@null@*/
	uint32* mu;	/* (size+1) words */
} mp32barrett;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
BEECRYPTAPI
void mp32bzero(/*@out@*/ mp32barrett* b)
	/*@modifies b->size, b->modl, b->mu @*/;

/**
 */
BEECRYPTAPI
void mp32binit(mp32barrett* b, uint32 size)
	/*@modifies b->size, b->modl, b->mu @*/;

/**
 */
BEECRYPTAPI
void mp32bfree(/*@special@*/ mp32barrett* b)
	/*@uses b->size, b->modl @*/
	/*@releases b->modl @*/
	/*@modifies b->size, b->modl, b->mu @*/;

/**
 */
BEECRYPTAPI
void mp32bcopy(mp32barrett* b, const mp32barrett* copy)
	/*@modifies b->size, b->modl, b->mu @*/;

/**
 */
BEECRYPTAPI
void mp32bset(mp32barrett* b, uint32 size, const uint32* data)
	/*@modifies b->size, b->modl, b->mu @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mp32bsethex(mp32barrett* b, const char* hex)
	/*@modifies b->size, b->modl, b->mu @*/;

/**
 */
BEECRYPTAPI
void mp32bsubone(const mp32barrett* b, uint32* result)
	/*@modifies result @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mp32bneg(const mp32barrett* b, const uint32* xdata, uint32* result)
	/*@modifies result @*/;

/**
 */
BEECRYPTAPI
void mp32bmu_w(mp32barrett* b, /*@out@*/ uint32* wksp)
	/*@modifies b->size, b->modl, b->mu, wksp @*/;

/**
 */
BEECRYPTAPI
void mp32brnd_w   (const mp32barrett* b, randomGeneratorContext* rc, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
void mp32brndodd_w(const mp32barrett* b, randomGeneratorContext* rc, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;
/*@=exportlocal@*/

/**
 */
BEECRYPTAPI
void mp32brndinv_w(const mp32barrett* b, randomGeneratorContext* rc, /*@out@*/ uint32* result, /*@out@*/ uint32* inverse, /*@out@*/ uint32* wksp)
	/*@modifies result, inverse, wksp @*/;

/**
 */
BEECRYPTAPI
void mp32bmod_w(const mp32barrett* b, const uint32* xdata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;

/**
 */
BEECRYPTAPI
void mp32baddmod_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mp32bsubmod_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;

/**
 */
BEECRYPTAPI
void mp32bmulmod_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32 ysize, const uint32* ydata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;

/**
 */
BEECRYPTAPI
void mp32bsqrmod_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;

/**
 */
BEECRYPTAPI
void mp32bpowmod_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, uint32 psize, const uint32* pdata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
void mp32bpowmodsld_w(const mp32barrett* b, const uint32* slide, uint32 psize, const uint32* pdata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@globals internalState @*/
	/*@modifies result, wksp, internalState @*/;
/*@=exportlocal@*/

/**
 */
BEECRYPTAPI
void mp32btwopowmod_w(const mp32barrett* b, uint32 psize, const uint32* pdata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;

/**
 */
BEECRYPTAPI
int  mp32binv_w(const mp32barrett* b, uint32 xsize, const uint32* xdata, /*@out@*/ uint32* result, /*@out@*/ uint32* wksp)
	/*@modifies result, wksp @*/;

#ifdef	NOTYET
/**
 * @todo Simultaneous multiple exponentiation, for use in dsa and elgamal
 * signature verification.
 */
BEECRYPTAPI /*@unused@*/
void mp32bsm2powmod(const mp32barrett* b, const uint32*, const uint32*, const uint32*, const uint32*);

/**
 */
BEECRYPTAPI /*@unused@*/
void mp32bsm3powmod(const mp32barrett* b, const uint32*, const uint32*, const uint32*, const uint32*, const uint32*, const uint32*);
#endif	/* NOTYET */

/**
 */
BEECRYPTAPI /*@unused@*/
int  mp32bpprime_w(const mp32barrett* b, randomGeneratorContext* rc, int t, /*@out@*/ uint32* wksp)
	/*@modifies wksp @*/;

/**
 * @note Takes mpnumber as parameter.
 */
BEECRYPTAPI
void mp32bnrnd(const mp32barrett* b, randomGeneratorContext* rc, mpnumber* result)
	/*@modifies result @*/;

/**
 * @note Takes mpnumber as parameter.
 */
BEECRYPTAPI /*@unused@*/
void mp32bnmulmod(const mp32barrett* b, const mpnumber* x, const mpnumber* y, mpnumber* result)
	/*@modifies result @*/;

/**
 * @note Takes mpnumber as parameter.
 */
BEECRYPTAPI /*@unused@*/
void mp32bnsqrmod(const mp32barrett* b, const mpnumber* x, mpnumber* result)
	/*@modifies result @*/;

/**
 * @note Takes mpnumber as parameter.
 */
BEECRYPTAPI
void mp32bnpowmod   (const mp32barrett* b, const mpnumber* x, const mpnumber* pow, mpnumber* y)
	/*@modifies y @*/;

/**
 * @note Takes mpnumber as parameter.
 */
BEECRYPTAPI /*@unused@*/
void mp32bnpowmodsld(const mp32barrett* b, const uint32* slide, const mpnumber* pow, mpnumber* y)
	/*@modifies y @*/;

#ifdef __cplusplus
}
#endif

#endif
