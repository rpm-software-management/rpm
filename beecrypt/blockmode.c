/*
 * Copyright (c) 2000, 2002 Virtual Unlimited B.V.
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

/*!\file blockmode.c
 * \brief Blockcipher operation modes.
 * \todo Additional modes, such as CFB and OFB.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup BC_m
 */

#include "system.h"
#include "blockmode.h"
#include "mp.h"
#include "debug.h"

/*!\addtogroup BC_m
 * \{
 */

int blockEncryptECB(const blockCipher* bc, blockCipherParam* bp, uint32_t* dst, const uint32_t* src, int nblocks)
{
	register const int blockwords = bc->blocksize >> 2;

	while (nblocks > 0)
	{
/*@-noeffectuncon@*/
		(void) bc->encrypt(bp, dst, src);
/*@=noeffectuncon@*/

		dst += blockwords;
		src += blockwords;

		nblocks--;
	}
	return 0;
}

int blockDecryptECB(const blockCipher* bc, blockCipherParam* bp, uint32_t* dst, const uint32_t* src, int nblocks)
{
	register const int blockwords = bc->blocksize >> 2;

	while (nblocks > 0)
	{
/*@-noeffectuncon@*/
		(void) bc->decrypt(bp, dst, src);
/*@=noeffectuncon@*/

		dst += blockwords;
		src += blockwords;

		nblocks--;
	}
	return 0;
}

int blockEncryptCBC(const blockCipher* bc, blockCipherParam* bp, uint32_t* dst, const uint32_t* src, int nblocks)
{
	register int blockwords = bc->blocksize >> 2;
	register uint32_t* fdback = bc->getfb(bp);

	if (nblocks > 0)
	{
		register int i;

		for (i = 0; i < blockwords; i++)
			dst[i] = src[i] ^ fdback[i];

/*@-noeffectuncon@*/
		(void) bc->encrypt(bp, dst, dst);
/*@=noeffectuncon@*/

		dst += blockwords;
		src += blockwords;

		nblocks--;

/*@-usedef@*/ /* LCL: dst is initialized. */
		while (nblocks > 0)
		{
			for (i = 0; i < blockwords; i++)
				dst[i] = src[i] ^ dst[i-blockwords];

/*@-noeffectuncon@*/
			(void) bc->encrypt(bp, dst, dst);
/*@=noeffectuncon@*/

			dst += blockwords;
			src += blockwords;

			nblocks--;
		}

		for (i = 0; i < blockwords; i++)
			fdback[i] = dst[i-blockwords];
/*@=usedef@*/
	}
	return 0;
}

int blockDecryptCBC(const blockCipher* bc, blockCipherParam* bp, uint32_t* dst, const uint32_t* src, int nblocks)
{
	/* assumes that every blockcipher's blocksize is a multiple of 32 bits */
	register int blockwords = bc->blocksize >> 2;
	register uint32_t* fdback = bc->getfb(bp);
	register uint32_t* buf = (uint32_t*) malloc(blockwords * sizeof(*buf));

	if (buf)
	{
		while (nblocks > 0)
		{
			register uint32_t tmp;
			register int i;

/*@-noeffectuncon@*/
			(void) bc->decrypt(bp, buf, src);
/*@=noeffectuncon@*/

			for (i = 0; i < blockwords; i++)
			{
				tmp = src[i];
/*@-usedef@*/ /* LCL: buf is initialized. */
				dst[i] = buf[i] ^ fdback[i];
/*@-usedef@*/
				fdback[i] = tmp;
			}

			dst += blockwords;
			src += blockwords;

			nblocks--;
		}
		free(buf);
		return 0;
	}
	return -1;
}

#ifdef	DYING
int blockEncrypt(const blockCipher* bc, blockCipherParam* bp, cipherMode mode, int blocks, uint32_t* dst, const uint32_t* src)
{
	if (bc->mode)
	{
		register const blockMode* bm = bc->mode+mode;

		if (bm)
		{
			register const blockModeEncrypt be = bm->encrypt;

			if (be)
				return be(bp, blocks, dst, src);
		}
	}

	return -1;
}

int blockDecrypt(const blockCipher* bc, blockCipherParam* bp, cipherMode mode, int blocks, uint32_t* dst, const uint32_t* src)
{
	if (bc->mode)
	{
		register const blockMode* bm = bc->mode+mode;

		if (bm)
		{
			register const blockModeEncrypt bd = bm->decrypt;

			if (bd)
				return bd(bp, blocks, dst, src);
		}
	}

	return -1;
}
#endif

/*!\}
 */
