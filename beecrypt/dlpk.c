/*
 * dlpk.c
 *
 * Discrete Logarithm Public Key, code
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

#include "dlpk.h"
#include "mp32.h"

void dlpk_pFree(dlpk_p* dp)
{
	dldp_pFree(&dp->param);
	mp32nfree(&dp->y);
}

void dlpk_pCopy(dlpk_p* dst, const dlpk_p* src)
{
	dldp_pCopy(&dst->param, &src->param);
	mp32nset(&dst->y, src->y.size, src->y.data);
}

int dlpk_pEqual(const dlpk_p* a, const dlpk_p* b)
{
	return dldp_pEqual(&a->param, &b->param) &&
		mp32eqx(a->y.size, a->y.data, b->y.size, b->y.data);
}

int dlpk_pgoqValidate(const dlpk_p* dp, randomGeneratorContext* rc, int cofactor)
{
	if (dldp_pgoqValidate(&dp->param, rc, cofactor) == 0)
		return 0;

	if (mp32leone(dp->y.size, dp->y.data))
		return 0;

	if (mp32gex(dp->y.size, dp->y.data, dp->param.p.size, dp->param.p.modl))
		return 0;

	return 1;
}

int dlpk_pgonValidate(const dlpk_p* dp, randomGeneratorContext* rc)
{
	if (dldp_pgonValidate(&dp->param, rc) == 0)
		return 0;

	if (mp32leone(dp->y.size, dp->y.data))
		return 0;

	if (mp32gex(dp->y.size, dp->y.data, dp->param.p.size, dp->param.p.data))
		return 0;

	return 1;
}
