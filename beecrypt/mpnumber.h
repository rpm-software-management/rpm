/*
 * Copyright (c) 2003 Bob Deblier
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

/*!\file mpnumber.h
 * \brief Multi-precision numbers, headers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup MP_m
 */

#ifndef _MPNUMBER_H
#define _MPNUMBER_H

#include "mp.h"

/**
 */
typedef struct
{
	size_t	size;
/*@owned@*/ /*@relnull@*/
	mpw*	data;
} mpnumber;

#ifdef __cplusplus
extern "C" {
#endif

/**
 */
BEECRYPTAPI
void mpnzero(/*@out@*/ mpnumber* n)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI
void mpnsize(mpnumber* n, size_t size)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpninit(mpnumber* n, size_t size, const mpw* data)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI
void mpnfree(mpnumber* n)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI
void mpncopy(mpnumber* n, const mpnumber* copy)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI
void mpnwipe(mpnumber* n)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI
void mpnset   (mpnumber* n, size_t size, /*@null@*/ const mpw* data)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI
void mpnsetw  (mpnumber* n, mpw val)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
void mpnsethex(/*@out@*/ mpnumber* n, const char* hex)
	/*@modifies n->size, n->data @*/;

/**
 */
BEECRYPTAPI /*@unused@*/
int mpninv(/*@out@*/ mpnumber* inv, const mpnumber* k, const mpnumber* mod)
	/*@modifies inv->size, inv->data @*/;

#ifdef __cplusplus
}
#endif

#endif
