/*
 * Copyright (c) 1999, 2000, 2002 Virtual Unlimited B.V.
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

/*!\file hmac.h
 * \brief HMAC algorithm, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup HMAC_m
 */

#ifndef _HMAC_H
#define _HMAC_H

#include "beecrypt.h"

/*!\ingroup HMAC_m
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
BEECRYPTAPI
int hmacSetup (byte* kxi, byte* kxo, const hashFunction* hash, hashFunctionParam* param, const byte* key, size_t keybits)
	/*@modifies kxi, kxo, param */;

/**
 */
BEECRYPTAPI
int hmacReset (const byte* kxi, const hashFunction* hash, hashFunctionParam* param)
	/*@modifies param */;

/**
 */
BEECRYPTAPI
int hmacUpdate(const hashFunction* hash, hashFunctionParam* param, const byte* data, size_t size)
	/*@modifies param */;

/**
 */
BEECRYPTAPI
int hmacDigest(const byte* kxo, const hashFunction* hash, hashFunctionParam* param, /*@out@*/ byte* data)
	/*@modifies data */;

#ifdef __cplusplus
}
#endif

#endif
