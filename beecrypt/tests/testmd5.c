/*
 * testmd5.c
 *
 * Unit test program for MD5; it tests all vectors specified by RFC 1321.
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

#include "md5.h"

struct input_expect
{
	unsigned char* input;
	uint32 expect[4];
};


struct input_expect table[7] = {
	{ "",
		{ 0xd41d8cd9, 0x8f00b204, 0xe9800998, 0xecf8427e } },
	{ "a",
		{ 0x0cc175b9, 0xc0f1b6a8, 0x31c399e2, 0x69772661 } },
	{ "abc",
		{ 0x90015098, 0x3cd24fb0, 0xd6963f7d, 0x28e17f72 } },
	{ "message digest",
		{ 0xf96b697d, 0x7cb7938d, 0x525a2f31, 0xaaf161d0 } },
	{ "abcdefghijklmnopqrstuvwxyz",
		{ 0xc3fcd3d7, 0x6192e400, 0x7dfb496c, 0xca67e13b } },
	{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
		{ 0xd174ab98, 0xd277d9f5, 0xa5611c2c, 0x9f419d9f } },
	{ "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
		{ 0x57edf4a2, 0x2be3c955, 0xac49da2e, 0x2107b67a } }
};

int main()
{
	int i, failures = 0;
        md5Param param;
	uint32 digest[4];

	for (i = 0; i < 7; i++)
	{
		if (md5Reset(&param))
			return -1;
		if (md5Update(&param, table[i].input, strlen(table[i].input)))
			return -1;
		if (md5Digest(&param, digest))
			return -1;

		if (mp32ne(4, digest, table[i].expect))
		{
			printf("failed\n");
			failures++;
		}
		else
			printf("ok\n");
	}
	return failures;
}
