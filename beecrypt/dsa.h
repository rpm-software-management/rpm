/*
 * Copyright (c) 2001, 2002 Virtual Unlimited B.V.
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

/*!\file dsa.h
 * \brief Digital Signature Algorithm, as specified by NIST FIPS 186.
 *
 * FIPS 186 specifies the DSA algorithm as having a large prime \f$p\f$,
 * a cofactor \f$q\f$ and a generator \f$g\f$ of a subgroup of
 * \f$\mathds{Z}^{*}_p\f$ with order \f$q\f$. The private and public key
 * values are \f$x\f$ and \f$y\f$ respectively.
 *
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup DL_dsa_m
 */

#ifndef _DSA_H
#define _DSA_H

#include "beecrypt/dlkp.h"

typedef dldp_p dsaparam;
typedef dlpk_p dsapub;
typedef dlkp_p dsakp;

#ifdef __cplusplus
extern "C" {
#endif

/*!\fn int dsasign(const mpbarrett* p, const mpbarrett* q, const mpnumber* g, randomGeneratorContext* rgc, const mpnumber* hm, const mpnumber* x, mpnumber* r, mpnumber* s)
 * \brief This function performs a raw DSA signature.
 *
 * Signing equations:
 *
 * \li \f$r=(g^{k}\ \textrm{mod}\ p)\ \textrm{mod}\ q\f$
 * \li \f$s=k^{-1}(h(m)+xr)\ \textrm{mod}\ q\f$
 *
 * \param p The prime.
 * \param q The cofactor.
 * \param g The generator.
 * \param rgc The pseudo-random generator context.
 * \param hm The hash to be signed.
 * \param x The private key value.
 * \param r The signature's \e r value.
 * \param s The signature's \e s value.
 * \retval 0 on success.
 * \retval -1 on failure.
 */
BEECRYPTAPI
int dsasign(const mpbarrett* p, const mpbarrett* q, const mpnumber* g, randomGeneratorContext*, const mpnumber* hm, const mpnumber* x, mpnumber* r, mpnumber* s);

/*!\fn int dsavrfy(const mpbarrett* p, const mpbarrett* q, const mpnumber* g, const mpnumber* hm, const mpnumber* y, const mpnumber* r, const mpnumber* s)
 * \brief This function performs a raw DSA verification.
 *
 * Verifying equations:
 * \li Check \f$0<r<q\f$ and \f$0<s<q\f$
 * \li \f$w=s^{-1}\ \textrm{mod}\ q\f$
 * \li \f$u_1=w \cdot h(m)\ \textrm{mod}\ q\f$
 * \li \f$u_2=rw\ \textrm{mod}\ q\f$
 * \li \f$v=(g^{u_1}y^{u_2}\ \textrm{mod}\ p)\ \textrm{mod}\ q\f$
 * \li Check \f$v=r\f$
 *
 * \param p The prime.
 * \param q The cofactor.
 * \param g The generator.
 * \param hm The digest to be verified.
 * \param y The public key value.
 * \param r The signature's \e r value.
 * \param s The signature's \e s value.
 * \retval 1 on success.
 * \retval 0 on failure.
 */
BEECRYPTAPI
int dsavrfy(const mpbarrett* p, const mpbarrett* q, const mpnumber* g, const mpnumber* hm, const mpnumber* y, const mpnumber* r, const mpnumber* s);

/*!\fn int dsaparamMake(dsaparam* dp, randomGeneratorContext* rgc, size_t psize)
 * \brief This function generates a set of DSA parameters.
 *
 * This function calls dldp_pgoqMake with appropriate parameters, i.e.
 * qsize  = 160 bits and cofactor = 1.
 *
 * \param dp The parameters to be generated.
 * \param rgc The random generator context.
 * \param psize The size of prime parameter p; psize must be >= 512 and <= 1024, and be a multiple of 64.
 * \retval 0 on success.
 * \retval -1 on failure.
 */
BEECRYPTAPI
int dsaparamMake(dsaparam*, randomGeneratorContext*, size_t);

#ifdef __cplusplus
}
#endif

#endif
