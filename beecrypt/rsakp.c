/*
 * Copyright (c) 2000, 2002 Virtual Unlimited B.V.
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

/*!\file rsakp.c
 * \brief RSA keypair.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup IF_m IF_rsa_m
 */

#include "system.h"
#include "rsakp.h"
#include "mpprime.h"
#include "mp.h"
#include "debug.h"

/*!\addtogroup IF_rsa_m
 * \{
 */

int rsakpMake(rsakp* kp, randomGeneratorContext* rgc, size_t nsize)
{
	/* 
	 * Generates an RSA Keypair for use with the Chinese Remainder Theorem
	 */

	register size_t pqsize = (nsize + 1U) >> 1;
	register mpw* temp = (mpw*) malloc((16*pqsize+6) * sizeof(*temp));
	register int newn = 1;

	if (temp)
	{
		mpbarrett r, psubone, qsubone;
		mpnumber phi;

		nsize = pqsize << 1;

		/* set e */
		mpnsetw(&kp->e, 65535);

		/* generate a random prime p and q */
		mpprnd_w(&kp->p, rgc, pqsize, mpptrials(MP_WORDS_TO_BITS(pqsize)), &kp->e, temp);
		mpprnd_w(&kp->q, rgc, pqsize, mpptrials(MP_WORDS_TO_BITS(pqsize)), &kp->e, temp);

		/* if p <= q, perform a swap to make p larger than q */
		if (mple(pqsize, kp->p.modl, kp->q.modl))
		{
			memcpy(&r, &kp->q, sizeof(r));
			memcpy(&kp->q, &kp->p, sizeof(kp->q));
			memcpy(&kp->p, &r, sizeof(kp->p));
		}

		mpbzero(&r);
		mpbzero(&psubone);
		mpbzero(&qsubone);
		mpnzero(&phi);

		while (1)
		{
			mpmul(temp, pqsize, kp->p.modl, pqsize, kp->q.modl);

			if (newn && mpmsbset(nsize, temp))
				break;

			/* product of p and q doesn't have the required size (one bit short) */

			mpprnd_w(&r, rgc, pqsize, mpptrials(MP_WORDS_TO_BITS(pqsize)), &kp->e, temp);

			/*@-usedef -branchstate @*/ /* r is set */
			if (mple(pqsize, kp->p.modl, r.modl))
			{
				mpbfree(&kp->q);
				memcpy(&kp->q, &kp->p, sizeof(kp->q));
				memcpy(&kp->p, &r, sizeof(kp->p));
				mpbzero(&r);
				newn = 1;
			}
			else if (mple(pqsize, kp->q.modl, r.modl))
			{
				mpbfree(&kp->q);
				memcpy(&kp->q, &r, sizeof(kp->q));
				mpbzero(&r);
				newn = 1;
			}
			else
			{
				mpbfree(&r);
				newn = 0;
			}
			/*@=usedef =branchstate @*/
		}

		mpbset(&kp->n, nsize, temp);

		/* compute p-1 */
		mpbsubone(&kp->p, temp);
		mpbset(&psubone, pqsize, temp);

		/* compute q-1 */
		mpbsubone(&kp->q, temp);
		mpbset(&qsubone, pqsize, temp);

		/*@-usedef@*/	/* psubone/qsubone are set */
		/* compute phi = (p-1)*(q-1) */
		mpmul(temp, pqsize, psubone.modl, pqsize, qsubone.modl);
		mpbset(&phi, nsize, temp);

		/* compute d = inv(e) mod phi */
		(void) mpninv(&kp->d, &kp->e, &phi);

		/* compute d1 = d mod (p-1) */
		mpnsize(&kp->d1, pqsize);
		mpbmod_w(&psubone, kp->d.data, kp->d1.data, temp);

		/* compute d2 = d mod (q-1) */
		mpnsize(&kp->d2, pqsize);
		mpbmod_w(&qsubone, kp->d.data, kp->d2.data, temp);

		/* compute c = inv(q) mod p */
		(void) mpninv(&kp->c, (const mpnumber*) &kp->q, (const mpnumber*) &kp->p);

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
	mpbzero(&kp->n);
	mpnzero(&kp->e);
	mpnzero(&kp->d);
	mpbzero(&kp->p);
	mpbzero(&kp->q);
	mpnzero(&kp->d1);
	mpnzero(&kp->d2);
	mpnzero(&kp->c);
	*/

	return 0;
}

int rsakpFree(rsakp* kp)
{
	/*@-usereleased -compdef @*/ /* kp->param.{n,p,q}.modl is OK */
	mpbfree(&kp->n);
	mpnfree(&kp->e);
	mpnfree(&kp->d);
	mpbfree(&kp->p);
	mpbfree(&kp->q);
	mpnfree(&kp->d1);
	mpnfree(&kp->d2);
	mpnfree(&kp->c);

	return 0;
	/*@=usereleased =compdef @*/
}

int rsakpCopy(rsakp* dst, const rsakp* src)
{
	mpbcopy(&dst->n, &src->n);
	mpncopy(&dst->e, &src->e);
	mpncopy(&dst->d, &src->d);
	mpbcopy(&dst->p, &src->p);
	mpbcopy(&dst->q, &src->q);
	mpncopy(&dst->d1, &src->d1);
	mpncopy(&dst->d2, &src->d2);
	mpncopy(&dst->c, &src->c);

	return 0;
}

/*!\}
 */
