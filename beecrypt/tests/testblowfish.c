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

extern int fromhex(byte*, const char*);

struct vector
{
	char*			key;
	char*			input;
	char*			expect;
	cipherOperation	op;
};

#define NVECTORS 4

struct vector table[NVECTORS] = {
	{ "0000000000000000",
	  "0000000000000000",
	  "4ef997456198dd78",
	  ENCRYPT },
	{ "ffffffffffffffff",
	  "ffffffffffffffff",
	  "51866fd5B85ecb8a",
	  ENCRYPT },
	{ "3000000000000000",
	  "1000000000000001",
	  "7d856f9a613063f2",
	  ENCRYPT },
	{ "1111111111111111",
	  "1111111111111111",
	  "2466dd878b963c9d",
	  ENCRYPT }
};

int main()
{
	int i, failures = 0;
	blowfishParam param;
    byte key[56];
    byte src[8];
    byte dst[8];
    byte chk[8];
    size_t keybits;

    for (i = 0; i < NVECTORS; i++)
    { 
        keybits = fromhex(key, table[i].key) << 3;
      
        if (blowfishSetup(&param, key, keybits, table[i].op))
            return -1;
      
        fromhex(src, table[i].input);
        fromhex(chk, table[i].expect);
    
        switch (table[i].op)
        {
        case ENCRYPT:
            if (blowfishEncrypt(&param, (uint32_t*) dst, (const uint32_t*) src))
                return -1;
            break;
        case DECRYPT:
            if (blowfishDecrypt(&param, (uint32_t*) dst, (const uint32_t*) src))
                return -1;
            break;
        }
    
        if (memcmp(dst, chk, 8))
        {
            printf("failed vector %d\n", i+1);
            failures++;
        }

    }

	return failures;
}
