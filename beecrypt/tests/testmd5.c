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

/*!\file testmd5.c
 * \brief Unit test program for the MD5 algorithm; it tests all vectors
 *        specified by RFC 1321.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup UNIT_m
 */

#include <stdio.h>

#include "md5.h"

struct vector
{
	int		input_size;
	byte*	input;
	byte*	expect;
};

struct vector table[7] = {
	{  0, (byte*) "",
	      (byte*) "\xd4\x1d\x8c\xd9\x8f\x00\xb2\x04\xe9\x80\x09\x98\xec\xf8\x42\x7e" },
	{  1, (byte*) "a",
	      (byte*) "\x0c\xc1\x75\xb9\xc0\xf1\xb6\xa8\x31\xc3\x99\xe2\x69\x77\x26\x61" },
	{  3, (byte*) "abc",
	      (byte*) "\x90\x01\x50\x98\x3c\xd2\x4f\xb0\xd6\x96\x3f\x7d\x28\xe1\x7f\x72" },
	{ 14, (byte*) "message digest",
	      (byte*) "\xf9\x6b\x69\x7d\x7c\xb7\x93\x8d\x52\x5a\x2f\x31\xaa\xf1\x61\xd0" },
	{ 26, (byte*) "abcdefghijklmnopqrstuvwxyz",
	      (byte*) "\xc3\xfc\xd3\xd7\x61\x92\xe4\x00\x7d\xfb\x49\x6c\xca\x67\xe1\x3b" },
	{ 62, (byte*) "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
	      (byte*) "\xd1\x74\xab\x98\xd2\x77\xd9\xf5\xa5\x61\x1c\x2c\x9f\x41\x9d\x9f" },
	{ 80, (byte*) "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
	      (byte*) "\x57\xed\xf4\xa2\x2b\xe3\xc9\x55\xac\x49\xda\x2e\x21\x07\xb6\x7a" }
};

int main()
{
	int i, failures = 0;
	byte digest[16];
	md5Param param;

	for (i = 0; i < 7; i++)
	{
		if (md5Reset(&param))
			return -1;
		if (md5Update(&param, table[i].input, table[i].input_size))
			return -1;
		if (md5Digest(&param, digest))
			return -1;

		if (memcmp(digest, table[i].expect, 16))
		{
			printf("failed test vector %d\n", i+1);
			failures++;
		}
	}
	return failures;
}
