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

/*!\file benchme.c
 * \brief Benchmark program for Modular Exponentiation.
 * \author Bob Deblier <bob.deblier@pandora.be>
 */

#include <stdio.h>

#include "beecrypt.h"
#include "dldp.h"
#include "timestamp.h"

#define SECONDS	10

static const char* hp = "d860d6c36ce3bd73c493e15113abd7ba6cc311d1365ddce4c194b65a96ac47ceab6a9ca7af9bfc871d341bf129e674b903533c7db1f8fad957d679ee14a3cbc5a73bf7f8173f33fb7b6a8a11c24652c2573276b214db3898f51cec3a6ff4263d0c5616502e91055bff6b9717d801c41f4b69eaed911fced89b601edfe73b1103";
static const char* hq = "ea6f6724b9b9152766d01adfee421f48012e4a35";
static const char* hg = "9b01e78dcaca5ae69656bb01c9a1f3b159f7cf8f77781146f916836dbca3a2ebc31cd73fbf7ea864ae5e8d0f24ead4332ce0f039ff5648a5f3514d84dd9632598def5b2da266f15391c031758855329ab15f87d3e612bee4f15ab3fd938a1da37992ea64ea90ceeeae654d9f3844e245951fd56a3c7919b3768c5719b43f3d3f";

int main()
{
	dldp_p params;
	mpnumber gq;
	javalong start, now;
	int iterations = 0;

	dldp_pInit(&params);

	mpbsethex(&params.p, hp);
	mpbsethex(&params.q, hq);
	mpnsethex(&params.g, hg);
	mpnzero(&gq);

	/* get starting time */
	start = timestamp();
	do
	{
		mpbnpowmod(&params.p, &params.g, (mpnumber*) &params.q, &gq);
		now = timestamp();
		iterations++;
	} while (now < (start + (SECONDS * ONE_SECOND)));

	mpnfree(&gq);

	printf("(%d bits ^ %d bits) mod (%d bits): %d times in %d seconds\n",
		(int) mpbits(params.g.size, params.g.data),
		(int) mpbits(params.q.size, params.q.modl),
		(int) mpbits(params.p.size, params.p.modl),
		iterations,
		SECONDS);

	dldp_pFree(&params);

	return 0;
}
