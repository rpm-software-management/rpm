/*
 * dldp.c
 *
 * Discrete Logarithm Domain Parameters, code
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

#include "dldp.h"
#include "mp32.h"
#include "mp32prime.h"

#include <stdio.h>

void dldp_pPrivate(const dldp_p* dp, randomGeneratorContext* rc, mp32number* x)
{
	/*
	 * Note: the private key is randomly selected to be smaller than q
	 *
	 * This is the variant of Diffie-Hellman as described in IEEE P1363
	 */

	mp32brnd(&dp->q, rc);
	mp32nset(x, dp->q.size, dp->q.data);
}

void dldp_pPublic(const dldp_p* dp, const mp32number* x, mp32number* y)
{
	/*
	 * Public key y is computed as g^x mod p
	 */

	mp32bpowmod(&dp->p, dp->g.size, dp->g.data, x->size, x->data);
	mp32nset(y, dp->p.size, dp->p.data);
}

void dldp_pPair(const dldp_p* dp, randomGeneratorContext* rc, mp32number* x, mp32number* y)
{
	/*
	 * Combination of the two previous functions
	 */

	mp32brnd(&dp->q, rc);
	mp32nset(x, dp->q.size, dp->q.data);
	mp32bpowmod(&dp->p, dp->g.size, dp->g.data, x->size, x->data);
	mp32nset(y, dp->p.size, dp->p.data);
}

int dldp_pEqual(const dldp_p* a, const dldp_p* b)
{
	return mp32eqx(a->p.size, a->p.modl, b->p.size, b->p.modl) &&
		mp32eqx(a->q.size, a->q.modl, b->q.size, b->q.modl) &&
		mp32eqx(a->g.size, a->g.data, b->g.size, b->g.data);
}

int dldp_pValidate(const dldp_p* dp, randomGeneratorContext* rc)
{
	/* check that p > 2 and p odd, then run miller-rabin test with t 50 */
	if (mp32even(dp->p.size, dp->p.modl))
		return 0;

	if (mp32pmilrab(&dp->p, rc, 50) == 0)
		return 0;

	/* check that q > 2 and q odd, then run miller-rabin test with t 50 */
	if (mp32even(dp->q.size, dp->q.modl))
		return 0;

	if (mp32pmilrab(&dp->q, rc, 50) == 0)
		return 0;

	/* check that 1 < g < p */
	if (mp32leone(dp->g.size, dp->g.data))
		return 0;

	if (mp32gex(dp->g.size, dp->g.data, dp->p.size, dp->p.modl))
		return 0;

	return 1;
}

void dldp_pInit(dldp_p* dp)
{
	mp32bzero(&dp->p);
	mp32bzero(&dp->q);
	mp32nzero(&dp->g);
	mp32nzero(&dp->r);
	mp32bzero(&dp->n);
}

void dldp_pFree(dldp_p* dp)
{
	mp32bfree(&dp->p);
	mp32bfree(&dp->q);
	mp32nfree(&dp->g);
	mp32nfree(&dp->r);
	mp32bfree(&dp->n);
}

void dldp_pCopy(dldp_p* dst, const dldp_p* src)
{
	mp32bset(&dst->p, src->p.size, src->p.modl);
	mp32bset(&dst->q, src->q.size, src->q.modl);
	mp32nset(&dst->r, src->r.size, src->r.data);
	mp32nset(&dst->g, src->g.size, src->g.data);
	mp32bset(&dst->n, src->n.size, src->n.modl);
}

void dldp_pgoqMake(dldp_p* dp, randomGeneratorContext* rc, uint32 psize, uint32 qsize, int cofactor)
{
	/*
	 * Generate parameters as described by IEEE P1363, A.16.1
	 */

	/* first generate q */
	mp32prnd(&dp->q, rc, qsize, mp32ptrials(qsize << 5), (const mp32number*) 0);

	/* generate p with the appropriate congruences */
	mp32prndconone(&dp->p, rc, psize, mp32ptrials(psize << 5), &dp->q, (const mp32number*) 0, &dp->r, cofactor);

	/* clear n */
	mp32bfree(&dp->n);

	dldp_pgoqGenerator(dp, rc);
}

void dldp_pgoqMakeSafe(dldp_p* dp, randomGeneratorContext* rc, uint32 psize)
{
	/*
	 * Generate parameters with a safe prime; p = 2q+1 i.e. r=1
	 *
	 */

	/* generate p */
	mp32prndsafe(&dp->p, rc, psize, mp32ptrials(psize << 5));

	/* set q */
	mp32copy(dp->p.size, dp->p.data, dp->p.modl);
	mp32divtwo(dp->p.size, dp->p.data);

	mp32bset(&dp->q, dp->p.size, dp->p.data);

	/* set r = 1 */
	mp32nsetw(&dp->r, 1);

	/* clear n */
	mp32bfree(&dp->n);

	dldp_pgoqGenerator(dp, rc);
}

