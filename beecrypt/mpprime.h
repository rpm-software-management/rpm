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

/*!\file mpprime.h
 * \brief Multi-precision primes, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup MP_m
 */

#ifndef _MPPRIME_H
#define _MPPRIME_H

#include "mpbarrett.h"

#define SMALL_PRIMES_PRODUCT_MAX	32

extern mpw* mpspprod[SMALL_PRIMES_PRODUCT_MAX];

#ifdef __cplusplus
extern "C" {
#endif

BEECRYPTAPI
int  mpptrials     (size_t bits)
	/*@*/;
BEECRYPTAPI
int  mppmilrab_w   (const mpbarrett* p, randomGeneratorContext* rc, int t, mpw* wksp)
	/*@modifies wksp @*/;

BEECRYPTAPI
int  mpprnd_w      (mpbarrett* p, randomGeneratorContext* rc, size_t bits, int t, const mpnumber* f, mpw* wksp)
	/*@modifies p, wksp @*/;
BEECRYPTAPI
int  mpprndr_w     (mpbarrett* p, randomGeneratorContext* rc, size_t bits, int t, const mpnumber* min, const mpnumber* max, const mpnumber* f, mpw* wksp)
	/*@modifies p, wksp @*/;
BEECRYPTAPI
void mpprndsafe_w  (mpbarrett* p, randomGeneratorContext* rc, size_t bits, int t, mpw* wksp)
	/*@modifies p, wksp @*/;
BEECRYPTAPI
void mpprndcon_w   (mpbarrett*, randomGeneratorContext*, size_t, int, const mpnumber*, const mpnumber*, const mpnumber*, mpnumber*, mpw*)
	/*@*/;
BEECRYPTAPI
void mpprndconone_w(mpbarrett* p, randomGeneratorContext* rc, size_t bits, int t, const mpbarrett* q, const mpnumber* f, mpnumber* r, int cofactor, mpw* wksp)
	/*@modifies p, r, wksp @*/;

#ifdef __cplusplus
}
#endif

#endif
