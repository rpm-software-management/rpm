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

#include "beecrypt/beecrypt.h"
#include "beecrypt/mpnumber.h"

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
	mpw*	modl;	/* (size) words */
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
void mpbzero(mpbarrett*);
BEECRYPTAPI
void mpbinit(mpbarrett*, size_t);
BEECRYPTAPI
void mpbfree(mpbarrett*);
BEECRYPTAPI
void mpbcopy(mpbarrett*, const mpbarrett*);
BEECRYPTAPI
void mpbwipe(mpbarrett*);

BEECRYPTAPI
void mpbset(mpbarrett*, size_t, const mpw*);

BEECRYPTAPI
int mpbsetbin(mpbarrett*, const byte*, size_t);
BEECRYPTAPI
int mpbsethex(mpbarrett*, const char*);

BEECRYPTAPI
void mpbsubone(const mpbarrett*, mpw*);

BEECRYPTAPI
void mpbmu_w(mpbarrett*, mpw*);

BEECRYPTAPI
void mpbrnd_w   (const mpbarrett*, randomGeneratorContext*, mpw*, mpw*);
BEECRYPTAPI
void mpbrndodd_w(const mpbarrett*, randomGeneratorContext*, mpw*, mpw*);
BEECRYPTAPI
void mpbrndinv_w(const mpbarrett*, randomGeneratorContext*, mpw*, mpw*, mpw*);

BEECRYPTAPI
void mpbneg_w(const mpbarrett*, const mpw*, mpw*);
BEECRYPTAPI
void mpbmod_w(const mpbarrett*, const mpw*, mpw*, mpw*);

BEECRYPTAPI
void mpbaddmod_w(const mpbarrett*, size_t, const mpw*, size_t, const mpw*, mpw*, mpw*);
BEECRYPTAPI
void mpbsubmod_w(const mpbarrett*, size_t, const mpw*, size_t, const mpw*, mpw*, mpw*);
BEECRYPTAPI
void mpbmulmod_w(const mpbarrett*, size_t, const mpw*, size_t, const mpw*, mpw*, mpw*);
BEECRYPTAPI
void mpbsqrmod_w(const mpbarrett*, size_t, const mpw*, mpw*, mpw*);
BEECRYPTAPI
void mpbpowmod_w(const mpbarrett*, size_t, const mpw*, size_t, const mpw*, mpw*, mpw*);
BEECRYPTAPI
void mpbpowmodsld_w(const mpbarrett*, const mpw*, size_t, const mpw*, mpw*, mpw*);
BEECRYPTAPI
void mpbtwopowmod_w(const mpbarrett*, size_t, const mpw*, mpw*, mpw*);

/* To be added:
 * simultaneous multiple exponentiation, for use in dsa and elgamal signature verification
 */
BEECRYPTAPI
void mpbsm2powmod(const mpbarrett*, const mpw*, const mpw*, const mpw*, const mpw*);
BEECRYPTAPI
void mpbsm3powmod(const mpbarrett*, const mpw*, const mpw*, const mpw*, const mpw*, const mpw*, const mpw*);

BEECRYPTAPI
int  mpbpprime_w(const mpbarrett*, randomGeneratorContext*, int, mpw*);

/* the next routines take mpnumbers as parameters */

BEECRYPTAPI
void mpbnrnd(const mpbarrett*, randomGeneratorContext*, mpnumber*);

BEECRYPTAPI
void mpbnmulmod(const mpbarrett*, const mpnumber*, const mpnumber*, mpnumber*);
BEECRYPTAPI
void mpbnsqrmod(const mpbarrett*, const mpnumber*, mpnumber*);

BEECRYPTAPI
void mpbnpowmod   (const mpbarrett*, const mpnumber*, const mpnumber*, mpnumber*);
BEECRYPTAPI
void mpbnpowmodsld(const mpbarrett*, const mpw*, const mpnumber*, mpnumber*);

BEECRYPTAPI
size_t mpbbits(const mpbarrett*);

#ifdef __cplusplus
}
#endif

#endif
