/*
 * testsha1.c
 *
 * Unit test program for SHA-1; it tests all but one of vectors specified by FIPS PUB 180-1.
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

#include "sha1.h"

struct input_expect
{
	unsigned char* input;
	uint32 expect[5];
};


struct input_expect table[2] = {
	{ "abc",
		{ 0xA9993E36, 0x4706816A, 0xBA3E2571, 0x7850C26C, 0x9CD0D89D } },
	{ "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
		{ 0x84983E44, 0x1C3BD26E, 0xBAAE4AA1, 0xF95129E5, 0xE54670F1 } }
};

int main()
{
	int i, failures = 0;
        sha1Param param;
	uint32 digest[5];

	for (i = 0; i < 2; i++)
	{
		if (sha1Reset(&param))
			return -1;
		if (sha1Update(&param, table[i].input, strlen(table[i].input)))
			return -1;
		if (sha1Digest(&param, digest))
			return -1;

		if (mp32ne(5, digest, table[i].expect))
		{
			printf("failed\n");
			failures++;
		}
		else
			printf("ok\n");
	}
	return failures;
}
