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

/*!\file aes.h
 * \brief AES block cipher, as specified by NIST FIPS 197.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup BC_m BC_aes_m
 */

#ifndef _AES_H
#define _AES_H

#include "beecrypt/beecrypt.h"
#include "beecrypt/aesopt.h"

/*!\brief Holds all the parameters necessary for the AES cipher.
 * \ingroup BC_aes_m
 */
typedef struct
{
	/*!\var k
	 * \brief Holds the key expansion.
	 */
	uint32_t k[64];
	/*!\var nr
	 * \brief Number of rounds to be used in encryption/decryption.
	 */
	uint32_t nr;
	/*!\var fdback
	 * \brief Buffer to be used by block chaining or feedback modes.
	 */
	uint32_t fdback[4];
} aesParam;

#ifdef __cplusplus
extern "C" {
#endif

/*!\var aes
 * \brief Holds the full API description of the AES algorithm.
 */
extern const BEECRYPTAPI blockCipher aes;

/*!\fn int aesSetup(aesParam* ap, const byte* key, size_t keybits, cipherOperation op)
 * \brief This function performs the cipher's key expansion.
 * \param ap The cipher's parameter block.
 * \param key The key value.
 * \param keybits The number of bits in the key; legal values are:
 *  128, 192 and 256.
 * \param op ENCRYPT or DECRYPT.
 * \retval 0 on success.
 * \retval -1 on failure.
 */
BEECRYPTAPI
int			aesSetup   (aesParam* ap, const byte* key, size_t keybits, cipherOperation op);

/*!\fn int aesSetIV(aesParam* ap, const byte* iv)
 * \brief This function sets the Initialization Vector.
 * \note This function is only useful in block chaining or feedback modes.
 * \param ap The cipher's parameter block.
 * \param iv The initialization vector; may be null.
 * \retval 0 on success.
 */
BEECRYPTAPI
int			aesSetIV   (aesParam* ap, const byte* iv);

/*!\fn aesEncrypt(aesParam* ap, uint32_t* dst, const uint32_t* src)
 * \brief This function performs the raw AES encryption; it encrypts one block
 *  of 128 bits.
 * \param ap The cipher's parameter block.
 * \param dst The ciphertext; should be aligned on 32-bit boundary.
 * \param src The cleartext; should be aligned on 32-bit boundary.
 * \retval 0 on success.
 */
BEECRYPTAPI
int			aesEncrypt (aesParam* ap, uint32_t* dst, const uint32_t* src);

/*!\fn aesDecrypt(aesParam* ap, uint32_t* dst, const uint32_t* src)
 * \brief This function performs the raw AES decryption; it decrypts one block
 *  of 128 bits.
 * \param ap The cipher's parameter block.
 * \param dst The cleartext; should be aligned on 32-bit boundary.
 * \param src The ciphertext; should be aligned on 32-bit boundary.
 * \retval 0 on success.
 */
BEECRYPTAPI
int			aesDecrypt (aesParam* ap, uint32_t* dst, const uint32_t* src);

BEECRYPTAPI
uint32_t*	aesFeedback(aesParam* ap);

#ifdef __cplusplus
}
#endif

#endif
