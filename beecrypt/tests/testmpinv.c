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

#include "system.h"

#include "beecrypt.h"
#include "mpbarrett.h"

#include "debug.h"

static const char* dsa_q		= "c773218c737ec8ee993b4f2ded30f48edace915f";
static const char* dsa_k		= "358dad571462710f50e254cf1a376b2bdeaadfbf";
static const char* dsa_inv_k	= "0d5167298202e49b4116ac104fc3f415ae52f917";

int main()
{
	int failures = 0;

	mpnumber q;
	mpnumber k;
	mpnumber inv_k;
	mpnumber inv;

	mpnzero(&q);
	mpnzero(&k);
	mpnzero(&inv_k);
	mpnzero(&inv);

	mpnsethex(&q, dsa_q);
	mpnsethex(&k, dsa_k);
	mpnsethex(&inv_k, dsa_inv_k);

	if (mpninv(&inv, &k, &q))
	{
		if (mpnex(inv.size, inv.data, inv_k.size, inv_k.data))
		{
			printf("mpninv return unexpected result\n");
			mpprintln(inv_k.size, inv_k.data);
			mpprintln(inv.size, inv.data);
			failures++;
		}
	}
	else
	{
		printf("mpninv failed\n");
		failures++;
	}

	return failures;
}
