/** \ingroup DL_m
 * \file dldp.c
 *
 * Discrete Logarithm Domain Parameters, code.
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

#define BEECRYPT_DLL_EXPORT

#include "dldp.h"
#include "mp32.h"
#include "mp32prime.h"

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_MALLOC
# include <malloc.h>
#endif

/**
 */
static int dldp_pgoqGenerator_w(dldp_p* dp, randomGeneratorContext* rgc, /*@out@*/ uint32* wksp)
	/*@modifies dp, wksp @*/;

/**
 */
static int dldp_pgonGenerator_w(dldp_p* dp, randomGeneratorContext* rgc, /*@out@*/ uint32* wksp)
	/*@modifies dp, wksp @*/;

int dldp_pPrivate(const dldp_p* dp, randomGeneratorContext* rgc, mp32number* x)
{
	/*
	 * Note: the private key is randomly selected to be smaller than q
	 *
	 * This is the variant of Diffie-Hellman as described in IEEE P1363
	 */

	mp32bnrnd(&dp->q, rgc, x);

	return 0;
}

int dldp_pPublic(const dldp_p* dp, const mp32number* x, mp32number* y)
{
	/*
	 * Public key y is computed as g^x mod p
	 */

	mp32bnpowmod(&dp->p, &dp->g, x, y);

	return 0;
}

int dldp_pPair(const dldp_p* dp, randomGeneratorContext* rgc, mp32number* x, mp32number* y)
{
	/*
	 * Combination of the two previous functions
	 */

	mp32bnrnd(&dp->q, rgc, x);
	mp32bnpowmod(&dp->p, &dp->g, x, y);

	return 0;
}

int dldp_pEqual(const dldp_p* a, const dldp_p* b)
{
	return mp32eqx(a->p.size, a->p.modl, b->p.size, b->p.modl) &&
		mp32eqx(a->q.size, a->q.modl, b->q.size, b->q.modl) &&
		mp32eqx(a->g.size, a->g.data, b->g.size, b->g.data);
}

/**
 * needs to make workspace of 8*size+2
 */
static int dldp_pValidate(const dldp_p* dp, randomGeneratorContext* rgc)
	/*@*/
{
	register uint32  size = dp->p.size;
	register uint32* temp = (uint32*) malloc((8*size+2) * sizeof(uint32));

	if (temp)
	{
		/* check that p > 2 and p odd, then run miller-rabin test with t 50 */
		if (mp32even(dp->p.size, dp->p.modl))
		{
			free(temp);
			return 0;
		}

		if (mp32pmilrab_w(&dp->p, rgc, 50, temp) == 0)
		{
			free(temp);
			return 0;
		}

		/* check that q > 2 and q odd, then run miller-rabin test with t 50 */
		if (mp32even(dp->q.size, dp->q.modl))
		{
			free(temp);
			return 0;
		}

		if (mp32pmilrab_w(&dp->q, rgc, 50, temp) == 0)
		{
			free(temp);
			return 0;
		}

		free(temp);

		/* check that 1 < g < p */
		if (mp32leone(dp->g.size, dp->g.data))
			return 0;

		if (mp32gex(dp->g.size, dp->g.data, dp->p.size, dp->p.modl))
			return 0;

		return 1;
	}
	return -1;
}

int dldp_pInit(dldp_p* dp)
{
	mp32bzero(&dp->p);
	mp32bzero(&dp->q);
	mp32nzero(&dp->g);
	mp32nzero(&dp->r);
	mp32bzero(&dp->n);

	return 0;
}

int dldp_pFree(dldp_p* dp)
{
	mp32bfree(&dp->p);
	mp32bfree(&dp->q);
	mp32nfree(&dp->g);
	mp32nfree(&dp->r);
	mp32bfree(&dp->n);

	return 0;
}

int dldp_pCopy(dldp_p* dst, const dldp_p* src)
{
	mp32bcopy(&dst->p, &src->p);
	mp32bcopy(&dst->q, &src->q);
	mp32ncopy(&dst->r, &src->r);
	mp32ncopy(&dst->g, &src->g);
	mp32bcopy(&dst->n, &src->n);

	return 0;
}

int dldp_pgoqMake(dldp_p* dp, randomGeneratorContext* rgc, uint32 psize, uint32 qsize, int cofactor)
{
	/*
	 * Generate parameters as described by IEEE P1363, A.16.1
	 */

	register uint32* temp = (uint32*) malloc((8*psize+2) * sizeof(uint32));

	if (temp)
	{
		/* first generate q */
		mp32prnd_w(&dp->q, rgc, qsize, mp32ptrials(qsize << 5), (const mp32number*) 0, temp);

		/* generate p with the appropriate congruences */
		mp32prndconone_w(&dp->p, rgc, psize, mp32ptrials(psize << 5), &dp->q, (const mp32number*) 0, &dp->r, cofactor, temp);

		/* clear n */
		mp32bzero(&dp->n);

		/* clear g */
		mp32nzero(&dp->g);

		(void) dldp_pgoqGenerator_w(dp, rgc, temp);

		free(temp);

		return 0;
	}
	return -1;
}

int dldp_pgoqMakeSafe(dldp_p* dp, randomGeneratorContext* rgc, uint32 psize)
{
	/*
	 * Generate parameters with a safe prime; p = 2q+1 i.e. r=2
	 *
	 */

	register uint32* temp = (uint32*) malloc((8*psize+2) * sizeof(uint32));

	if (temp)
	{
		/* generate p */
		mp32prndsafe_w(&dp->p, rgc, psize, mp32ptrials(psize << 5), temp);

		/* set q */
		mp32copy(psize, temp, dp->p.modl);
		mp32divtwo(psize, temp);
		mp32bset(&dp->q, psize, temp);

		/* set r = 2 */
		mp32nsetw(&dp->r, 2);

		/* clear n */
		mp32bzero(&dp->n);

		(void) dldp_pgoqGenerator_w(dp, rgc, temp);

		free(temp);

		return 0;
	}
	return -1;
}

int dldp_pgoqGenerator_w(dldp_p* dp, randomGeneratorContext* rgc, uint32* wksp)
{
	/*
	 * Randomly determine a generator over the subgroup with order q
	 */

	register uint32  size = dp->p.size;

	mp32nfree(&dp->g);
	mp32nsize(&dp->g, size);

	while (1)
	{
		/* get a random value h (stored into g) */
		mp32brnd_w(&dp->p, rgc, dp->g.data, wksp);

		/* first compute h^r mod p (stored in g) */
		mp32bpowmod_w(&dp->p, size, dp->g.data, dp->r.size, dp->r.data, dp->g.data, wksp);

		if (mp32isone(size, dp->g.data))
			continue;

		return 0;
	}
	return -1;
}

int dldp_pgoqGenerator(dldp_p* dp, randomGeneratorContext* rgc)
{
	register uint32  size = dp->p.size;
	register uint32* temp = (uint32*) malloc((4*size+2)*sizeof(uint32));

	if (temp)
	{
		(void) dldp_pgoqGenerator_w(dp, rgc, temp);

		free(temp);

		return 0;
	}
	return -1;
}

int dldp_pgoqValidate(const dldp_p* dp, randomGeneratorContext* rgc, /*@unused@*/ int cofactor)
{
	register int rc = dldp_pValidate(dp, rgc);

	if (rc <= 0)
		return rc;

	/* check that g^q mod p = 1 */

	/* if r != 0, then check that qr+1 = p */

	/* if cofactor, then check that q does not divide (r) */

	return 1;
}

int dldp_pgonMake(dldp_p* dp, randomGeneratorContext* rgc, uint32 psize, uint32 qsize)
{
	/*
	 * Generate parameters with a prime p such that p = qr+1, with q prime, and r = 2s, with s prime
	 */

	register uint32* temp = (uint32*) malloc((8*psize+2) * sizeof(uint32));

	if (temp)
	{
		/* generate q */
		mp32prnd_w(&dp->q, rgc, qsize, mp32ptrials(qsize << 5), (const mp32number*) 0, temp);

		/* generate p with the appropriate congruences */
		mp32prndconone_w(&dp->p, rgc, psize, mp32ptrials(psize << 5), &dp->q, (const mp32number*) 0, &dp->r, 2, temp);

		/* set n */
		mp32bsubone(&dp->p, temp);
		mp32bset(&dp->n, psize, temp);

		(void) dldp_pgonGenerator_w(dp, rgc, temp);

		free(temp);

		return 0;
	}
	return -1;
}

int dldp_pgonMakeSafe(dldp_p* dp, randomGeneratorContext* rgc, uint32 psize)
{
	/*
	 * Generate parameters with a safe prime; i.e. p = 2q+1, where q is prime
	 */

	register uint32* temp = (uint32*) malloc((8*psize+2) * sizeof(uint32));

	if (temp)
	{
		/* generate safe p */
		mp32prndsafe_w(&dp->p, rgc, psize, mp32ptrials(psize << 5), temp);

		/* set n */
		mp32bsubone(&dp->p, temp);
		mp32bset(&dp->n, psize, temp);

		/* set q */
		mp32copy(psize, temp, dp->p.modl);
		mp32divtwo(psize, temp);
		mp32bset(&dp->q, psize, temp);

		/* set r = 2 */
		mp32nsetw(&dp->r, 2);

		(void) dldp_pgonGenerator_w(dp, rgc, temp);

		free(temp);

		return 0;
	}
	return -1;
}

int dldp_pgonGenerator_w(dldp_p* dp, randomGeneratorContext* rgc, uint32* wksp)
{
	register uint32  size = dp->p.size;

	mp32nfree(&dp->g);
	mp32nsize(&dp->g, size);

	while (1)
	{
		mp32brnd_w(&dp->p, rgc, dp->g.data, wksp);

		if (mp32istwo(dp->r.size, dp->r.data))
		{
			/*
			 * A little math here: the only element in the group which has order 2 is (p-1);
			 * the two group elements raised to power two which result in 1 (mod p) are thus (p-1) and 1
			 *
			 * mp32brnd_w doesn't return 1 or (p-1), so the test where g^2 mod p = 1 can be safely skipped
			 */

			/* check g^q mod p*/
			mp32bpowmod_w(&dp->p, size, dp->g.data, dp->q.size, dp->q.modl, wksp, wksp+size);
			if (mp32isone(size, wksp))
				continue;
		}
		else
		{
			/* we can either compute g^r, g^2q and g^(qr/2) or
			 * we first compute s = r/2, and then compute g^2s, g^2q and g^qs
			 *
			 * hence we first compute t = g^s
			 * then compute t^2 mod p, and test if one
			 * then compute t^q mod p, and test if one
			 * then compute (g^q mod p)^2 mod p, and test if one
			 */

			/* compute s = r/2 */
			mp32setx(size, wksp, dp->r.size, dp->r.data);
			mp32divtwo(size, wksp);

			/* compute t = g^s mod p */
			mp32bpowmod_w(&dp->p, size, dp->g.data, size, wksp, wksp+size, wksp+2*size);
			/* compute t^2 mod p = g^2s mod p = g^r mod p*/
			mp32bsqrmod_w(&dp->p, size, wksp+size, wksp+size, wksp+2*size);
			if (mp32isone(size, wksp+size))
				continue;

			/* compute t^q mod p = g^qs mod p */
			mp32bpowmod_w(&dp->p, size, wksp, dp->q.size, dp->q.modl, wksp+size, wksp+2*size);
			if (mp32isone(size, wksp+size))
				continue;

			/* compute g^2q mod p */
			mp32bpowmod_w(&dp->p, size, dp->g.data, dp->q.size, dp->q.modl, wksp, wksp+size);
			mp32bsqrmod_w(&dp->p, size, wksp, wksp+size, wksp+2*size);
			if (mp32isone(size, wksp+size))
				continue;
		}

		return 0;
	}

	return -1;
}

int dldp_pgonGenerator(dldp_p* dp, randomGeneratorContext* rgc)
{
	register uint32  psize = dp->p.size;
	register uint32* temp = (uint32*) malloc((8*psize+2) * sizeof(uint32));

	if (temp)
	{
		(void) dldp_pgonGenerator_w(dp, rgc, temp);

		free(temp);

		return 0;
	}
	return -1;
}

int dldp_pgonValidate(const dldp_p* dp, randomGeneratorContext* rgc)
{
	return dldp_pValidate((const dldp_p*) dp, rgc);
}
