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

/*!\file testmpinv.c
 * \brief Unit test program for the multi-precision modular inverse.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup UNIT_m
 */

#include <stdio.h>

#include "beecrypt.h"
#include "mpnumber.h"

struct vector
{
	const char* m;
	const char* k;
	const char* inv_k;
};

#define NVECTORS	5

struct vector table[NVECTORS] = {
	{	"c773218c737ec8ee993b4f2ded30f48edace915f",
		"358dad571462710f50e254cf1a376b2bdeaadfbf",
		"0d5167298202e49b4116ac104fc3f415ae52f917" },
	{	"fe95df16069b516859ba036ef0e563a7b6a86409",
		"eedd5539e982b570a5f8efc73f243a04f312920d",
		"f64a00a9ce43f4128e5eee1991b2e08c6008ba4e" },
	{	"fe95df16069b516859ba036ef0e563a7b6a86409",
		"d75f6d17eb243613eacc0dcbb41db4e5a3364b07",
		"e90aa0a992ebd4c9176f0e20a885101218111a73" },
	{	"fe95df16069b516859ba036ef0e563a7b6a86409",
		"759ea04b65f66184af22fcabfe99a1cda3a79236",
		"2c701a52078afe539a281cba7f35df34a7a125a4" },
	{	"80277b4855a39cb9a98b2107cc1efb29f1832f727df05931cdd4a64cd78363134bf2abe78723784d2013a26875afe13f04526399c6b0cee659abb60dc8263400",
		"10001",
		"6e5f92b24defc7ffafa20024b30ccbcce810d0408f6efda3035f6e8b27e224e66db6e78f54b89bd7f11477fff7bc2f071335d24a92f19c8090226f7d97303001" }

};

int main()
{
	int i, failures = 0;

	mpnumber m;
	mpnumber k;
	mpnumber inv_k;
	mpnumber inv;

	mpnzero(&m);
	mpnzero(&k);
	mpnzero(&inv_k);
	mpnzero(&inv);

	for (i = 0; i < NVECTORS; i++)
	{
		mpnsethex(&m, table[i].m);
		mpnsethex(&k, table[i].k);
		mpnsethex(&inv_k, table[i].inv_k);

		if (mpninv(&inv, &k, &m))
		{
			if (mpnex(inv.size, inv.data, inv_k.size, inv_k.data))
			{
				printf("mpninv return unexpected result\n");
				failures++;
			}
		}
		else
		{
			printf("mpninv failed\n");
			failures++;
		}
	}

	mpnfree(&m);
	mpnfree(&k);
	mpnfree(&inv_k);
	mpnfree(&inv);

	return failures;
}
