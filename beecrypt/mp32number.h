/*
 * mp32number.h
 *
 * Multiprecision numbers, header
 *
 * Copyright (c) 1997, 1998, 1999, 2000, 2001 Virtual Unlimited B.V.
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

#ifndef _MP32NUMBER_H
#define _MP32NUMBER_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

typedef struct
{
	uint32  size;
/*@only@*/ uint32* data;
} mp32number;

#ifdef __cplusplus
extern "C" {
#endif

BEEDLLAPI
void mp32nzero(mp32number* n)
	/*@modifies n @*/;
BEEDLLAPI
void mp32nsize(mp32number* n, uint32 size)
	/*@modifies n @*/;
BEEDLLAPI
void mp32ninit(mp32number* n, uint32 size, const uint32* data)
	/*@modifies n @*/;
BEEDLLAPI
void mp32nfree(mp32number* n)
	/*@modifies n @*/;
BEEDLLAPI
void mp32ncopy(mp32number* n, const mp32number* copy)
	/*@modifies n @*/;
BEEDLLAPI
void mp32nwipe(mp32number* n)
	/*@modifies n @*/;

BEEDLLAPI
void mp32nset   (mp32number* n, uint32 size, /*@null@*/ const uint32* data)
	/*@modifies n @*/;
BEEDLLAPI
void mp32nsetw  (mp32number* n, uint32 val)
	/*@modifies n @*/;
BEEDLLAPI
void mp32nsethex(mp32number* n, const char* hex)
	/*@modifies n @*/;

#ifdef __cplusplus
}
#endif

#endif
