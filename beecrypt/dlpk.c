/** \ingroup DL_m
 * \file dlpk.c
 *
 * Discrete Logarithm Public Key, code.
 */

/*
 * Copyright (c) 2000, 2001 Virtual Unlimited B.V.
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

int dlpk_pInit(dlpk_p* pk)
{
	if (dldp_pInit(&pk->param) < 0)
		return -1;

	mp32nzero(&pk->y);

	return 0;
}

int dlpk_pFree(dlpk_p* pk)
{
	if (dldp_pFree(&pk->param) < 0)
		return -1;

	mp32nfree(&pk->y);

	return 0;
}

int dlpk_pCopy(dlpk_p* dst, const dlpk_p* src)
{
	if (dldp_pCopy(&dst->param, &src->param) < 0)
		return -1;

	mp32ncopy(&dst->y, &src->y);

	return 0;
}

int dlpk_pEqual(const dlpk_p* a, const dlpk_p* b)
{
	return dldp_pEqual(&a->param, &b->param) &&
		mp32eqx(a->y.size, a->y.data, b->y.size, b->y.data);
}

int dlpk_pgoqValidate(const dlpk_p* pk, randomGeneratorContext* rgc, int cofactor)
{
	register int rc = dldp_pgoqValidate(&pk->param, rgc, cofactor);

	if (rc <= 0)
		return rc;

	if (mp32leone(pk->y.size, pk->y.data))
		return 0;

	if (mp32gex(pk->y.size, pk->y.data, pk->param.p.size, pk->param.p.modl))
		return 0;

	return 1;
}

int dlpk_pgonValidate(const dlpk_p* pk, randomGeneratorContext* rgc)
{
	register int rc = dldp_pgonValidate(&pk->param, rgc);

	if (rc <= 0)
		return rc;

	if (mp32leone(pk->y.size, pk->y.data))
		return 0;

	if (mp32gex(pk->y.size, pk->y.data, pk->param.p.size, pk->param.p.modl))
		return 0;

	return 1;
}
