/** \ingroup RSA_m
 * \file rsakp.c
 *
 * RSA Keypair, code.
 */

/*
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

#include "system.h"
#include "rsakp.h"
#include "mp32prime.h"
#include "mp32.h"
#include "debug.h"

int rsakpMake(rsakp* kp, randomGeneratorContext* rgc, int nsize)
{
	/* 
	 * Generates an RSA Keypair for use with the Chinese Remainder Theorem
	 */

	register uint32  pqsize = ((uint32)(nsize+1)) >> 1;
	register uint32* temp = (uint32*) malloc((16*pqsize+6) * sizeof(*temp));
	register uint32  newn = 1;

	if (temp)
	{
		mp32barrett r, psubone, qsubone, phi;

		nsize = pqsize << 1;

		/* set e */
		mp32nsetw(&kp->e, 65535);

		/* generate a random prime p and q */
		/*@-globs@*/
		mp32prnd_w(&kp->p, rgc, pqsize, mp32ptrials(pqsize << 5), &kp->e, temp);
		mp32prnd_w(&kp->q, rgc, pqsize, mp32ptrials(pqsize << 5), &kp->e, temp);
		/*@-globs@*/

		/* if p <= q, perform a swap to make p larger than q */
		if (mp32le(pqsize, kp->p.modl, kp->q.modl))
		{
			/*@-sizeoftype@*/
			memcpy(&r, &kp->q, sizeof(mp32barrett));
			memcpy(&kp->q, &kp->p, sizeof(mp32barrett));
			memcpy(&kp->p, &r, sizeof(mp32barrett));
			/*@=sizeoftype@*/
		}

		mp32bzero(&r);
		mp32bzero(&psubone);
		mp32bzero(&qsubone);
		mp32bzero(&phi);

		while (1)
		{
			mp32mul(temp, pqsize, kp->p.modl, pqsize, kp->q.modl);

			if (newn && mp32msbset(nsize, temp))
				break;

			/* product of p and q doesn't have the required size (one bit short) */

			/*@-globs@*/
			mp32prnd_w(&r, rgc, pqsize, mp32ptrials(pqsize << 5), &kp->e, temp);
			/*@=globs@*/

			/*@-usedef -branchstate @*/ /* r is set */
			if (mp32le(pqsize, kp->p.modl, r.modl))
			{
				mp32bfree(&kp->q);
				/*@-sizeoftype@*/
				memcpy(&kp->q, &kp->p, sizeof(mp32barrett));
				memcpy(&kp->p, &r, sizeof(mp32barrett));
				/*@=sizeoftype@*/
				mp32bzero(&r);
				newn = 1;
			}
			else if (mp32le(pqsize, kp->q.modl, r.modl))
			{
				mp32bfree(&kp->q);
				/*@-sizeoftype@*/
				memcpy(&kp->q, &r, sizeof(mp32barrett));
				/*@=sizeoftype@*/
				mp32bzero(&r);
				newn = 1;
			}
			else
			{
				mp32bfree(&r);
				newn = 0;
			}
			/*@=usedef =branchstate @*/
		}

		mp32bset(&kp->n, nsize, temp);

		/* compute p-1 */
		mp32bsubone(&kp->p, temp);
		mp32bset(&psubone, pqsize, temp);

		/* compute q-1 */
		mp32bsubone(&kp->q, temp);
		mp32bset(&qsubone, pqsize, temp);

		/*@-usedef@*/	/* psubone/qsubone are set */
		/* compute phi = (p-1)*(q-1) */
		mp32mul(temp, pqsize, psubone.modl, pqsize, qsubone.modl);
		mp32bset(&phi, nsize, temp);

		/* compute d = inv(e) mod phi */
		mp32nsize(&kp->d, nsize);
		(void) mp32binv_w(&phi, kp->e.size, kp->e.data, kp->d.data, temp);

		/* compute d1 = d mod (p-1) */
		mp32nsize(&kp->d1, pqsize);
		mp32bmod_w(&psubone, kp->d.data, kp->d1.data, temp);

		/* compute d2 = d mod (q-1) */
		mp32nsize(&kp->d2, pqsize);
		mp32bmod_w(&qsubone, kp->d.data, kp->d2.data, temp);

		/* compute c = inv(q) mod p */
		mp32nsize(&kp->c, pqsize);
		(void) mp32binv_w(&kp->p, pqsize, kp->q.modl, kp->c.data, temp);

		free(temp);
		/*@=usedef@*/

		return 0;
	}
	return -1;
}

int rsakpInit(rsakp* kp)
{
	memset(kp, 0, sizeof(*kp));
	/* or
	mp32bzero(&kp->n);
	mp32nzero(&kp->e);
	mp32nzero(&kp->d);
	mp32bzero(&kp->p);
	mp32bzero(&kp->q);
	mp32nzero(&kp->d1);
	mp32nzero(&kp->d2);
	mp32nzero(&kp->c);
	*/

	return 0;
}

int rsakpFree(rsakp* kp)
{
	/*@-usereleased -compdef @*/ /* kp->param.{n,p,q}.modl is OK */
	mp32bfree(&kp->n);
	mp32nfree(&kp->e);
	mp32nfree(&kp->d);
	mp32bfree(&kp->p);
	mp32bfree(&kp->q);
	mp32nfree(&kp->d1);
	mp32nfree(&kp->d2);
	mp32nfree(&kp->c);

	return 0;
	/*@=usereleased =compdef @*/
}

int rsakpCopy(rsakp* dst, const rsakp* src)
{
	mp32bcopy(&dst->n, &src->n);
	mp32ncopy(&dst->e, &src->e);
	mp32ncopy(&dst->d, &src->d);
	mp32bcopy(&dst->p, &src->p);
	mp32bcopy(&dst->q, &src->q);
	mp32ncopy(&dst->d1, &src->d1);
	mp32ncopy(&dst->d2, &src->d2);
	mp32ncopy(&dst->c, &src->c);

	return 0;
}
