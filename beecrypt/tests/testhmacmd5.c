/*
 * testhmacmd5.c
 *
 * Unit test program for HMAC-MD5; it tests all vectors specified by RFC 2202.
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

#include "hmacmd5.h"

struct key_input_expect
{
	unsigned char* key;
	unsigned char* input;
	uint32 expect[4];
};

struct key_input_expect table[7] = 
{
	{ "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b", "Hi There",
		{ 0x9294727a, 0x3638bb1c, 0x13f48ef8, 0x158bfc9d } },
	{ "Jefe", "what do ya want for nothing?",
		{ 0x750c783e, 0x6ab0b503, 0xeaa86e31, 0x0a5db738 } },
	{ "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa", "\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd",
		{ 0x56be3452, 0x1d144c88, 0xdbb8c733, 0xf0e8b3f6 } },
	{ "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19",  "\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd",
		{ 0x697eaf0a, 0xca3a3aea, 0x3a751647, 0x46ffaa79 } },
	{ "\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c", "Test With Truncation",
		{ 0x56461ef2, 0x342edc00, 0xf9bab995, 0x690efd4c } },
	{ "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa", "Test Using Larger Than Block-Size Key - Hash Key First",
		{ 0x6b1ab7fe, 0x4bd7bf8f, 0x0b62e6ce, 0x61b9d0cd } },
	{ "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa", "Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data",
		{ 0x6f630fad, 0x67cda0ee, 0x1fb1f562, 0xdb3aa53e} }
};

int main()
{
	int i, failures = 0;
        hmacmd5Param param;
	uint32 digest[4];
	uint32 key[64];

	for (i = 0; i < 7; i++)
	{
		/* set the key up properly, removing endian-ness */
		decodeIntsPartial(key, table[i].key, strlen(table[i].key));

		if (hmacmd5Setup(&param, key, strlen(table[i].key) << 3))
			return -1;

		if (hmacmd5Update(&param, table[i].input, strlen(table[i].input)))
			return -1;
		if (hmacmd5Digest(&param, digest))
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
