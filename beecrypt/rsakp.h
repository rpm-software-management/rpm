/*
 * Copyright (c) 2000, 2001, 2002 Virtual Unlimited B.V.
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

/*!\file rsakp.h
 * \brief RSA keypair, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup IF_m IF_rsa_m
 */

#ifndef _RSAKP_H
#define _RSAKP_H

#include "beecrypt/rsapk.h"

/*!\brief RSA keypair.
 * \ingroup IF_rsa_m
 */
#ifdef __cplusplus
struct BEECRYPTAPI rsakp
#else
struct _rsakp
#endif
{
	/*!\var n
	 * \brief The modulus.
	 *
	 * \f$n=pq\f$
	 */
	mpbarrett n;
	/*!\var e
	 * \brief The public exponent.
	 */
	mpnumber e;
	/*!\var d
	 * \brief The private exponent.
	 */
	mpnumber d;
	/*!\var p
	 * \brief The first prime factor of the modulus.
	 */
	mpbarrett p;
	/*!\var q
	 * \brief The second prime factor of the modulus.
	 */
	mpbarrett q;
	/*!\var dp
	 * \brief the first prime coefficient.
	 * \f$dp=d\ \textrm{mod}\ (p-1)\f$
	 */
	mpnumber dp;
	/*!\var dq
	 * \brief the second prime coefficient.
	 * \f$dq=d\ \textrm{mod}\ (q-1)\f$
	 */
	mpnumber dq;
	/*!\var qi
	 * \brief the crt coefficient.
	 * \f$qi=q^{-1}\ \textrm{mod}\ p\f$
	 */
	mpnumber qi;
	#ifdef __cplusplus
	rsakp();
	rsakp(const rsakp&);
	~rsakp();
	#endif
};

#ifndef __cplusplus
typedef struct _rsakp rsakp;
#endif

#ifdef __cplusplus
extern "C" {
#endif

BEECRYPTAPI
int rsakpMake(rsakp*, randomGeneratorContext*, size_t);
BEECRYPTAPI
int rsakpInit(rsakp*);
BEECRYPTAPI
int rsakpFree(rsakp*);
BEECRYPTAPI
int rsakpCopy(rsakp*, const rsakp*);

#ifdef __cplusplus
}
#endif

#endif
