/*
 * Copyright (c) 2002, 2003 Bob Deblier
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

/*!\file testdsa.c
 * \brief Unit test program for the DSA algorithm.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup UNIT_m
 */

#include <stdio.h>

#include "beecrypt.h"
#include "dlkp.h"
#include "dsa.h"

static const char* dsa_p = "8df2a494492276aa3d25759bb06869cbeac0d83afb8d0cf7cbb8324f0d7882e5d0762fc5b7210eafc2e9adac32ab7aac49693dfbf83724c2ec0736ee31c80291";
static const char* dsa_q = "c773218c737ec8ee993b4f2ded30f48edace915f";
static const char* dsa_g = "626d027839ea0a13413163a55b4cb500299d5522956cefcb3bff10f399ce2c2e71cb9de5fa24babf58e5b79521925c9cc42e9f6f464b088cc572af53e6d78802";
static const char* dsa_x = "2070b3223dba372fde1c0ffc7b2e3b498b260614";
static const char* dsa_y = "19131871d75b1612a819f29d78d1b0d7346f7aa77bb62a859bfd6c5675da9d212d3a36ef1672ef660b8c7c255cc0ec74858fba33f44c06699630a76b030ee333";

static const char* dsa_k = "358dad571462710f50e254cf1a376b2bdeaadfbf";

static const char* dsa_hm = "a9993e364706816aba3e25717850c26c9cd0d89d";

static const char* expect_r = "8bac1ab66410435cb7181f95b16ab97c92b341c0";
static const char* expect_s = "41e2345f1f56df2458f426d155b4ba2db6dcd8c8";

/* we need to fake a random generator to pass k into the signing algorithm */

int fake_setup(randomGeneratorParam* p)
{
	return 0;
}

int fake_seed(randomGeneratorParam* p, const byte* data, size_t size)
{
	return 0;
}

int fake_next(randomGeneratorParam* p, byte* data, size_t size)
{
	mpnumber tmp;

	mpnzero(&tmp);
	mpnsethex(&tmp, dsa_k);

	memcpy(data, tmp.data, size);

	mpnfree(&tmp);

	return 0;
}

int fake_cleanup(randomGeneratorParam* p)
{
	return 0;
}

const randomGenerator fakeprng = { "fake", 4, fake_setup, fake_seed, fake_next, fake_cleanup };

int main()
{
	int failures = 0;

	dlkp_p keypair;
	mpnumber hm, r, s, e_r, e_s;
	randomGeneratorContext rngc;

	dlkp_pInit(&keypair);

	mpbsethex(&keypair.param.p, dsa_p);                 
	mpbsethex(&keypair.param.q, dsa_q);
	mpnsethex(&keypair.param.g, dsa_g);
	mpnsethex(&keypair.y, dsa_y);
	mpnsethex(&keypair.x, dsa_x);

	mpnzero(&e_r);
	mpnzero(&e_s);
	mpnsethex(&e_r, expect_r);
	mpnsethex(&e_s, expect_s);

	mpnzero(&hm);
	mpnsethex(&hm, dsa_hm);

	/* first test, from NIST FIPS 186-1 */
	mpnzero(&r);
	mpnzero(&s);

	if (randomGeneratorContextInit(&rngc, &fakeprng))
		return -1;

	if (dsasign(&keypair.param.p, &keypair.param.q, &keypair.param.g, &rngc, &hm, &keypair.x, &r, &s))
		return -1;

	if (mpnex(e_r.size, e_r.data, r.size, r.data) || mpnex(e_s.size, e_s.data, s.size, s.data))
	{
		printf("failed test vector 1\n");
		failures++;
	}
	else
		printf("ok\n");

	if (randomGeneratorContextFree(&rngc))
		return -1;

	mpnfree(&s);
	mpnfree(&r);

	/* second test, sign a hash and verify the signature */
	mpnzero(&s);
	mpnzero(&r);

	if (randomGeneratorContextInit(&rngc, randomGeneratorDefault()))
		return -1;

	if (dsasign(&keypair.param.p, &keypair.param.q, &keypair.param.g, &rngc, &hm, &keypair.x, &r, &s))
		return -1;

	if (!dsavrfy(&keypair.param.p, &keypair.param.q, &keypair.param.g, &hm, &keypair.y, &r, &s))
	{
		printf("failed test vector 2\n");
		failures++;
	}
	else
		printf("ok\n");

	if (randomGeneratorContextFree(&rngc))
		return -1;
		
	mpnfree(&s);
	mpnfree(&r);

	mpnfree(&hm);

	mpnfree(&e_s);
	mpnfree(&e_r);

	dlkp_pFree(&keypair);

	return failures;
}
