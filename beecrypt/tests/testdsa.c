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

struct vector
{
	const char* p;
	const char* q;
	const char* g;
	const char* y;
	const char* m;
	const char* r;
	const char* s;
};

#define NVECTORS 2

struct vector table[NVECTORS] = {
	{ "8df2a494492276aa3d25759bb06869cbeac0d83afb8d0cf7cbb8324f0d7882e5d0762fc5b7210eafc2e9adac32ab7aac49693dfbf83724c2ec0736ee31c80291",
	  "c773218c737ec8ee993b4f2ded30f48edace915f",
	  "626d027839ea0a13413163a55b4cb500299d5522956cefcb3bff10f399ce2c2e71cb9de5fa24babf58e5b79521925c9cc42e9f6f464b088cc572af53e6d78802",
	  "19131871d75b1612a819f29d78d1b0d7346f7aa77bb62a859bfd6c5675da9d212d3a36ef1672ef660b8c7c255cc0ec74858fba33f44c06699630a76b030ee333",
	  "a9993e364706816aba3e25717850c26c9cd0d89d",
	  "8bac1ab66410435cb7181f95b16ab97c92b341c0",
	  "41e2345f1f56df2458f426d155b4ba2db6dcd8c8" },
	{ "A62927E72F9F12CD31C50E30D0E9B580539C4F7CA2AC3E2EE244C834303B039A1A388FDE4DCD42B5402807047FBEC0DB09ECF897CD2B8546A893499B3A8A409C52476708EAD0124E43F31CA2495A950731D254F56F4F39AC379E0620E15A9CC5A8EA5100CD1137012093E11F73A1E38FAEB95588BB54A48913977D1A1EC6986F",
	  "C4243BE451ECBA6F87F539A7F899D4047208B091",
	  "9C8D21312FD7358D86D82E8F237E99A9DFC375529456420F159361C40A76A891DA8D6CEE8EB1BDEC97CA60CCBE921BED5EB29EC35A2EFCA295311585753EFABBADF599620EA0FB8489FBEE60EDE6D5A99DD3506F37CC21741D306BEE15BBB8EAA1261C2DC18221FB5C6A08602B3E1084029285DF161A2CB6B179830C31C351A3",
	  "7602862B1A4F6F154BE21AFD86CF2AADD6393AE0EBBB5781CB82758C9A360A540BBCC3CBBF014509FD0ED2FC30C6ED0959C43954CF058854B8469DD4247AC463D4C10B6479C8B4FBE56E97067A7FC4E7F1A95507A0B6D70328534C37B590DB8ED12BB460FC3232758F9B64D7BD63BD320FF1FA635A3F77D13D71A8AD4E8B5469",
	  "73F6679451E5F98CA60235E6B4C58FC14043C56D",
	  "22EDDAD362C3209DF597070D144E8FDDB8B65E53",
	  "3AB093E7A7CD30125036B384C6C114317F10E10D" }
};

int main()
{
	int i, failures = 0;

	dlkp_p keypair;
	mpnumber hm, r, s, k, e_r, e_s;

	for (i = 0; i < NVECTORS; i++)
	{
		dlkp_pInit(&keypair);

		mpbsethex(&keypair.param.p, table[i].p);
		mpbsethex(&keypair.param.q, table[i].q);
		mpnsethex(&keypair.param.g, table[i].g);
		mpnsethex(&keypair.y, table[i].y);

		mpnzero(&hm);
		mpnsethex(&hm, table[i].m);

		mpnzero(&e_r);
		mpnzero(&e_s);

		mpnsethex(&e_r, table[i].r);
		mpnsethex(&e_s, table[i].s);

		mpnzero(&r);
		mpnzero(&s);

		/* first test, verify the signature result from NIST FIPS 186-1 */
		if (!dsavrfy(&keypair.param.p, &keypair.param.q, &keypair.param.g, &hm, &keypair.y, &e_r, &e_s))
			failures++;

		mpnfree(&s);
		mpnfree(&r);

		mpnfree(&hm);

		mpnfree(&e_s);
		mpnfree(&e_r);

		dlkp_pFree(&keypair);
	}

	return failures;
}
