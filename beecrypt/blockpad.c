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
#if HAVE_MALLOC_H
# include <malloc.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif

memchunk* pkcs5Pad(int blockbytes, memchunk* tmp)
{
	if (tmp)
	{
		byte padvalue = blockbytes - (tmp->size % blockbytes);

		tmp = memchunkResize(tmp, tmp->size + padvalue);

		if (tmp)
			memset(tmp->data - padvalue, padvalue, padvalue);
	}

	return tmp;
}

memchunk* pkcs5Unpad(int blockbytes, memchunk* tmp)
{
	if (tmp)
	{
		byte padvalue;
		int i;

		if (tmp->data == (memchunk*) 0)
			return (memchunk*) 0;
		padvalue = tmp->data[tmp->size - 1];
		if (padvalue > blockbytes)
			return (memchunk*) 0;

		for (i = (tmp->size - padvalue); i < (tmp->size - 1); i++)
		{
			if (tmp->data[i] != padvalue)
				return (memchunk*) 0;
		}

		tmp->size -= padvalue;
/*		tmp->data = (byte*) realloc(tmp->data, tmp->size; */
	}

	/*@-temptrans@*/
	return tmp;
	/*@=temptrans@*/
}

memchunk* pkcs5PadCopy(int blockbytes, const memchunk* src)
{
	memchunk* tmp;
	byte padvalue = blockbytes - (src->size % blockbytes);

	if (src == (memchunk*) 0)
		return (memchunk*) 0;

	tmp = memchunkAlloc(src->size + padvalue);

	if (tmp)
	{
		memcpy(tmp->data, src->data, src->size);
		memset(tmp->data+src->size, padvalue, padvalue);
	}

	return tmp;
}

memchunk* pkcs5UnpadCopy(/*@unused@*/ int blockbytes, const memchunk* src)
{
	memchunk* tmp;
	byte padvalue;
	int i;

	if (src == (memchunk*) 0)
		return (memchunk*) 0;
	if (src->data == (memchunk*) 0)
		return (memchunk*) 0;

	padvalue = src->data[src->size - 1];

	for (i = (src->size - padvalue); i < (src->size - 1); i++)
	{
		if (src->data[i] != padvalue)
			return (memchunk*) 0;
	}

	tmp = memchunkAlloc(src->size - padvalue);

	if (tmp)
		memcpy(tmp->data, src->data, tmp->size);

	return tmp;
}
