/** \ingroup DL_m
 * \file dlkp.c
 *
 * Discrete Logarithm Keypair, code.
 */

/*
 * <conformance statement for IEEE P1363 needed here>
 *
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

#include "system.h"
#include "dlkp.h"
#include "debug.h"

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

	mp32nzero(&kp->y);
	mp32nzero(&kp->x);

	return 0;
}

int dlkp_pFree(dlkp_p* kp)
{
	/*@-usereleased -compdef @*/ /* kp->param.{p,q,n}.modl is OK */
	if (dldp_pFree(&kp->param) < 0)
		return -1;

	mp32nfree(&kp->y);
	mp32nfree(&kp->x);

	return 0;
	/*@=usereleased =compdef @*/
}

int dlkp_pCopy(dlkp_p* dst, const dlkp_p* src)
{
	if (dldp_pCopy(&dst->param, &src->param) < 0)
		return -1;

	mp32ncopy(&dst->y, &src->y);
	mp32ncopy(&dst->x, &src->x);

	return 0;
}
