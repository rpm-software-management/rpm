/** \ingroup MP_m
 * \file mp32number.h
 *
 * Multiprecision numbers, header.
 */

/*
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

/**
 */
typedef struct
{
	uint32  size;
/*@owned@*/ uint32* data;
} mp32number;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
BEECRYPTAPI
void mp32nzero(/*@out@*/ mp32number* n)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI
void mp32nsize(mp32number* n, uint32 size)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mp32ninit(mp32number* n, uint32 size, const uint32* data)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI
void mp32nfree(mp32number* n)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI
void mp32ncopy(/*@out@*/ mp32number* n, const mp32number* copy)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI
void mp32nwipe(mp32number* n)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI
void mp32nset   (mp32number* n, uint32 size, /*@null@*/ const uint32* data)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI
void mp32nsetw  (mp32number* n, uint32 val)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mp32nsethex(/*@out@*/ mp32number* n, const char* hex)
	/*@modifies n->size, n->data @*/;

#ifdef __cplusplus
}
#endif

#endif
