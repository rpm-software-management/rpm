/*
 * mp32number.c
 *
 * Multiple precision numbers, code
 *
 * Copyright (c) 1997-2000 Virtual Unlimited B.V.
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
#include <stdlib.h>
#endif

void mp32nzero(mp32number* n)
{
	n->size = 0;
	n->data = (uint32*) 0;
}

void mp32nsize(mp32number* n, uint32 size)
{
	n->size = size;
	n->data = (uint32*) malloc(size * sizeof(uint32));
}

void mp32ninit(mp32number* n, uint32 size, const uint32* data)
{
	n->size = size;
	n->data = (uint32*) malloc(size * sizeof(uint32));

	if (n->data)
	{
		mp32copy(size, n->data, data);
	}
}

void mp32nfree(mp32number* n)
{
	if (n->data)
	{
		free(n->data);
		n->data = (uint32*) 0;
	}
	n->size = 0;
}

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
		{
			n->data = (uint32*) malloc(size * sizeof(uint32));
		}
	}
	else
	{
		free(n->data);
		n->data = (uint32*) 0;
	}

	if (n->data)
	{
		n->size = size;
		mp32copy(size, n->data, data);
	}
	else
		n->size = 0;
}

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
		n->size = 0;
}

void mp32nsethex(mp32number* n, const char* hex)
{
	int length = strlen(hex);
	int size = (length+7) >> 3;
	int rem = length & 0x7;

	if (n->data)
	{
		if (n->size != size)
			n->data = (uint32*) realloc(n->data, size * sizeof(uint32));
	}
	else
		n->data = (uint32*) malloc(size * sizeof(uint32));

	if (n->data)
	{
		register uint32  temp = 0;
		register uint32* dst = n->data;
		register char ch;

		n->size = size;

		while (length-- > 0)
		{
			ch = *(hex++);
			temp <<= 4;
			if (ch >= '0' && ch <= '9')
				temp += (ch - '0');
			else if (ch >= 'A' && ch <= 'F')
				temp += (ch - 'A') + 10;
			else if (ch >= 'a' && ch <= 'f')
				temp += (ch - 'a') + 10;

			if ((length & 0x7) == 0)
			{
				*(dst++) = temp;
				temp = 0;
			}
		}
		if (rem)
			*dst = temp;
	}
	else
		n->size = 0;
}
