/*
 * testsha256.c
 *
 * Unit test program for SHA-256; it implements the test vectors from the draft FIPS document.
 *
 * Copyright (c) 2002 Bob Deblier
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

#include <stdio.h>

#include "sha256.h"

struct input_expect
{
	unsigned char* input;
	uint32 expect[8];
};


struct input_expect table[2] = {
	{ "abc",
		{ 0xba7816bf, 0x8f01cfea, 0x414140de, 0x5dae2223, 0xb00361a3, 0x96177a9c, 0xb410ff61, 0xf20015ad } },
	{ "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
		{ 0x248d6a61, 0xd20638b8, 0xe5c02693, 0x0c3e6039, 0xa33ce459, 0x64ff2167, 0xf6ecedd4, 0x19db06c1} }
};

int main()
{
	int i, failures = 0;
        sha256Param param;
	uint32 digest[8];

	for (i = 0; i < 2; i++)
	{
		if (sha256Reset(&param))
			return -1;
		if (sha256Update(&param, table[i].input, strlen(table[i].input)))
			return -1;
		if (sha256Digest(&param, digest))
			return -1;

		if (mp32ne(8, digest, table[i].expect))
		{
			printf("failed\n");
			failures++;
		}
		else
			printf("ok\n");
	}
	return failures;
}
