/*
 * dlkp.c
 *
 * Discrete Logarithm Keypair, code
 *
 * <conformance statement for IEEE P1363 needed here>
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

#define BEECRYPT_DLL_EXPORT

#include "dlkp.h"

void dlkp_pPair(dlkp_p* dp, randomGeneratorContext* rc, const dldp_p* param)
{
	/* copy the parameters */
	dldp_pCopy(&dp->param, param);

	dldp_pPair((const dldp_p*) param, rc, &dp->x, &dp->y);
}

void dlkp_pFree(dlkp_p* dp)
{
	dldp_pFree(&dp->param);

	mp32nfree(&dp->y);
	mp32nfree(&dp->x);
}

void dlkp_pCopy(dlkp_p* dst, const dlkp_p* src)
{
	dldp_pCopy(&dst->param, &src->param);

	mp32nset(&dst->y, src->y.size, src->y.data);
	mp32nset(&dst->x, src->x.size, src->x.data);
}
