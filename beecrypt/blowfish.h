/** \ingroup BC_blowfish_m BC_m
 * \file blowfish.h
 *
 * Blowfish block cipher, header.
 */

/*
 * Copyright (c) 1999, 2000 Virtual Unlimited B.V.
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

#ifndef _BLOWFISH_H
#define _BLOWFISH_H

#include "beecrypt.h"
#include "blowfishopt.h"

#define BLOWFISHROUNDS	16
#define BLOWFISHPSIZE	(BLOWFISHROUNDS+2)

/** \ingroup BC_blowfish_m
 */
typedef struct
{
	uint32 p[BLOWFISHPSIZE];
	uint32 s[1024];
	uint32 fdback[2];
} blowfishParam;

#ifdef __cplusplus
extern "C" {
#endif

/** \ingroup BC_blowfish_m
 */
/*@observer@*/ /*@checked@*/
extern const BEECRYPTAPI blockCipher blowfish;

/** \ingroup BC_blowfish_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int blowfishSetup  (blowfishParam* bp, const uint32* key, int keybits, cipherOperation op)
	/*@modifies bp */;
/*@=exportlocal@*/

/** \ingroup BC_blowfish_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int blowfishSetIV  (blowfishParam* bp, const uint32* iv)
	/*@modifies bp */;
/*@=exportlocal@*/

/** \ingroup BC_blowfish_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int blowfishEncrypt(blowfishParam* bp, uint32* dst, const uint32* src)
	/*@modifies bp, dst */;
/*@=exportlocal@*/

/** \ingroup BC_blowfish_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int blowfishDecrypt(blowfishParam* bp, uint32* dst, const uint32* src)
	/*@modifies bp, dst */;
/*@=exportlocal@*/

/** \ingroup BC_blowfish_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int blowfishECBEncrypt(blowfishParam* bp, int count, uint32* dst, const uint32* src)
	/*@modifies bp, dst */;
/*@=exportlocal@*/

/** \ingroup BC_blowfish_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int blowfishECBDecrypt(blowfishParam* bp, int count, uint32* dst, const uint32* src)
	/*@modifies bp, dst */;
/*@=exportlocal@*/

/** \ingroup BC_blowfish_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int blowfishCBCEncrypt(blowfishParam* bp, int count, uint32* dst, const uint32* src)
	/*@modifies bp, dst */;
/*@=exportlocal@*/

/** \ingroup BC_blowfish_m
 */
/*@-exportlocal@*/
BEECRYPTAPI
int blowfishCBCDecrypt(blowfishParam* bp, int count, uint32* dst, const uint32* src)
	/*@modifies bp, dst */;
/*@=exportlocal@*/

#ifdef __cplusplus
}
#endif

#endif
