/*
 * Copyright (c) 1999, 2000, 2001, 2002 Virtual Unlimited B.V.
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

/*!\file dlsvdp-dh.c
 * \brief Diffie-Hellman algorithm.
 *
 * The IEEE P.1363 designation is:
 * Discrete Logarithm Secret Value Derivation Primitive, Diffie-Hellman style.
 *
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup DL_m DL_dh_m
 */

#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/dlsvdp-dh.h"

/*!\addtogroup DL_dh_m
 * \{
 */

/*!\fn dlsvdp_pDHSecret(const dhparam* dp, const mpnumber* x, const mpnumber* y, mpnumber* s)
 * \brief Computes the shared secret.
 *
 * Equation:
 *
 * \li \f$s=y^{x}\ \textrm{mod}\ p\f$
 *
 * \param dp The domain parameters.
 * \param x The private value.
 * \param y The public value (of the peer).
 * \param s The computed secret value.
 *
 * \retval 0 on success.
 * \retval -1 on failure.
 */
int dlsvdp_pDHSecret(const dhparam* dp, const mpnumber* x, const mpnumber* y, mpnumber* s)
{
	mpbnpowmod(&dp->p, y, x, s);

	return 0;
}

/*!\}
 */
