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

/** \ingroup BC_aes_m
 */
typedef struct
{
	uint32_t k[64];
	uint32_t nr;
	uint32_t fdback[4];
} aesParam;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup BC_aes_m
 */
/*@observer@*/ /*@unchecked@*/
extern const BEECRYPTAPI blockCipher aes;

/** \ingroup BC_aes_m
 * The cipher's setup function.
 *
 * This function expands the key depending on whether the ENCRYPT or DECRYPT
 * operation was selected.
 *
 * @param ap		parameter block
 * @param key		key value
 * @param keybits	number of bits in the key (128, 192 or 256)
 * @param op		ENCRYPT or DECRYPT.
 * @retval		0 on success, -1 on failure.
 */
/*@-exportlocal@*/
BEECRYPTAPI
int aesSetup  (aesParam* ap, const byte* key, size_t keybits, cipherOperation op)
	/*@modifies ap @*/;
/*@=exportlocal@*/

/** \ingroup BC_aes_m
 * The Initialization Vector setup function.
 *
 * This function is only necessary in block chaining or feedback modes.
 *
 * @param ap		parameter block
 * @param iv		initialization vector (or NULL)
 * @retval		0 on success.
 */
/*@-exportlocal@*/
BEECRYPTAPI
int aesSetIV  (aesParam* ap, /*@null@*/ const byte* iv)
	/*@modifies ap @*/;
/*@=exportlocal@*/

/** \ingroup BC_aes_m
 * The raw encryption function.
 *
 * This function encrypts one block of data; the size of a block is 128 bits.
 *
 * @param ap		parameter block
 * @param dst		ciphertext (aligned on 32-bit boundary)
 * @param src		cleartext (aligned on 32-bit boundary)
 * @retval		0 on success.
 */
/*@-exportlocal@*/
BEECRYPTAPI
int aesEncrypt(aesParam* ap, uint32_t* dst, const uint32_t* src)
	/*@modifies dst @*/;
/*@=exportlocal@*/

/** \ingroup BC_aes_m
 * The raw decryption function.
 *
 * This function decrypts one block of data; the size of a block is 128 bits.
 *
 * @param ap		parameter block
 * @param dst		cleartext (aligned on 32-bit boundary)
 * @param src		ciphertext (aligned on 32-bit boundary)
 * @retval		0 on success.
 */
/*@-exportlocal@*/
BEECRYPTAPI
int aesDecrypt(aesParam* ap, uint32_t* dst, const uint32_t* src)
	/*@modifies dst @*/;
/*@=exportlocal@*/

/** \ingroup BC_aes_m
 */
/*@-exportlocal@*/
BEECRYPTAPI /*@observer@*/
uint32_t* aesFeedback(aesParam* ap)
	/*@*/;
/*@=exportlocal@*/

#ifdef __cplusplus
}
#endif

#endif
