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

/*!\file rsapk.c
 * \brief RSA public key.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup IF_m IF_rsa_m
 */

#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/rsapk.h"

/*!\addtogroup IF_rsa_m
 * \{
 */

int rsapkInit(rsapk* pk)
{
	memset(pk, 0, sizeof(rsapk));
	/* or
	mpbzero(&pk->n);
	mpnzero(&pk->e);
	*/

	return 0;
}

int rsapkFree(rsapk* pk)
{
	mpbfree(&pk->n);
	mpnfree(&pk->e);

	return 0;
}

int rsapkCopy(rsapk* dst, const rsapk* src)
{
	mpbcopy(&dst->n, &src->n);
	mpncopy(&dst->e, &src->e);

	return 0;
}

/*!\}
 */
