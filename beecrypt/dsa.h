/** \ingroup DSA_m
 * \file dsa.h
 *
 * Digital Signature Algorithm signature scheme, header.
 */

/*
 * Copyright (c) 2001, 2002 Virtual Unlimited B.V.
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

#ifndef _DSA_H
#define _DSA_H

#include "mpbarrett.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The raw DSA signing function.
 *
 * Signing equations:
 *
 * \li \f$r=(g^{k}\ \textrm{mod}\ p)\ \textrm{mod}\ q\f$
 * \li \f$s=k^{-1}(h(m)+xr)\ \textrm{mod}\ q\f$
 *
 * @param p		The prime.
 * @param q		The cofactor.
 * @param g		The generator.
 * @param rgc		The pseudo-random generator context.
 * @param hm		The hash to be signed.
 * @param x		The private key value.
 * @param r		The signature's \e r value.
 * @param s		The signature's \e r value.
 * @retval		0 on success, -1 on failure.
 */
BEECRYPTAPI /*@unused@*/
int dsasign(const mpbarrett* p, const mpbarrett* q, const mpnumber* g, randomGeneratorContext* rgc, const mpnumber* hm, const mpnumber* x, mpnumber* r, mpnumber* s)
	/*@modifies r->size, r->data, *r->data, s->size, s->data @*/;

/**
 * The raw DSA verification function.
 *
 * Verifying equations:
 * \li Check \f$0<r<q\f$ and \f$0<s<q\f$
 * \li \f$w=s^{-1}\ \textrm{mod}\ q\f$
 * \li \f$u_1=w \cdot h(m)\ \textrm{mod}\ q\f$
 * \li \f$u_2=rw\ \textrm{mod}\ q\f$
 * \li \f$v=(g^{u_1}y^{u_2}\ \textrm{mod}\ p)\ \textrm{mod}\ q\f$
 * \li Check \f$v=r\f$
 *
 * @warning The return type of this function should be a boolean, but since
 *          that type isn't portable, an int is used.
 *
 * @param p		The prime.
 * @param q		The cofactor.
 * @param g		The generator.
 * @param hm		The digest to be verified.
 * @param y		The public key value.
 * @param r		The signature's r value.
 * @param s		The signature's r value.
 * @retval		0 on failure, 1 on success.
 */
BEECRYPTAPI /*@unused@*/
int dsavrfy(const mpbarrett* p, const mpbarrett* q, const mpnumber* g, const mpnumber* hm, const mpnumber* y, const mpnumber* r, const mpnumber* s)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif
