/*
 * Copyright (c) 2000, 2001, 2002, 2003 Virtual Unlimited B.V.
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

/*!\file dldp.c
 * \brief Discrete Logarithm domain parameters.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup DL_m
 */

#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/dldp.h"
#include "beecrypt/mp.h"
#include "beecrypt/mpprime.h"

/*!\addtogroup DL_m
 * \{
 */
static int dldp_pgoqGenerator_w(dldp_p*, randomGeneratorContext*, mpw*);
static int dldp_pgonGenerator_w(dldp_p*, randomGeneratorContext*, mpw*);

int dldp_pPrivate(const dldp_p* dp, randomGeneratorContext* rgc, mpnumber* x)
{
	/*
	 * Note: the private key is randomly selected to be smaller than q
	 *
	 * This is the variant of Diffie-Hellman as described in IEEE P1363
	 */

	mpbnrnd(&dp->q, rgc, x);

	return 0;
}

int dldp_pPrivate_s(const dldp_p* dp, randomGeneratorContext* rgc, mpnumber* x, size_t xbits)
{
	/*
	 * Note: the private key is randomly selected smaller than q with xbits < mpbits(q)
	 *
	 */

	mpbnrnd(&dp->q, rgc, x);
	mpntrbits(x, xbits);

	return 0;
}

int dldp_pPublic(const dldp_p* dp, const mpnumber* x, mpnumber* y)
{
	/*
	 * Public key y is computed as g^x mod p
	 */

	mpbnpowmod(&dp->p, &dp->g, x, y);

	return 0;
}

int dldp_pPair(const dldp_p* dp, randomGeneratorContext* rgc, mpnumber* x, mpnumber* y)
{
	/*
	 * Combination of the two previous functions
	 */

	mpbnrnd(&dp->q, rgc, x);
	mpbnpowmod(&dp->p, &dp->g, x, y);

	return 0;
}

int dldp_pPair_s(const dldp_p* dp, randomGeneratorContext* rgc, mpnumber* x, mpnumber* y, size_t xbits)
{
	mpbnrnd(&dp->q, rgc, x);
	mpntrbits(x, xbits);
	mpbnpowmod(&dp->p, &dp->g, x, y);

	return 0;
}

int dldp_pEqual(const dldp_p* a, const dldp_p* b)
{
	return mpeqx(a->p.size, a->p.modl, b->p.size, b->p.modl) &&
		mpeqx(a->q.size, a->q.modl, b->q.size, b->q.modl) &&
		mpeqx(a->g.size, a->g.data, b->g.size, b->g.data);
}

/*
 * needs to make workspace of 8*size+2
 */
int dldp_pValidate(const dldp_p* dp, randomGeneratorContext* rgc)
{
	register size_t size = dp->p.size;

	register mpw* temp = (mpw*) malloc((8*size+2) * sizeof(mpw));

	if (temp)
	{
		/* check that p > 2 and p odd, then run miller-rabin test with t 50 */
		if (mpeven(dp->p.size, dp->p.modl))
		{
			free(temp);
			return 0;
		}

		if (mppmilrab_w(&dp->p, rgc, 50, temp) == 0)
		{
			free(temp);
			return 0;
		}

		/* check that q > 2 and q odd, then run miller-rabin test with t 50 */
		if (mpeven(dp->q.size, dp->q.modl))
		{
			free(temp);
			return 0;
		}

		if (mppmilrab_w(&dp->q, rgc, 50, temp) == 0)
		{
			free(temp);
			return 0;
		}

		free(temp);

		/* check that 1 < g < p */
		if (mpleone(dp->g.size, dp->g.data))
			return 0;

		if (mpgex(dp->g.size, dp->g.data, dp->p.size, dp->p.modl))
			return 0;

		return 1;
	}
	return -1;
}

int dldp_pInit(dldp_p* dp)
{
	mpbzero(&dp->p);
	mpbzero(&dp->q);
	mpnzero(&dp->g);
	mpnzero(&dp->r);
	mpbzero(&dp->n);

	return 0;
}

int dldp_pFree(dldp_p* dp)
{
	mpbfree(&dp->p);
	mpbfree(&dp->q);
	mpnfree(&dp->g);
	mpnfree(&dp->r);
	mpbfree(&dp->n);

	return 0;
}

int dldp_pCopy(dldp_p* dst, const dldp_p* src)
{
	mpbcopy(&dst->p, &src->p);
	mpbcopy(&dst->q, &src->q);
	mpncopy(&dst->r, &src->r);
	mpncopy(&dst->g, &src->g);
	mpbcopy(&dst->n, &src->n);

	return 0;
}

int dldp_pgoqMake(dldp_p* dp, randomGeneratorContext* rgc, size_t pbits, size_t qbits, int cofactor)
{
	/*
	 * Generate parameters as described by IEEE P1363, A.16.1
	 */
	register size_t psize = MP_BITS_TO_WORDS(pbits + MP_WBITS - 1);
	register mpw* temp = (mpw*) malloc((8*psize+2) * sizeof(mpw));

	if (temp)
	{
		/* first generate q */
		mpprnd_w(&dp->q, rgc, qbits, mpptrials(qbits), (const mpnumber*) 0, temp);

		/* generate p with the appropriate congruences */
		mpprndconone_w(&dp->p, rgc, pbits, mpptrials(pbits), &dp->q, (const mpnumber*) 0, &dp->r, cofactor, temp);

		/* clear n */
		mpbzero(&dp->n);

		/* clear g */
		mpnzero(&dp->g);

		dldp_pgoqGenerator_w(dp, rgc, temp);

		free(temp);

		return 0;
	}

	return -1;
}

int dldp_pgoqMakeSafe(dldp_p* dp, randomGeneratorContext* rgc, size_t bits)
{
	/*
	 * Generate parameters with a safe prime; p = 2q+1 i.e. r=2
	 *
	 */

	register size_t size = MP_BITS_TO_WORDS(bits + MP_WBITS - 1);
	register mpw* temp = (mpw*) malloc((8*size+2) * sizeof(mpw));

	if (temp)
	{
		/* generate p */
		mpprndsafe_w(&dp->p, rgc, bits, mpptrials(bits), temp);

		/* set q */
		mpcopy(size, temp, dp->p.modl);
		mpdivtwo(size, temp);
		mpbset(&dp->q, size, temp);

		/* set r = 2 */
		mpnsetw(&dp->r, 2);

		/* clear n */
		mpbzero(&dp->n);

		dldp_pgoqGenerator_w(dp, rgc, temp);

		free(temp);

		return 0;
	}
	return -1;
}

int dldp_pgoqGenerator_w(dldp_p* dp, randomGeneratorContext* rgc, mpw* wksp)
{
	/*
	 * Randomly determine a generator over the subgroup with order q
	 */

	register size_t size = dp->p.size;

	mpnfree(&dp->g);
	mpnsize(&dp->g, size);

	while (1)
	{
		/* get a random value h (stored into g) */
		mpbrnd_w(&dp->p, rgc, dp->g.data, wksp);

		/* first compute h^r mod p (stored in g) */
		mpbpowmod_w(&dp->p, size, dp->g.data, dp->r.size, dp->r.data, dp->g.data, wksp);

		if (mpisone(size, dp->g.data))
			continue;

		return 0;
	}
	return -1;
}

int dldp_pgoqGenerator(dldp_p* dp, randomGeneratorContext* rgc)
{
	register size_t size = dp->p.size;
	register mpw* temp = (mpw*) malloc((4*size+2)*sizeof(mpw));

	if (temp)
	{
		dldp_pgoqGenerator_w(dp, rgc, temp);

		free(temp);

		return 0;
	}
	return -1;
}

int dldp_pgoqValidate(const dldp_p* dp, randomGeneratorContext* rgc, int cofactor)
{
	register int rc = dldp_pValidate(dp, rgc);

	if (rc <= 0)
		return rc;

	/* check that g^q mod p = 1 */

	/* if r != 0, then check that qr+1 = p */

	/* if cofactor, then check that q does not divide (r) */

	return 1;
}

int dldp_pgonMake(dldp_p* dp, randomGeneratorContext* rgc, size_t pbits, size_t qbits)
{
	/*
	 * Generate parameters with a prime p such that p = qr+1, with q prime, and r = 2s, with s prime
	 */

	register size_t psize = MP_BITS_TO_WORDS(pbits + MP_WBITS - 1);
	register mpw* temp = (mpw*) malloc((8*psize+2) * sizeof(mpw));

	if (temp)
	{
		/* generate q */
		mpprnd_w(&dp->q, rgc, qbits, mpptrials(qbits), (const mpnumber*) 0, temp);

		/* generate p with the appropriate congruences */
		mpprndconone_w(&dp->p, rgc, pbits, mpptrials(pbits), &dp->q, (const mpnumber*) 0, &dp->r, 2, temp);

		/* set n */
		mpbsubone(&dp->p, temp);
		mpbset(&dp->n, psize, temp);

		dldp_pgonGenerator_w(dp, rgc, temp);

		free(temp);

		return 0;
	}
	return -1;
}

int dldp_pgonMakeSafe(dldp_p* dp, randomGeneratorContext* rgc, size_t pbits)
{
	/*
	 * Generate parameters with a safe prime; i.e. p = 2q+1, where q is prime
	 */

	register size_t psize = MP_BITS_TO_WORDS(pbits + MP_WBITS - 1);
	register mpw* temp = (mpw*) malloc((8*psize+2) * sizeof(mpw));

	if (temp)
	{
		/* generate safe p */
		mpprndsafe_w(&dp->p, rgc, pbits, mpptrials(pbits), temp);

		/* set n */
		mpbsubone(&dp->p, temp);
		mpbset(&dp->n, psize, temp);

		/* set q */
		mpcopy(psize, temp, dp->p.modl);
		mpdivtwo(psize, temp);
		mpbset(&dp->q, psize, temp);

		/* set r = 2 */
		mpnsetw(&dp->r, 2);

		dldp_pgonGenerator_w(dp, rgc, temp);

		free(temp);

		return 0;
	}
	return -1;
}

int dldp_pgonGenerator_w(dldp_p* dp, randomGeneratorContext* rgc, mpw* wksp)
{
	register size_t size = dp->p.size;

	mpnfree(&dp->g);
	mpnsize(&dp->g, size);

	while (1)
	{
		mpbrnd_w(&dp->p, rgc, dp->g.data, wksp);

		if (mpistwo(dp->r.size, dp->r.data))
		{
			/*
			 * A little math here: the only element in the group which has order 2 is (p-1);
			 * the two group elements raised to power two which result in 1 (mod p) are thus (p-1) and 1
			 *
			 * mpbrnd_w doesn't return 1 or (p-1), so the test where g^2 mod p = 1 can be safely skipped
			 */

			/* check g^q mod p*/
			mpbpowmod_w(&dp->p, size, dp->g.data, dp->q.size, dp->q.modl, wksp, wksp+size);
			if (mpisone(size, wksp))
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
			mpsetx(size, wksp, dp->r.size, dp->r.data);
			mpdivtwo(size, wksp);

			/* compute t = g^s mod p */
			mpbpowmod_w(&dp->p, size, dp->g.data, size, wksp, wksp+size, wksp+2*size);
			/* compute t^2 mod p = g^2s mod p = g^r mod p*/
			mpbsqrmod_w(&dp->p, size, wksp+size, wksp+size, wksp+2*size);
			if (mpisone(size, wksp+size))
				continue;

			/* compute t^q mod p = g^qs mod p */
			mpbpowmod_w(&dp->p, size, wksp, dp->q.size, dp->q.modl, wksp+size, wksp+2*size);
			if (mpisone(size, wksp+size))
				continue;

			/* compute g^2q mod p */
			mpbpowmod_w(&dp->p, size, dp->g.data, dp->q.size, dp->q.modl, wksp, wksp+size);
			mpbsqrmod_w(&dp->p, size, wksp, wksp+size, wksp+2*size);
			if (mpisone(size, wksp+size))
				continue;
		}

		return 0;
	}

	return -1;
}

int dldp_pgonGenerator(dldp_p* dp, randomGeneratorContext* rgc)
{
	register size_t psize = dp->p.size;
	register mpw* temp = (mpw*) malloc((8*psize+2) * sizeof(mpw));

	if (temp)
	{
		dldp_pgonGenerator_w(dp, rgc, temp);

		free(temp);

		return 0;
	}
	return -1;
}

int dldp_pgonValidate(const dldp_p* dp, randomGeneratorContext* rgc)
{
	return dldp_pValidate((const dldp_p*) dp, rgc);
}

/*!\}
 */
