/** \ingroup MP_m
 * \file mpnumber.c
 *
 * Multiple precision numbers, code.
 */

/*
 *
 * Copyright (c) 2003 Bob Deblier
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

#include "system.h"
#include "mpnumber.h"
#include "mp.h"
#include "debug.h"

void mpnzero(mpnumber* n)
{
	n->size = 0;
	n->data = (mpw*) 0;
}

/*@-compdef @*/	/* n->data not initialized */
void mpnsize(mpnumber* n, size_t size)
{
	if (size)
	{
		if (n->data)
		{
			if (n->size != size)
				n->data = (mpw*) realloc(n->data, size * sizeof(*n->data));
		}
		else
			n->data = (mpw*) malloc(size * sizeof(*n->data));

		if (n->data)
			n->size = size;
		else
		{
			n->size = 0;
			n->data = (mpw*) 0;
		}

	}
	else if (n->data)
	{
		free(n->data);
		n->data = (mpw*) 0;
		n->size = 0;
	}
	else
		{};
}
/*@=compdef @*/

/*@-boundswrite@*/
void mpninit(mpnumber* n, size_t size, const mpw* data)
{
	n->size = size;
	if (n->data)
	{
		free(n->data);
		n->data = (mpw*) 0;
	}
	n->data = (mpw*) malloc(size * sizeof(*n->data));

	if (n->data && data)
		mp32copy(size, n->data, data);
}
/*@=boundswrite@*/

void mpnfree(mpnumber* n)
{
	if (n->data)
	{
		free(n->data);
		n->data = (mpw*) 0;
	}
	n->size = 0;
}

void mpncopy(mpnumber* n, const mpnumber* copy)
{
	mpnset(n, copy->size, copy->data);
}

void mpnwipe(mpnumber* n)
{
	if (n->data)
		mp32zero(n->size, n->data);
}

/*@-boundswrite@*/
void mpnset(mpnumber* n, size_t size, const mpw* data)
{
	if (size)
	{
		if (n->data)
		{
			if (n->size != size)
				n->data = (mpw*) realloc(n->data, size * sizeof(*n->data));
		}
		else
			n->data = (mpw*) malloc(size * sizeof(*n->data));

		if (n->data && data)
			/*@-nullpass@*/ /* data is notnull */
			mp32copy(n->size = size, n->data, data);
			/*@=nullpass@*/
		else
		{
			n->size = 0;
			n->data = (mpw*) 0;
		}
	}
	else if (n->data)
	{
		free(n->data);
		n->data = (mpw*) 0;
		n->size = 0;
	}
	else
		{};
}
/*@=boundswrite@*/

/*@-boundswrite@*/
void mpnsetw(mpnumber* n, mpw val)
{
	if (n->data)
	{
		if (n->size != 1)
			n->data = (mpw*) realloc(n->data, 1 * sizeof(*n->data));
	}
	else
		n->data = (mpw*) malloc(1 * sizeof(*n->data));

	if (n->data)
	{
		n->size = 1;
		n->data[0] = val;
	}
	else
	{
		n->size = 0;
		n->data = (mpw*) 0;
	}
}
/*@=boundswrite@*/

/*@-boundswrite@*/
/*@-usedef @*/	/* n->data may be NULL */
void mpnsethex(mpnumber* n, const char* hex)
{
	register size_t len = strlen(hex);
	register size_t size = (len+7) >> 3;
	uint8 rem = (uint8)(len & 0x7);

	if (n->data)
	{
		if (n->size != size)
			n->data = (mpw*) realloc(n->data, size * sizeof(*n->data));
	}
	else
		n->data = (mpw*) malloc(size * sizeof(*n->data));

	if (n->data)
	{
		register size_t  val = 0;
		register mpw* dst = n->data;
		register char ch;

		n->size = size;

		while (len-- > 0)
		{
			ch = *(hex++);
			val <<= 4;
			if (ch >= '0' && ch <= '9')
				val += (ch - '0');
			else if (ch >= 'A' && ch <= 'F')
				val += (ch - 'A') + 10;
			else if (ch >= 'a' && ch <= 'f')
				val += (ch - 'a') + 10;
			else
				{};

			if ((len & 0x7) == 0)
			{
				*(dst++) = val;
				val = 0;
				rem = 0;
			} else
				rem = 1;
		}
		if (rem != 0) {
			*dst = val;
		}
	}
	else {
		n->size = 0;
		n->data = (uint32*)0;
	}
}
/*@=usedef @*/
/*@=boundswrite@*/
