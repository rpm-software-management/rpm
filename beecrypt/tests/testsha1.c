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

/*!\file testsha1.c
 * \brief Unit test program for the SHA-1 algorithm ; it tests all but one of
 *        the vectors specified by FIPS PUB 180-1.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup UNIT_m
 */

#include <stdio.h>

#include "sha1.h"
#include "memchunk.h"

struct vector
{
	int input_size;
	byte* input;
	byte* expect;
};

struct vector table[2] = {
	{  3, (byte*) "abc",
	      (byte*) "\xA9\x99\x3E\x36\x47\x06\x81\x6A\xBA\x3E\x25\x71\x78\x50\xC2\x6C\x9C\xD0\xD8\x9D" },
	{ 56, (byte*) "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
		  (byte*) "\x84\x98\x3E\x44\x1C\x3B\xD2\x6E\xBA\xAE\x4A\xA1\xF9\x51\x29\xE5\xE5\x46\x70\xF1" }
};

int main()
{
	int i, failures = 0;
	byte digest[20];
	sha1Param param;

	for (i = 0; i < 2; i++)
	{
		if (sha1Reset(&param))
			return -1;
		if (sha1Update(&param, table[i].input, table[i].input_size))
			return -1;
		if (sha1Digest(&param, digest))
			return -1;

		if (memcmp(digest, table[i].expect, 20))
		{
			printf("failed test vector %d\n", i+1);
			failures++;
		}
	}
	return failures;
}