void dldp_pgoqGenerator(dldp_p* dp, randomGeneratorContext* rc)
{
	/*
	 * Randomly determine a generator over the subgroup with order q
	 */

	register uint32  psize = dp->p.size;
	register uint32* hdata = dp->p.data+psize*4+2;

	while (1)
	{
		mp32brndres(&dp->p, hdata, rc);

		mp32bpowmod(&dp->p, psize, hdata, dp->r.size, dp->r.data);
		if (mp32isone(psize, dp->p.data))
			continue;

		mp32nset(&dp->g, psize, dp->p.data);
		return;
	}
}

int dldp_pgoqValidate(const dldp_p* dp, randomGeneratorContext* rc, int cofactor)
{
	if (dldp_pValidate(dp, rc) == 0)
		return 0;

	/* check that g^q mod p = 1 */

	/* if r != 0, then check that 2qr+1 = p */

	/* if cofactor, then check that q does not divide (2r) */

	return 1;
}

void dldp_pgonMake(dldp_p* dp, randomGeneratorContext* rc, uint32 psize, uint32 qsize)
{
	/*
	 * Generate parameters with a prime p such that p = 2qr+1, with q and r prime
	 */

	/* generate q */
	mp32prnd(&dp->q, rc, qsize, mp32ptrials(qsize << 5), (const mp32number*) 0);

	/* generate p with the appropriate congruences */
	mp32prndconone(&dp->p, rc, psize, mp32ptrials(psize << 5), &dp->q, (const mp32number*) 0, &dp->r, 2);

	/* set n */
	mp32bmodsubone(&dp->p);
	mp32bset(&dp->n, psize, dp->p.data);

	dldp_pgonGenerator(dp, rc);
}

void dldp_pgonMakeSafe(dldp_p* dp, randomGeneratorContext* rc, uint32 psize)
{
	/*
	 * Generate parameters with a safe prime; i.e. p = 2q+1, where q is prime
	 */

	/* generate safe p */
	mp32prndsafe(&dp->p, rc, psize, mp32ptrials(psize << 5));

	/* set n */
	mp32bmodsubone(&dp->p);
	mp32bset(&dp->n, dp->p.size, dp->p.data);

	/* set q */
	mp32divtwo(dp->p.size, dp->p.data);
	mp32bset(&dp->q, dp->p.size, dp->p.data);

	/* set r = 1 */
	mp32nsetw(&dp->r, 1);

	dldp_pgonGenerator(dp, rc);
}

void dldp_pgonGenerator(dldp_p* dp, randomGeneratorContext* rc)
{
	register uint32  psize = dp->p.size;
	register uint32* gdata = dp->p.data+psize*4+2;

	while (1)
	{
		mp32brndres(&dp->p, gdata, rc);

		if (mp32isone(dp->r.size, dp->r.data))
		{
			/*
			 * A little math here: the only element in the group which has order 2 is (p-1);
			 * the two group elements raised to power two which result in 1 (mod p) are thus (p-1) and 1
			 *
			 * mp32brndres doesn't return 1 or (p-1), so the test where g^2 mod p = 1 can be safely skipped
			 */

			#if 0
			/* first check g^2 mod p */
			mp32bsqrmod(&dp->p, psize, gdata);
			if (mp32isone(psize, dp->p.data))
				continue;
			#endif

			/* check g^q mod p*/
			mp32bpowmod(&dp->p, psize, gdata, dp->q.size, dp->q.modl);
			if (mp32isone(psize, dp->p.data))
				continue;
		}
		else
		{
			/* we need g^2r, g^2q and g^qr, hence we first compute t = g^r
			 * then compute t^2 mod p, and test if one
			 * then compute t^q mod p, and test if one
			 * then compute (g^q mod p)^2 mod p, and test if one
			 */

			register uint32* tdata = gdata+psize;

			/* compute t = g^r mod p */			
			mp32bpowmod(&dp->p, psize, gdata, dp->r.size, dp->r.data);
			mp32copy(psize, tdata, dp->p.data);
			/* compute t^2 mod p = g^2r mod p */
			mp32bsqrmod(&dp->p, psize, dp->p.data);
			if (mp32isone(psize, dp->p.data))
				continue;

			/* compute t^q mod p = g^qr mod p */
			mp32bpowmod(&dp->p, psize, tdata, dp->q.size, dp->q.data);
			if (mp32isone(psize, dp->p.data))
				continue;

			/* compute g^2q mod p */
			mp32bpowmod(&dp->p, psize, gdata, dp->q.size, dp->q.modl);
			mp32bsqrmod(&dp->p, psize, dp->p.data);
			if (mp32isone(psize, dp->p.data))
				continue;
		}

		mp32nset(&dp->g, psize, dp->p.data);

		return;
	}
}

int dldp_pgonValidate(const dldp_p* dp, randomGeneratorContext* rc)
{
	if (dldp_pValidate((const dldp_p*) dp, rc) == 0)
		return 0;

	return 1;
}
