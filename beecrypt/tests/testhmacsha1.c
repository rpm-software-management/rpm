/*
 * testhmacsha1.c
 *
 * Unit test program for HMAC-SHA1; it tests all vectors specified by RFC 2202.
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

#include "hmacsha1.h"

struct key_input_expect
{
	unsigned char* key;
	unsigned char* input;
	uint32 expect[5];
};

struct key_input_expect table[7] = 
{
	{ "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b", "Hi There",
		{ 0xb6173186, 0x55057264, 0xe28bc0b6, 0xfb378c8e, 0xf146be00 } },
	{ "Jefe", "what do ya want for nothing?",
		{ 0xeffcdf6a, 0xe5eb2fa2, 0xd27416d5, 0xf184df9c, 0x259a7c79 } },
	{ "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa", "\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd",
		{ 0x125d7342, 0xb9ac11cd, 0x91a39af4, 0x8aa17b4f, 0x63f175d3 } },
	{ "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19",  "\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd",
		{ 0x4c9007f4, 0x026250c6, 0xbc8414f9, 0xbf50c86c, 0x2d7235da } },
	{ "\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c", "Test With Truncation",
		{ 0x4c1a0342, 0x4b55e07f, 0xe7f27be1, 0xd58bb932, 0x4a9a5a04 } },
	{ "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa", "Test Using Larger Than Block-Size Key - Hash Key First",
		{ 0xaa4ae5e1, 0x5272d00e, 0x95705637, 0xce8a3b55, 0xed402112 } },
	{ "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa", "Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data",
		{ 0xe8e99d0f, 0x45237d78, 0x6d6bbaa7, 0x965c7808, 0xbbff1a91 } }
};

int main()
{
	int i, failures = 0;
	uint32 digest[5];
	uint32 key[64];
        hmacsha1Param param;

	for (i = 0; i < 7; i++)
	{
		/* set the key up properly, removing endian-ness */
		decodeIntsPartial(key, table[i].key, strlen(table[i].key));

		if (hmacsha1Setup(&param, key, strlen(table[i].key) << 3))
			return -1;
		if (hmacsha1Update(&param, table[i].input, strlen(table[i].input)))
			return -1;
		if (hmacsha1Digest(&param, digest))
			return -1;

		if (mp32ne(5, digest, table[i].expect))
		{
			printf("failed\n");
			mp32println(5, table[i].expect);
			mp32println(5, digest);
			failures++;
		}
		else
			printf("ok\n");
	}

	return failures;
}
