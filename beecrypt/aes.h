/** \ingroup BC_aes_m BC_m
 * \file aes.h
 *
 * AES block cipher, header.
 *
 */

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

#ifndef _AES_H
#define _AES_H

/** \ingroup BC_aes_m
 */
typedef struct
{
	uint32 k[64];
	uint32 nr;
	uint32 fdback[4];
} aesParam;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup BC_aes_m
 */
/*@observer@*/ /*@checked@*/
extern const BEECRYPTAPI blockCipher aes;

/** \ingroup BC_aes_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int aesSetup  (aesParam* ap, const uint32* key, int keybits, cipherOperation op)
	/*@modifies ap @*/;
/*@=exportlocal@*/

/** \ingroup BC_aes_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int aesSetIV  (aesParam* ap, const uint32* iv)
	/*@modifies ap @*/;
/*@=exportlocal@*/

/** \ingroup BC_aes_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int aesEncrypt(aesParam* ap, uint32* dst, const uint32* src)
	/*@modifies dst @*/;
/*@=exportlocal@*/

/** \ingroup BC_aes_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int aesDecrypt(aesParam* ap, uint32* dst, const uint32* src)
	/*@modifies dst @*/;
/*@=exportlocal@*/

/** \ingroup BC_aes_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int aesECBEncrypt(aesParam* ap, int count, uint32* dst, const uint32* src)
	/*@modifies dst @*/;
/*@=exportlocal@*/

/** \ingroup BC_aes_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int aesECBDecrypt(aesParam* ap, int count, uint32* dst, const uint32* src)
	/*@modifies dst @*/;
/*@=exportlocal@*/

/** \ingroup BC_aes_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int aesCBCEncrypt(aesParam* ap, int count, uint32* dst, const uint32* src)
	/*@modifies ap, dst @*/;
/*@=exportlocal@*/

/** \ingroup BC_aes_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int aesCBCDecrypt(aesParam* ap, int count, uint32* dst, const uint32* src)
	/*@modifies ap, dst @*/;
/*@=exportlocal@*/

#ifdef __cplusplus
}
#endif

#endif
