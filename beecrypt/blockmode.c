/*
 * blockmode.c
 *
 * Block cipher operation modes, code
 *
 * Copyright (c) 2000, Virtual Unlimited B.V.
 *
 * Author: Bob Deblier <bob@virtualunlimited.com>
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

#define BEECRYPT_DLL_EXPORT

#include "blockmode.h"
#include "mp32.h"

/* generic functions for those blockCiphers that don't implement an optimized version */

static int ecbencrypt(const blockCipher* bc, blockCipherParam* bp, int blocks, uint32* dst, const uint32* src)
{
	register int i;
	register uint32 blockwords = (bc->blockbits >> 5);

	mp32copy(blocks * blockwords, dst, src);
	for (i = 0; i < blocks; i++)
	{
		bc->encrypt(bp, dst);
		dst += blockwords;
	}
	return 0;
}

static int ecbdecrypt(const blockCipher* bc, blockCipherParam* bp, int blocks, uint32* dst, const uint32* src)
{
	register int i;
	register uint32 blockwords = (bc->blockbits >> 5);

	mp32copy(blocks * blockwords, dst, src);
	for (i = 0; i < blocks; i++)
	{
		bc->decrypt(bp, dst);
		dst += blockwords;
	}
	return 0;
}


static int cbcencrypt(const blockCipher* bc, blockCipherParam* bp, int blocks, uint32* dst, const uint32* src, const uint32* iv)
{
	register int i;
	register uint32 blockwords = (bc->blockbits >> 5);

	mp32copy(blockwords, dst, src);

	if (iv)
		mp32xor(blockwords, dst, iv);

	bc->encrypt(bp, dst);

	dst += blockwords;
	src += blockwords;

	for (i = 1; i < blocks; i++)
	{
		mp32xor(blockwords, dst, dst - blockwords);

		mp32copy(blockwords, dst, src);
		bc->encrypt(bp, dst);

		dst += blockwords;
		src += blockwords;
	}
	return 0;
}

static int cbcdecrypt(const blockCipher* bc, blockCipherParam* bp, int blocks, uint32* dst, const uint32* src, const uint32* iv)
{
	register int i;
	register uint32 blockwords = (bc->blockbits >> 5);

	mp32copy(blockwords, dst, src);
	bc->decrypt(bp, dst);

	if (iv)
		mp32xor(blockwords, dst, iv);

	dst += blockwords;
	src += blockwords;

	for (i = 1; i < blocks; i++)
	{
		mp32copy(blockwords, dst, src);
		bc->decrypt(bp, dst);

		mp32xor(blockwords, dst, src - blockwords);

		dst += blockwords;
		src += blockwords;
	}
	return 0;
}

int blockEncrypt(const blockCipher* bc, blockCipherParam* bp, cipherMode mode, int blocks, uint32* dst, const uint32* src, const uint32* iv)
{
	if (bc->mode)
	{
		register const blockMode* bm = bc->mode+mode;

		if (bm)
		{
			register const blockModeEncrypt be = bm->encrypt;

			if (be) /* we have an optimized version for this cipher / mode combination */
				return be(bp, blocks, dst, src, iv);
		}
	}

	switch (mode)
	{
	case ECB:
		return ecbencrypt(bc, bp, blocks, dst, src);
	case CBC:
		return cbcencrypt(bc, bp, blocks, dst, src, iv);
	default: /* other block modes aren't implemented yet */
		return -1;
	}
}

int blockDecrypt(const blockCipher* bc, blockCipherParam* bp, cipherMode mode, int blocks, uint32* dst, const uint32* src, const uint32* iv)
{
	if (bc->mode)
	{
		register const blockMode* bm = bc->mode+mode;

		if (bm)
		{
			register const blockModeEncrypt bd = bm->decrypt;

			if (bd) /* we have an optimized version for this cipher / mode combination */
				return bd(bp, blocks, dst, src, iv);
		}
	}

	switch (mode)
	{
	case ECB:
		return ecbdecrypt(bc, bp, blocks, dst, src);
	case CBC:
		return cbcdecrypt(bc, bp, blocks, dst, src, iv);
	default: /* other block modes aren't implemented yet */
		return -1;
	}
}
