/*
 * Copyright (c) 2003 Bob Deblier
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

/*!\file mpbarrett.h
 * \brief Multi-precision integer routines using Barrett modular reduction, headers.   
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup MP_m
 */

#ifndef _MPBARRETT_H
#define _MPBARRETT_H

#include "beecrypt.h"
#include "mpnumber.h"

typedef struct
{
	size_t	size;
/*@owned@*/
	mpw*	modl;	/* (size) words */
/*@dependent@*/ /*@null@*/
	mpw*	mu;	/* (size+1) words */
} mpbarrett;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
BEECRYPTAPI
void mpbzero(/*@out@*/ mpbarrett* b)
	/*@modifies b->size, b->modl, b->mu @*/;

/**
 */
BEECRYPTAPI
void mpbinit(mpbarrett* b, size_t size)
	/*@modifies b->size, b->modl, b->mu @*/;

/**
 */
BEECRYPTAPI
void mpbfree(/*@special@*/ mpbarrett* b)
	/*@uses b->size, b->modl @*/
	/*@releases b->modl @*/
	/*@modifies b->size, b->modl, b->mu @*/;

/**
 */
BEECRYPTAPI
void mpbcopy(mpbarrett* b, const mpbarrett* copy)
	/*@modifies b->size, b->modl, b->mu @*/;

/**
 */
BEECRYPTAPI
void mpbset(mpbarrett* b, size_t size, const mpw* data)
	/*@modifies b->size, b->modl, b->mu @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpbsethex(mpbarrett* b, const char* hex)
	/*@modifies b->size, b->modl, b->mu @*/;

/**
 */
BEECRYPTAPI
void mpbsubone(const mpbarrett* b, mpw* result)
	/*@modifies result @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpbneg(const mpbarrett* b, const mpw* data, mpw* result)
	/*@modifies result @*/;

/**
 */
BEECRYPTAPI
void mpbmu_w(mpbarrett* b, /*@out@*/ mpw* wksp)
	/*@modifies b->size, b->modl, b->mu, wksp @*/;

/**
 */
BEECRYPTAPI
void mpbrnd_w   (const mpbarrett* b, randomGeneratorContext* rc, /*@out@*/ mpw* result, /*@out@*/ mpw* wksp)
	/*@modifies result, wksp @*/;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
void mpbrndodd_w(const mpbarrett* b, randomGeneratorContext* rc, /*@out@*/ mpw* result, /*@out@*/ mpw* wksp)
	/*@modifies result, wksp @*/;
/*@=exportlocal@*/

/**
 */
BEECRYPTAPI
void mpbrndinv_w(const mpbarrett* b, randomGeneratorContext* rc, /*@out@*/ mpw* result, /*@out@*/ mpw* inverse, /*@out@*/ mpw* wksp)
	/*@modifies result, inverse, wksp @*/;

/**
 */
BEECRYPTAPI
void mpbmod_w(const mpbarrett* b, const mpw* data, /*@out@*/ mpw* result, /*@out@*/ mpw* wksp)
	/*@modifies result, wksp @*/;

/**
 */
BEECRYPTAPI
void mpbaddmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, /*@out@*/ mpw* result, /*@out@*/ mpw* wksp)
	/*@modifies result, wksp @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpbsubmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, /*@out@*/ mpw* result, /*@out@*/ mpw* wksp)
	/*@modifies result, wksp @*/;

/**
 */
BEECRYPTAPI
void mpbmulmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, /*@out@*/ mpw* result, /*@out@*/ mpw* wksp)
	/*@modifies result, wksp @*/;

/**
 */
BEECRYPTAPI
void mpbsqrmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, /*@out@*/ mpw* result, /*@out@*/ mpw* wksp)
	/*@modifies result, wksp @*/;

/**
 */
BEECRYPTAPI
void mpbpowmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, size_t psize, const mpw* pdata, /*@out@*/ mpw* result, /*@out@*/ mpw* wksp)
	/*@modifies result, wksp @*/;

/**
 */
/*@-exportlocal@*/
BEECRYPTAPI
void mpbpowmodsld_w(const mpbarrett* b, const mpw* slide, size_t psize, const mpw* pdata, /*@out@*/ mpw* result, /*@out@*/ mpw* wksp)
	/*@globals internalState @*/
	/*@modifies result, wksp, internalState @*/;
/*@=exportlocal@*/

/**
 */
BEECRYPTAPI
void mpbtwopowmod_w(const mpbarrett* b, size_t psize, const mpw* pdata, /*@out@*/ mpw* result, /*@out@*/ mpw* wksp)
	/*@modifies result, wksp @*/;

/**
 */
BEECRYPTAPI
int  mpbinv_w(const mpbarrett* b, size_t xsize, const mpw* xdata, /*@out@*/ mpw* result, /*@out@*/ mpw* wksp)
	/*@modifies result, wksp @*/;

#ifdef	NOTYET
/**
 * @todo Simultaneous multiple exponentiation, for use in dsa and elgamal
 * signature verification.
 */
BEECRYPTAPI /*@unused@*/
void mpbsm2powmod(const mpbarrett* b, const mpw*, const mpw*, const mpw*, const mpw*);

/**
 */
BEECRYPTAPI /*@unused@*/
void mpbsm3powmod(const mpbarrett* b, const mpw*, const mpw*, const mpw*, const mpw*, const mpw*, const mpw*);
#endif	/* NOTYET */

/**
 */
BEECRYPTAPI /*@unused@*/
int  mpbpprime_w(const mpbarrett* b, randomGeneratorContext* r, int t, /*@out@*/ mpw* wksp)
	/*@modifies wksp @*/;

/**
 * @note Takes mpnumber as parameter.
 */
BEECRYPTAPI
void mpbnrnd(const mpbarrett* b, randomGeneratorContext* rc, mpnumber* result)
	/*@modifies result @*/;

/**
 * @note Takes mpnumber as parameter.
 */
BEECRYPTAPI /*@unused@*/
void mpbnmulmod(const mpbarrett* b, const mpnumber* x, const mpnumber* y, mpnumber* result)
	/*@modifies result @*/;

/**
 * @note Takes mpnumber as parameter.
 */
BEECRYPTAPI /*@unused@*/
void mpbnsqrmod(const mpbarrett* b, const mpnumber* x, mpnumber* result)
	/*@modifies result @*/;

/**
 * @note Takes mpnumber as parameter.
 */
BEECRYPTAPI
void mpbnpowmod   (const mpbarrett* b, const mpnumber* x, const mpnumber* pow, mpnumber* y)
	/*@modifies y @*/;

/**
 * @note Takes mpnumber as parameter.
 */
BEECRYPTAPI /*@unused@*/
void mpbnpowmodsld(const mpbarrett* b, const mpw* slide, const mpnumber* pow, mpnumber* y)
	/*@modifies y @*/;

#ifdef __cplusplus
}
#endif

#endif
