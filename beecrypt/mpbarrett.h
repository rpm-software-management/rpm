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

#ifdef __cplusplus
# include <iostream>
#endif

#ifdef __cplusplus
struct BEECRYPTAPI mpbarrett
#else
struct _mpbarrett
#endif
{
	size_t	size;
/*@relnull@*/
	mpw*	modl;	/* (size) words */
/*@relnull@*/
	mpw*	mu;		/* (size+1) words */

#ifdef __cplusplus
	mpbarrett();
	mpbarrett(const mpbarrett&);
	~mpbarrett();
																				
	const mpbarrett& operator=(const mpbarrett&);
	bool operator==(const mpbarrett&);
	bool operator!=(const mpbarrett&);
																				
	void wipe();
	size_t bitlength() const;
#endif
};

#ifndef __cplusplus
typedef struct _mpbarrett mpbarrett;
#else
BEECRYPTAPI
std::ostream& operator<<(std::ostream&, const mpbarrett&);
#endif

#ifdef __cplusplus
extern "C" {
#endif

BEECRYPTAPI
void mpbzero(mpbarrett* b)
	/*@modifies b @*/;
BEECRYPTAPI
void mpbinit(mpbarrett* b, size_t size)
	/*@modifies b @*/;
BEECRYPTAPI
void mpbfree(mpbarrett* b)
	/*@modifies b @*/;
BEECRYPTAPI
void mpbcopy(mpbarrett* b, const mpbarrett* copy)
	/*@modifies b @*/;
BEECRYPTAPI
void mpbwipe(mpbarrett* b)
	/*@modifies b @*/;

BEECRYPTAPI
void mpbset(mpbarrett* b, size_t size, const mpw* data)
	/*@modifies b @*/;

BEECRYPTAPI
int mpbsetbin(mpbarrett* b, const byte* osdata, size_t ossize)
	/*@modifies b @*/;
BEECRYPTAPI
int mpbsethex(mpbarrett* b, const char* hex)
	/*@modifies b @*/;

BEECRYPTAPI
void mpbsubone(const mpbarrett* b, mpw* result)
	/*@modifies result @*/;

BEECRYPTAPI
void mpbmu_w(mpbarrett* b, mpw* wksp)
	/*@modifies b, wksp @*/;

BEECRYPTAPI
void mpbrnd_w   (const mpbarrett* b, randomGeneratorContext* rc, mpw* result, mpw* wksp)
	/*@modifies result, wksp @*/;
BEECRYPTAPI
void mpbrndodd_w(const mpbarrett* b, randomGeneratorContext* rc, mpw* result, mpw* wksp)
	/*@modifies result, wksp @*/;
BEECRYPTAPI
void mpbrndinv_w(const mpbarrett* b, randomGeneratorContext* rc, mpw* result, mpw* inverse, mpw* wksp)
	/*@modifies result, inverse, wksp @*/;

BEECRYPTAPI
void mpbneg_w(const mpbarrett* b, const mpw* data, mpw* result)
	/*@modifies result @*/;
BEECRYPTAPI
void mpbmod_w(const mpbarrett* b, const mpw* data, mpw* result, mpw* wksp)
	/*@modifies result, wksp @*/;

BEECRYPTAPI
void mpbaddmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, mpw* result, mpw* wksp)
	/*@modifies result, wksp @*/;
BEECRYPTAPI
void mpbsubmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, mpw* result, mpw* wksp)
	/*@modifies result, wksp @*/;
BEECRYPTAPI
void mpbmulmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, size_t ysize, const mpw* ydata, mpw* result, mpw* wksp)
	/*@modifies result, wksp @*/;
BEECRYPTAPI
void mpbsqrmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, mpw* result, mpw* wksp)
	/*@modifies result, wksp @*/;
BEECRYPTAPI
void mpbpowmod_w(const mpbarrett* b, size_t xsize, const mpw* xdata, size_t psize, const mpw* pdata, mpw* result, mpw* wksp)
	/*@modifies result, wksp @*/;
BEECRYPTAPI
void mpbpowmodsld_w(const mpbarrett* b, const mpw* slide, size_t psize, const mpw* pdata, mpw* result, mpw* wksp)
	/*@modifies result, wksp @*/;
BEECRYPTAPI
void mpbtwopowmod_w(const mpbarrett* b, size_t psize, const mpw* pdata, mpw* result, mpw* wksp)
	/*@modifies result, wksp @*/;

/* To be added:
 * simultaneous multiple exponentiation, for use in dsa and elgamal signature verification
 */
BEECRYPTAPI
void mpbsm2powmod(const mpbarrett* b, const mpw*, const mpw*, const mpw*, const mpw*)
	/*@*/;
BEECRYPTAPI
void mpbsm3powmod(const mpbarrett* b, const mpw*, const mpw*, const mpw*, const mpw*, const mpw*, const mpw*)
	/*@*/;

BEECRYPTAPI
int  mpbpprime_w(const mpbarrett* b, randomGeneratorContext* r, int t, mpw* wksp)
	/*@modifies wksp @*/;

/* the next routines take mpnumbers as parameters */

BEECRYPTAPI
void mpbnrnd(const mpbarrett* b, randomGeneratorContext* rc, mpnumber* result)
	/*@modifies result @*/;

BEECRYPTAPI
void mpbnmulmod(const mpbarrett* b, const mpnumber* x, const mpnumber* y, mpnumber* result)
	/*@modifies result @*/;
BEECRYPTAPI
void mpbnsqrmod(const mpbarrett* b, const mpnumber* x, mpnumber* result)
	/*@modifies result @*/;

BEECRYPTAPI
void mpbnpowmod   (const mpbarrett* b, const mpnumber* x, const mpnumber* pow, mpnumber* y)
	/*@modifies y @*/;
BEECRYPTAPI
void mpbnpowmodsld(const mpbarrett* b, const mpw* slide, const mpnumber* pow, mpnumber* y)
	/*@modifies y @*/;

BEECRYPTAPI
size_t mpbbits(const mpbarrett* b)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif
