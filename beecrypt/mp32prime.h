/*
 * mp32prime.h
 *
 * Multi-precision primes, header
 *
 * Copyright (c) 2000 Virtual Unlimited B.V.
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

#ifndef _MP32PRIME_H
#define _MP32PRIME_H

#include "mp32barrett.h"

#define SMALL_PRIMES_PRODUCT_MAX	64

extern uint32* mp32spprod[SMALL_PRIMES_PRODUCT_MAX];

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
int  mp32ptrials   (uint32);
BEEDLLAPI
int  mp32pmilrab   (const mp32barrett*, randomGeneratorContext*, int);

BEEDLLAPI
void mp32prnd      (mp32barrett*, randomGeneratorContext*, uint32, int, const mp32number*);
BEEDLLAPI
void mp32prndsafe  (mp32barrett*, randomGeneratorContext*, uint32, int);
BEEDLLAPI
void mp32prndcon   (mp32barrett*, randomGeneratorContext*, uint32, int, const mp32number*, const mp32number*, const mp32number*, mp32number*);
BEEDLLAPI
void mp32prndconone(mp32barrett*, randomGeneratorContext*, uint32, int, const mp32barrett*, const mp32number*, mp32number*, int);

#ifdef __cplusplus
}
#endif

#endif
