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

/*!\file dlkp.c
 * \brief Discrete Logarithm keypair.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup DL_m
 */

#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/dlkp.h"

int dlkp_pPair(dlkp_p* kp, randomGeneratorContext* rgc, const dldp_p* param)
{
	/* copy the parameters */
	if (dldp_pCopy(&kp->param, param) < 0)
		return -1;

	if (dldp_pPair(param, rgc, &kp->x, &kp->y) < 0)
		return -1;

	return 0;
}

int dlkp_pInit(dlkp_p* kp)
{
	if (dldp_pInit(&kp->param) < 0)
		return -1;

	mpnzero(&kp->y);
	mpnzero(&kp->x);

	return 0;
}

int dlkp_pFree(dlkp_p* kp)
{
	if (dldp_pFree(&kp->param) < 0)
		return -1;

	mpnfree(&kp->y);
	/* wipe secret key before freeing */
	mpnwipe(&kp->x);
	mpnfree(&kp->x);

	return 0;
}

int dlkp_pCopy(dlkp_p* dst, const dlkp_p* src)
{
	if (dldp_pCopy(&dst->param, &src->param) < 0)
		return -1;

	mpncopy(&dst->y, &src->y);
	mpncopy(&dst->x, &src->x);

	return 0;
}
