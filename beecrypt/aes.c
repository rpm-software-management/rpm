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

/*!\file aes.c
 * \brief AES block cipher, as specified by NIST FIPS 197.
 *
 * The table lookup method was inspired by Brian Gladman's AES implementation,
 * which is much more readable than the standard code.
 *
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup BC_aes_m BC_m
 */

#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/aes.h"

#if defined(BYTE_ORDER) && defined(BIG_ENDIAN) && defined(LITTLE_ENDIAN)
# if (BYTE_ORDER != BIG_ENDIAN) && (BYTE_ORDER != LITTLE_ENDIAN)
#  error unsupported endian-ness.
# endif
#endif

#if WORDS_BIGENDIAN
# include "beecrypt/aes_be.h"
#else
#  include "beecrypt/aes_le.h"
#endif

#ifdef ASM_AESENCRYPTECB
extern int aesEncryptECB(aesParam*, uint32_t*, const uint32_t*, unsigned int);
#endif

#ifdef ASM_AESDECRYPTECB
extern int aesDecryptECB(aesParam*, uint32_t*, const uint32_t*, unsigned int);
#endif

const blockCipher aes = {
	"AES",
	sizeof(aesParam),
	16,
	128,
	256,
	64,
	(blockCipherSetup) aesSetup,
	(blockCipherSetIV) aesSetIV,
	/* raw */
	{
		(blockCipherRawcrypt) aesEncrypt,
		(blockCipherRawcrypt) aesDecrypt
	},
	/* ecb */
	{
		#ifdef ASM_AESENCRYPTECB
		(blockCipherModcrypt) aesEncryptECB,
		#else
		(blockCipherModcrypt) 0,
		#endif
		#ifdef ASM_AESDECRYPTECB
		(blockCipherModcrypt) aesDecryptECB,
		#else
		(blockCipherModcrypt) 0,
		#endif
	},
	/* cbc */
	{
		(blockCipherModcrypt) 0,
		(blockCipherModcrypt) 0
	},
	(blockCipherFeedback) aesFeedback
};

