/** \ingroup MP_m
 * \file mp32number.c
 *
 * Multiple precision numbers, code.
 */

/*
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

#define BEECRYPT_DLL_EXPORT

#include "mp32number.h"
#include "mp32.h"

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_MALLOC_H
# include <malloc.h>
#endif

/*@-nullstate@*/	/* n->data may be NULL */
void mp32nzero(mp32number* n)
{
	n->size = 0;
	n->data = (uint32*) 0;
}
/*@=nullstate@*/

/*@-nullstate -compdef @*/	/* n->data may be NULL */
void mp32nsize(mp32number* n, uint32 size)
{
	if (size)
	{
		if (n->data)
		{
			if (n->size != size)
				n->data = (uint32*) realloc(n->data, size * sizeof(uint32));
		}
		else
			n->data = (uint32*) malloc(size * sizeof(uint32));

		if (n->data)
			n->size = size;
		else
		{
			n->size = 0;
			n->data = (uint32*) 0;
		}

	}
	else if (n->data)
	{
		free(n->data);
		n->size = 0;
		n->data = (uint32*) 0;
	}
	else
		{};
}
/*@=nullstate =compdef @*/

/*@-nullstate@*/	/* n->data may be NULL */
void mp32ninit(mp32number* n, uint32 size, const uint32* data)
{
	n->size = size;
	n->data = (uint32*) malloc(size * sizeof(uint32));

	if (n->data && data)
		mp32copy(size, n->data, data);
}
/*@=nullstate@*/

/*@-nullstate@*/	/* n->data may be NULL */
void mp32nfree(mp32number* n)
{
	if (n->data)
	{
		free(n->data);
		n->data = (uint32*) 0;
	}
	n->size = 0;
}
/*@=nullstate@*/

void mp32ncopy(mp32number* n, const mp32number* copy)
{
	/*@-compdef@*/
	mp32nset(n, copy->size, copy->data);
	/*@=compdef@*/
}

void mp32nwipe(mp32number* n)
{
	if (n->data)
		mp32zero(n->size, n->data);
}

/*@-nullstate@*/	/* n->data may be NULL */
void mp32nset(mp32number* n, uint32 size, const uint32* data)
{
	if (size)
	{
		if (n->data)
		{
			if (n->size != size)
				n->data = (uint32*) realloc(n->data, size * sizeof(uint32));
		}
		else
			n->data = (uint32*) malloc(size * sizeof(uint32));

		if (n->data && data)
			/*@-nullpass@*/	/* LCL: data != NULL */
			mp32copy(n->size = size, n->data, data);
			/*@=nullpass@*/
		else
		{
			n->size = 0;
			n->data = (uint32*) 0;
		}
	}
	else if (n->data)
	{
		free(n->data);
		n->data = (uint32*) 0;
		n->size = 0;
	}
	else
		{};
}
/*@=nullstate@*/

/*@-nullstate@*/	/* n->data may be NULL */
void mp32nsetw(mp32number* n, uint32 val)
{
	if (n->data)
	{
		if (n->size != 1)
			n->data = (uint32*) realloc(n->data, sizeof(uint32));
	}
	else
		n->data = (uint32*) malloc(sizeof(uint32));

	if (n->data)
	{
		n->size = 1;
		n->data[0] = val;
	}
	else
	{
		n->size = 0;
		n->data = (uint32*) 0;
	}
}
/*@=nullstate@*/

/*@-nullstate -compdef @*/	/* n->data may be NULL */
void mp32nsethex(mp32number* n, const char* hex)
{
	uint32 length = strlen(hex);
	uint32 size = (length+7) >> 3;
	uint8 rem = (uint8)(length & 0x7);

	if (n->data)
	{
		if (n->size != size)
			n->data = (uint32*) realloc(n->data, size * sizeof(uint32));
	}
	else
		n->data = (uint32*) malloc(size * sizeof(uint32));

	if (n->data)
	{
		register uint32  val = 0;
		register uint32* dst = n->data;
		register char ch;

		n->size = size;

		while (length-- > 0)
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

			if ((length & 0x7) == 0)
			{
				*(dst++) = val;
				val = 0;
			}
		}
		if (rem != 0)
			*dst = val;
	}
	else {
		n->size = 0;
		n->data = (uint32*)0;
	}
}
/*@=nullstate =compdef @*/
