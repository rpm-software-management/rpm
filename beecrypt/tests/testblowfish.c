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

/*!\file testblowfish.c
 * \brief Unit test program for the Blowfish cipher.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup UNIT_m
 */

#include <stdio.h>

#include "blowfish.h"

struct vector
{
	byte	key[8];
	byte	input[8];
	byte	expect[8];
};

#define NVECTORS 4

struct vector table[NVECTORS] = {
	{ "\x00\x00\x00\x00\x00\x00\x00\x00",
	  "\x00\x00\x00\x00\x00\x00\x00\x00",
	  "\x4E\xF9\x97\x45\x61\x98\xDD\x78"
	},
	{ "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF",
	  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF",
	  "\x51\x86\x6F\xD5\xB8\x5E\xCB\x8A"
	},
	{ "\x30\x00\x00\x00\x00\x00\x00\x00",
	  "\x10\x00\x00\x00\x00\x00\x00\x01",
	  "\x7D\x85\x6F\x9A\x61\x30\x63\xF2"
	},
	{ "\x11\x11\x11\x11\x11\x11\x11\x11",
	  "\x11\x11\x11\x11\x11\x11\x11\x11",
	  "\x24\x66\xDD\x87\x8B\x96\x3C\x9D"
	},
};

int main()
{
	int i, failures = 0;
	blowfishParam param;
	byte ciphertext[8];

	for (i = 0; i < NVECTORS; i++)
	{
		if (blowfishSetup(&param, table[i].key, 64, ENCRYPT))
			return -1;
		if (blowfishEncrypt(&param, (uint32_t*) ciphertext, (const uint32_t*) table[i].input))
			return -1;

		if (memcmp(ciphertext, table[i].expect, 8))
		{
			int j;
			printf("failed test vector %d\n", i+1);
			printf("key:\n");
			for (j = 0; j < 8; j++)
				printf("%02x", table[i].key[j]);
			printf("\ncleartext:\n");
			for (j = 0; j < 8; j++)
				printf("%02x", table[i].input[j]);
			printf("\nexpected ciphertext:\n");
			for (j = 0; j < 8; j++)
				printf("%02x", table[i].expect[j]);
			printf("\nciphertext:\n");
			for (j = 0; j < 8; j++)
				printf("%02x", ciphertext[j]);
			printf("\n");
				
			failures++;
		}
		else
			printf("ok\n");
	}
	return failures;
}
