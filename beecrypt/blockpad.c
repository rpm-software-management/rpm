/*
 * blockpad.c
 *
 * Blockcipher padding, code
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

#define BEECRYPT_DLL_EXPORT

#include "blockpad.h"

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif

int pkcs5PadInline(int blockbytes, memchunk* src)
{
	if (src != (memchunk*) 0)
	{
		byte padvalue = blockbytes - (src->size % blockbytes);

		src->size += padvalue;
		src->data = (byte*) realloc(src->data, src->size);
		memset(src->data - padvalue, padvalue, padvalue);

		return 0;
	}

	return -1;
}

int pkcs5UnpadInline(int blockbytes, memchunk* src)
{
	if (src != (memchunk*) 0)
	{
		byte padvalue = src->data[src->size - 1];

		int i;

		if (padvalue > blockbytes)
			return -1;

		for (i = (src->size - padvalue); i < (src->size - 1); i++)
		{
			if (src->data[i] != padvalue)
				return -1;
		}

		src->size -= padvalue;
/*		src->data = (byte*) realloc(src->data, src->size; */

		return 0;
	}

	return -1;
}

memchunk* pkcs5Pad(int blockbytes, const memchunk* src)
{
	memchunk* dst;

	if (src == (memchunk*) 0)
		return (memchunk*) 0;

	dst = (memchunk*) calloc(1, sizeof(memchunk));

	if (dst != (memchunk*) 0)
	{
		byte padvalue = blockbytes - (src->size % blockbytes);

		dst->size = src->size + padvalue;

		dst->data = (byte*) malloc(dst->size);

		if (dst->data == (byte*) 0)
		{
			free(dst);
			dst = (memchunk*) 0;
		}
		else
		{
			memcpy(dst->data, src->data, src->size);
			memset(dst->data+src->size, padvalue, padvalue);
		}
	}

	return dst;
}

memchunk* pkcs5Unpad(int blockbytes, const memchunk* src)
{
	memchunk* dst;

	if (src == (memchunk*) 0)
		return (memchunk*) 0;

	dst = (memchunk*) calloc(1, sizeof(memchunk));

	if (dst != (memchunk*) 0)
	{
		byte padvalue = src->data[src->size - 1];
		int i;

		for (i = (src->size - padvalue); i < (src->size - 1); i++)
		{
			if (src->data[i] != padvalue)
			{
				free(dst);
				return (memchunk*) 0;
			}
		}

		dst->size = src->size - padvalue;
		dst->data = (byte*) malloc(dst->size);

		if (dst->data == (byte*) 0)
		{
			free(dst);
			dst = (memchunk*) 0;
		}
		else
		{
			memcpy(dst->data, src->data, dst->size);
		}
	}

	return dst;
}
