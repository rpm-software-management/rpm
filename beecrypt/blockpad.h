/** \ingroup BC_m
 * \file blockpad.h
 *
 * Blockcipher padding, header.
 */

/*
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

/**
 * Enlarge buffer to boundary.
 * @param blockbytes	desired block alignment/pad boundary
 * @param tmp		buffer to pad
 * @return		buffer with pad added
 */
BEECRYPTAPI /*@only@*/ /*@null@*/ /*@unused@*/
memchunk* pkcs5Pad  (int blockbytes, /*@only@*/ /*@null@*/ memchunk* tmp)
	/*@*/;

/**
 * Shrink buffer to boundary.
 * @param blockbytes	desired block alignment/pad boundary
 * @param tmp		buffer to unpad
 * @return		buffer with pad removed
 */
BEECRYPTAPI /*@only@*/ /*@null@*/
memchunk* pkcs5Unpad(int blockbytes, /*@returned@*/ /*@null@*/ memchunk* tmp)
	/*@modifies tmp */;

/**
 * Copy/enlarge buffer to boundary.
 * @param blockbytes	desired block alignment/pad boundary
 * @param tmp		buffer to pad
 * @return		copy of buffer with pad added
 */
BEECRYPTAPI /*@only@*/ /*@null@*/
memchunk* pkcs5PadCopy  (int blockbytes, const memchunk* src)
	/*@*/;

/**
 * Copy/shrink buffer to boundary.
 * @param blockbytes	desired block alignment/pad boundary
 * @param tmp		buffer to unpad
 * @return		copy of buffer with pad removed
 */
BEECRYPTAPI /*@only@*/ /*@null@*/ /*@unused@*/
memchunk* pkcs5UnpadCopy(int blockbytes, const memchunk* src)
	/*@*/;

#ifdef __cplusplus
}
#endif

#endif