int aesSetup(aesParam* ap, const byte* key, size_t keybits, cipherOperation op)
{
	if ((op != ENCRYPT) && (op != DECRYPT))
		return -1;

	if (((keybits & 63) == 0) && (keybits >= 128) && (keybits <= 256))
	{
		register uint32_t* rk, t, i, j;

		/* clear fdback/iv */
		ap->fdback[0] = 0;
		ap->fdback[1] = 0;
		ap->fdback[2] = 0;
		ap->fdback[3] = 0;

		ap->nr = 6 + (keybits >> 5);

		rk = ap->k;

		memcpy(rk, key, keybits >> 3);

		i = 0;

		if (keybits == 128)
		{
			while (1)
			{
				t = rk[3];
				#if WORDS_BIGENDIAN
				t = (_ae4[(t >> 16) & 0xff] & 0xff000000) ^
					(_ae4[(t >>  8) & 0xff] & 0x00ff0000) ^
					(_ae4[(t      ) & 0xff] & 0x0000ff00) ^
					(_ae4[(t >> 24)       ] & 0x000000ff) ^
					 _arc[i];
				#else
				t = (_ae4[(t >>  8) & 0xff] & 0x000000ff) ^
					(_ae4[(t >> 16) & 0xff] & 0x0000ff00) ^
					(_ae4[(t >> 24)       ] & 0x00ff0000) ^
					(_ae4[(t      ) & 0xff] & 0xff000000) ^
					 _arc[i];
				#endif
				rk[4] = (t ^= rk[0]);
				rk[5] = (t ^= rk[1]);
				rk[6] = (t ^= rk[2]);
				rk[7] = (t ^= rk[3]);
				if (++i == 10)
					break;
				rk += 4;
			}
		}
		else if (keybits == 192)
		{
			while (1)
			{
				t = rk[5];
				#if WORDS_BIGENDIAN
				t = (_ae4[(t >> 16) & 0xff] & 0xff000000) ^
					(_ae4[(t >>  8) & 0xff] & 0x00ff0000) ^
					(_ae4[(t      ) & 0xff] & 0x0000ff00) ^
					(_ae4[(t >> 24)       ] & 0x000000ff) ^
					 _arc[i];
				#else
				t = (_ae4[(t >>  8) & 0xff] & 0x000000ff) ^
					(_ae4[(t >> 16) & 0xff] & 0x0000ff00) ^
					(_ae4[(t >> 24)       ] & 0x00ff0000) ^
					(_ae4[(t      ) & 0xff] & 0xff000000) ^
					 _arc[i];
				#endif
				rk[6] = (t ^= rk[0]);
				rk[7] = (t ^= rk[1]);
				rk[8] = (t ^= rk[2]);
				rk[9] = (t ^= rk[3]);
				if (++i == 8)
					break;
				rk[10] = (t ^= rk[4]);
				rk[11] = (t ^= rk[5]);
				rk += 6;
			}
		}
		else if (keybits == 256)
		{
			while (1)
			{
				t = rk[7];
				#if WORDS_BIGENDIAN
				t = (_ae4[(t >> 16) & 0xff] & 0xff000000) ^
					(_ae4[(t >>  8) & 0xff] & 0x00ff0000) ^
					(_ae4[(t      ) & 0xff] & 0x0000ff00) ^
					(_ae4[(t >> 24)       ] & 0x000000ff) ^
					 _arc[i];
				#else
				t = (_ae4[(t >>  8) & 0xff] & 0x000000ff) ^
					(_ae4[(t >> 16) & 0xff] & 0x0000ff00) ^
					(_ae4[(t >> 24)       ] & 0x00ff0000) ^
					(_ae4[(t      ) & 0xff] & 0xff000000) ^
					 _arc[i];
				#endif
				rk[8] = (t ^= rk[0]);
				rk[9] = (t ^= rk[1]);
				rk[10] = (t ^= rk[2]);
				rk[11] = (t ^= rk[3]);
				if (++i == 7)
					break;
				#if WORDS_BIGENDIAN
				t = (_ae4[(t >> 24)       ] & 0xff000000) ^
					(_ae4[(t >> 16) & 0xff] & 0x00ff0000) ^
					(_ae4[(t >>  8) & 0xff] & 0x0000ff00) ^
					(_ae4[(t      ) & 0xff] & 0x000000ff);
				#else
				t = (_ae4[(t      ) & 0xff] & 0x000000ff) ^
					(_ae4[(t >>  8) & 0xff] & 0x0000ff00) ^
					(_ae4[(t >> 16) & 0xff] & 0x00ff0000) ^
					(_ae4[(t >> 24)       ] & 0xff000000);
				#endif
				rk[12] = (t ^= rk[4]);
				rk[13] = (t ^= rk[5]);
				rk[14] = (t ^= rk[6]);
				rk[15] = (t ^= rk[7]);
				rk += 8;
			}
		}

		if (op == DECRYPT)
		{
			rk = ap->k;

			for (i = 0, j = (ap->nr << 2); i < j; i += 4, j -= 4)
			{
				t = rk[i  ]; rk[i  ] = rk[j  ]; rk[j  ] = t;
				t = rk[i+1]; rk[i+1] = rk[j+1]; rk[j+1] = t;
				t = rk[i+2]; rk[i+2] = rk[j+2]; rk[j+2] = t;
				t = rk[i+3]; rk[i+3] = rk[j+3]; rk[j+3] = t;
			}
			for (i = 1; i < ap->nr; i++)
			{
				rk += 4;
				#if WORDS_BIGENDIAN
				rk[0] =
					_ad0[_ae4[(rk[0] >> 24)       ] & 0xff] ^
					_ad1[_ae4[(rk[0] >> 16) & 0xff] & 0xff] ^
					_ad2[_ae4[(rk[0] >>  8) & 0xff] & 0xff] ^
					_ad3[_ae4[(rk[0]      ) & 0xff] & 0xff];
				rk[1] =
					_ad0[_ae4[(rk[1] >> 24)       ] & 0xff] ^
					_ad1[_ae4[(rk[1] >> 16) & 0xff] & 0xff] ^
					_ad2[_ae4[(rk[1] >>  8) & 0xff] & 0xff] ^
					_ad3[_ae4[(rk[1]      ) & 0xff] & 0xff];
				rk[2] =
					_ad0[_ae4[(rk[2] >> 24)       ] & 0xff] ^
					_ad1[_ae4[(rk[2] >> 16) & 0xff] & 0xff] ^
					_ad2[_ae4[(rk[2] >>  8) & 0xff] & 0xff] ^
					_ad3[_ae4[(rk[2]      ) & 0xff] & 0xff];
				rk[3] =
					_ad0[_ae4[(rk[3] >> 24)       ] & 0xff] ^
					_ad1[_ae4[(rk[3] >> 16) & 0xff] & 0xff] ^
					_ad2[_ae4[(rk[3] >>  8) & 0xff] & 0xff] ^
					_ad3[_ae4[(rk[3]      ) & 0xff] & 0xff];
				#else
				rk[0] =
					_ad0[_ae4[(rk[0]      ) & 0xff] & 0xff] ^
					_ad1[_ae4[(rk[0] >>  8) & 0xff] & 0xff] ^
					_ad2[_ae4[(rk[0] >> 16) & 0xff] & 0xff] ^
					_ad3[_ae4[(rk[0] >> 24)       ] & 0xff];
				rk[1] =
					_ad0[_ae4[(rk[1]      ) & 0xff] & 0xff] ^
					_ad1[_ae4[(rk[1] >>  8) & 0xff] & 0xff] ^
					_ad2[_ae4[(rk[1] >> 16) & 0xff] & 0xff] ^
					_ad3[_ae4[(rk[1] >> 24)       ] & 0xff];
				rk[2] =
					_ad0[_ae4[(rk[2]      ) & 0xff] & 0xff] ^
					_ad1[_ae4[(rk[2] >>  8) & 0xff] & 0xff] ^
					_ad2[_ae4[(rk[2] >> 16) & 0xff] & 0xff] ^
					_ad3[_ae4[(rk[2] >> 24)       ] & 0xff];
				rk[3] =
					_ad0[_ae4[(rk[3]      ) & 0xff] & 0xff] ^
					_ad1[_ae4[(rk[3] >>  8) & 0xff] & 0xff] ^
					_ad2[_ae4[(rk[3] >> 16) & 0xff] & 0xff] ^
					_ad3[_ae4[(rk[3] >> 24)       ] & 0xff];
				#endif
			}
		}
		return 0;
	}
	return -1;
}

