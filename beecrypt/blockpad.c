/*
 * Copyright (c) 2000, 2001, 2002 Virtual Unlimited B.V.
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

/*!\file blockpad.c
 * \brief Blockcipher padding algorithms.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup BC_m
 */

#include "system.h"
#include "blockpad.h"
#include "debug.h"

memchunk* pkcs5Pad(size_t blockbytes, memchunk* tmp)
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

memchunk* pkcs5Unpad(size_t blockbytes, memchunk* tmp)
{
	if (tmp)
	{
		byte padvalue;
		unsigned int i;

/*@-usedef@*/ /* LCL: tmp->{data,size} not initialized? */
		if (tmp->data == (byte*) 0)
			return (memchunk*) 0;
		padvalue = tmp->data[tmp->size - 1];
/*@=usedef@*/
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

	/*@-temptrans -compdef @*/
	return tmp;
	/*@=temptrans =compdef @*/
}

memchunk* pkcs5PadCopy(size_t blockbytes, const memchunk* src)
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

memchunk* pkcs5UnpadCopy(/*@unused@*/ size_t blockbytes, const memchunk* src)
{
	memchunk* tmp;
	byte padvalue;
	unsigned int i;

	if (src == (memchunk*) 0)
		return (memchunk*) 0;
	if (src->data == (byte*) 0)
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
