/*
 * blockpad.h
 *
 * Blockcipher padding, header
 *
 * Copyright (c) 2000, 2001 Virtual Unlimited B.V.
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

#ifndef _BLOCKPAD_H
#define _BLOCKPAD_H

#include "beecrypt.h"

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
int pkcs5PadInline  (int, memchunk*);
BEEDLLAPI
int pkcs5UnpadInline(int, memchunk*);

BEEDLLAPI
memchunk* pkcs5Pad  (int, const memchunk*);
BEEDLLAPI
memchunk* pkcs5Unpad(int, const memchunk*);

#ifdef __cplusplus
}
#endif

#endif
