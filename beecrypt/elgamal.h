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

/*!\file elgamal.h
 * \brief ElGamal algorithm.
 *
 * For more information on this algorithm, see:
 *  "Handbook of Applied Cryptography",
 *  11.5.2: "The ElGamal signature scheme", p. 454-459
 *
 * Two of the signature variants in Note 11.70 are implemented.
 *
 * \todo Implement ElGamal encryption and decryption.
 *
 * \todo Explore the possibility of using simultaneous multiple exponentiation,
 *       as described in HAC, 14.87 (iii).
 *
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup DL_m DL_elgamal_m
 */

#ifndef _ELGAMAL_H
#define _ELGAMAL_H

#include "mpbarrett.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!\fn int elgv1sign(const mpbarrett* p, const mpbarrett* n, const mpnumber* g,
randomGeneratorContext* rgc, const mpnumber* hm, const mpnumber* x, mpnumber* r,
 mpnumber* s)
 * \brief This function performs raw ElGamal signing, variant 1.
 *
 * Signing equations:
 *
 * \li \f$r=g^{k}\ \textrm{mod}\ p\f$
 * \li \f$s=k^{-1}(h(m)-xr)\ \textrm{mod}\ (p-1)\f$
 *
 * \param p The prime.
 * \param n The reducer mod (p-1).
 * \param g The generator.
 * \param rgc The pseudo-random generat
 * \param hm The hash to be signed.
 * \param x The private key value.
 * \param r The signature's \e r value.
 * \param s The signature's \e s value.
 * \retval 0 on success.
 * \retval -1 on failure.
 */
BEECRYPTAPI
int elgv1sign(const mpbarrett* p, const mpbarrett* n, const mpnumber* g, randomGeneratorContext*, const mpnumber* hm, const mpnumber* x, mpnumber* r, mpnumber* s);

/*!\fn int elgv1vrfy(const mpbarrett* p, const mpbarrett* n, const mpnumber* g, const mpnumber* hm, const mpnumber* y, const mpnumber* r, const mpnumber* s)
 * \brief This function performs raw ElGamal verification, variant 1.
 *
 * Verifying equations:
 *
 * \li Check \f$0<r<p\f$ and \f$0<s<(p-1)\f$
 * \li \f$v_1=y^{r}r^{s}\ \textrm{mod}\ p\f$
 * \li \f$v_2=g^{h(m)}\ \textrm{mod}\ p\f$
 * \li Check \f$v_1=v_2\f$
 *
 * \param p The prime.
 * \param n The reducer mod (p-1).
 * \param g The generator.
 * \param hm The hash to be signed.
 * \param y The public key value.
 * \param r The signature's \e r value.
 * \param s The signature's \e s value.
 * \retval 1 on success.
 * \retval 0 on failure.
 */
BEECRYPTAPI
int elgv3sign(const mpbarrett* p, const mpbarrett* n, const mpnumber* g, randomGeneratorContext*, const mpnumber* hm, const mpnumber* x, mpnumber* r, mpnumber* s);

/*!\fn int elgv3sign(const mpbarrett* p, const mpbarrett* n, const mpnumber* g, randomGeneratorContext* rgc, const mpnumber* hm, const mpnumber* x, mpnumber* r, mpnumber* s)
 * \brief This function performs raw ElGamal signing, variant 3.
 *
 * Signing equations:
 *
 * \li \f$r=g^{k}\ \textrm{mod}\ p\f$
 * \li \f$s=xr+kh(m)\ \textrm{mod}\ (p-1)\f$
 *
 * \param p The prime.
 * \param n The reducer mod (p-1).
 * \param g The generator.
 * \param rgc The pseudo-random generat
 * \param hm The hash to be signed.
 * \param x The private key value.
 * \param r The signature's \e r value.
 * \param s The signature's \e s value.
 * \retval 0 on success.
 * \retval -1 on failure.
 */
BEECRYPTAPI
int elgv1vrfy(const mpbarrett* p, const mpbarrett* n, const mpnumber* g, const mpnumber* hm, const mpnumber* y, const mpnumber* r, const mpnumber* s);

/*!\fn int elgv3vrfy(const mpbarrett* p, const mpbarrett* n, const mpnumber* g, const mpnumber* hm, const mpnumber* y, const mpnumber* r, const mpnumber* s)
 * \brief This function performs raw ElGamal verification, variant 3.
 *
 * Verifying equations:
 *
 * \li Check \f$0<r<p\f$ and \f$0<s<(p-1)\f$
 * \li \f$v_1=g^{s}\ \textrm{mod}\ p\f$
 * \li \f$v_2=y^{r}r^{h(m)}\ \textrm{mod}\ p\f$
 * \li Check \f$v_1=v_2\f$
 *
 * \param p The prime.
 * \param n The reducer mod (p-1).
 * \param g The generator.
 * \param hm The hash to be signed.
 * \param y The public key value.
 * \param r The signature's \e r value.
 * \param s The signature's \e s value.
 * \retval 1 on success.
 * \retval 0 on failure.
 */
BEECRYPTAPI
int elgv3vrfy(const mpbarrett* p, const mpbarrett* n, const mpnumber* g, const mpnumber* hm, const mpnumber* y, const mpnumber* r, const mpnumber* s);

#ifdef __cplusplus
}
#endif

#endif
