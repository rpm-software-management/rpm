/*
 * blockmode.h
 *
 * Blockcipher operation modes, header
 *
 * Copyright (c) 2000 Virtual Unlimited B.V.
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

#ifndef _BLOCKMODE_H
#define _BLOCKMODE_H

#include "beecrypt.h"

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
int blockEncrypt(const blockCipher*, blockCipherParam*, cipherMode, int, uint32*, const uint32*)
	/*@*/;
BEEDLLAPI
int blockDecrypt(const blockCipher*, blockCipherParam*, cipherMode, int, uint32*, const uint32*)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif
