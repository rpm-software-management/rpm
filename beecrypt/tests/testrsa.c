/*
 * Copyright (c) 2003 Bob Deblier
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

/*!\file testrsa.c
 * \brief Unit test program for the RSA algorithm.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup UNIT_m
 */

#include <stdio.h>

#include "beecrypt.h"
#include "rsa.h"

static const char* rsa_n  = "bbf82f090682ce9c2338ac2b9da871f7368d07eed41043a440d6b6f07454f51fb8dfbaaf035c02ab61ea48ceeb6fcd4876ed520d60e1ec4619719d8a5b8b807fafb8e0a3dfc737723ee6b4b7d93a2584ee6a649d060953748834b2454598394ee0aab12d7b61a51f527a9a41f6c1687fe2537298ca2a8f5946f8e5fd091dbdcb";
static const char* rsa_e  = "11";
static const char* rsa_p  = "eecfae81b1b9b3c908810b10a1b5600199eb9f44aef4fda493b81a9e3d84f632124ef0236e5d1e3b7e28fae7aa040a2d5b252176459d1f397541ba2a58fb6599";
static const char* rsa_q  = "c97fb1f027f453f6341233eaaad1d9353f6c42d08866b1d05a0f2035028b9d869840b41666b42e92ea0da3b43204b5cfce3352524d0416a5a441e700af461503";
static const char* rsa_d1 = "54494ca63eba0337e4e24023fcd69a5aeb07dddc0183a4d0ac9b54b051f2b13ed9490975eab77414ff59c1f7692e9a2e202b38fc910a474174adc93c1f67c981";
static const char* rsa_d2 = "471e0290ff0af0750351b7f878864ca961adbd3a8a7e991c5c0556a94c3146a7f9803f8f6f8ae342e931fd8ae47a220d1b99a495849807fe39f9245a9836da3d";
static const char* rsa_c  = "b06c4fdabb6301198d265bdbae9423b380f271f73453885093077fcd39e2119fc98632154f5883b167a967bf402b4e9e2e0f9656e698ea3666edfb25798039f7";

static const char* rsa_m  = "d436e99569fd32a7c8a05bbc90d32c49";

int main()
{
	int failures = 0;

	rsakp keypair;
	mpnumber m, cipher, decipher;
	randomGeneratorContext rngc;

	if (randomGeneratorContextInit(&rngc, randomGeneratorDefault()) == 0)
	{
		/* First we do the fixed value verification */
		rsakpInit(&keypair);

		mpbsethex(&keypair.n, rsa_n);
		mpnsethex(&keypair.e, rsa_e);
		mpbsethex(&keypair.p, rsa_p);
		mpbsethex(&keypair.q, rsa_q);
		mpnsethex(&keypair.dp, rsa_d1);
		mpnsethex(&keypair.dq, rsa_d2);
		mpnsethex(&keypair.qi, rsa_c);

		mpnzero(&m);
		mpnzero(&cipher);
		mpnzero(&decipher);

		mpnsethex(&m, rsa_m);

		/* it's safe to cast the keypair to a public key */
		if (rsapub(&keypair.n, &keypair.e, &m, &cipher))
			failures++;

		if (rsapricrt(&keypair.n, &keypair.p, &keypair.q, &keypair.dp, &keypair.dq, &keypair.qi, &cipher, &decipher))
			failures++;

		if (mpnex(m.size, m.data, decipher.size, decipher.data))
			failures++;

		mpnfree(&decipher);
		mpnfree(&cipher);
		mpnfree(&m);

		rsakpFree(&keypair);

		mpnzero(&m);
		mpnzero(&cipher);
		mpnzero(&decipher);

		/* Now we generate a keypair and do some tests on it */
		rsakpMake(&keypair, &rngc, 512);

		/* generate a random m in the range 0 < m < n */
		mpbnrnd(&keypair.n, &rngc, &m);

		/* it's safe to cast the keypair to a public key */
		if (rsapub(&keypair.n, &keypair.e, &m, &cipher))
			failures++;

		if (rsapricrt(&keypair.n, &keypair.p, &keypair.q, &keypair.dp, &keypair.dq, &keypair.qi, &cipher, &decipher))
			failures++;

		if (mpnex(m.size, m.data, decipher.size, decipher.data))
			failures++;

		rsakpFree(&keypair);
	}
	return failures;
}