#ifndef ASM_AESSETIV
int aesSetIV(aesParam* ap, const byte* iv)
{
	if (iv)
		memcpy(ap->fdback, iv, 16);
	else
		memset(ap->fdback, 0, 16);

	return 0;
}
#endif

#ifndef ASM_AESENCRYPT
int aesEncrypt(aesParam* ap, uint32_t* dst, const uint32_t* src)
{
	register uint32_t s0, s1, s2, s3;
	register uint32_t t0, t1, t2, t3;
	register uint32_t* rk = ap->k;

	s0 = src[0] ^ rk[0];
	s1 = src[1] ^ rk[1];
	s2 = src[2] ^ rk[2];
	s3 = src[3] ^ rk[3];

	etfs(4);		/* round 1 */
	esft(8);		/* round 2 */
	etfs(12);		/* round 3 */
	esft(16);		/* round 4 */
	etfs(20);		/* round 5 */
	esft(24);		/* round 6 */
	etfs(28);		/* round 7 */
	esft(32);		/* round 8 */
	etfs(36);		/* round 9 */

	if (ap->nr > 10)
	{
		esft(40);	/* round 10 */
		etfs(44);	/* round 11 */
		if (ap->nr > 12)
		{
			esft(48);	/* round 12 */
			etfs(52);	/* round 13 */
		}
	}

	rk += (ap->nr << 2);

	elr(); /* last round */

	dst[0] = s0;
	dst[1] = s1;
	dst[2] = s2;
	dst[3] = s3;

	return 0;
}
#endif

#ifndef ASM_AESDECRYPT
int aesDecrypt(aesParam* ap, uint32_t* dst, const uint32_t* src)
{
	register uint32_t s0, s1, s2, s3;
	register uint32_t t0, t1, t2, t3;
	register uint32_t* rk = ap->k;

	s0 = src[0] ^ rk[0];
	s1 = src[1] ^ rk[1];
	s2 = src[2] ^ rk[2];
	s3 = src[3] ^ rk[3];

	dtfs(4);		/* round 1 */
	dsft(8);		/* round 2 */
	dtfs(12);		/* round 3 */
	dsft(16);		/* round 4 */
	dtfs(20);		/* round 5 */
	dsft(24);		/* round 6 */
	dtfs(28);		/* round 7 */
	dsft(32);		/* round 8 */
	dtfs(36);		/* round 9 */

	if (ap->nr > 10)
	{
		dsft(40);	/* round 10 */
		dtfs(44);	/* round 11 */
		if (ap->nr > 12)
		{
			dsft(48);	/* round 12 */
			dtfs(52);	/* round 13 */
		}
	}

	rk += (ap->nr << 2);

	dlr(); /* last round */

	dst[0] = s0;
	dst[1] = s1;
	dst[2] = s2;
	dst[3] = s3;

	return 0;
}
#endif

uint32_t* aesFeedback(aesParam* ap)
{
	return ap->fdback;
}
