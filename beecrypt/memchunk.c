/*
 * memchunk.c
 *
 * BeeCrypt memory block handling, code
 *
 * Copyright (c) 2001 Virtual Unlimited B.V.
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

#include "memchunk.h"

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_MALLOC_H
# include <malloc.h>
#endif

/*@-compdef@*/	/* tmp-?data is undefined */
memchunk* memchunkAlloc(int size)
{
	memchunk* tmp = (memchunk*) calloc(1, sizeof(memchunk));

	if (tmp)
	{
		tmp->size = size;
		tmp->data = (byte*) malloc(size);

		if (tmp->data == (byte*) 0)
		{
			free(tmp);
			tmp = 0;
		}
	}

	return tmp;
}
/*@=compdef@*/

void memchunkFree(memchunk* m)
{
	if (m)
	{
		if (m->data)
		{
			free(m->data);

			m->size = 0;
			m->data = (byte*) 0;
		}
		free(m);
	}
}

memchunk* memchunkResize(memchunk* m, int size)
{
	if (m)
	{
		if (m->data)
			m->data = (byte*) realloc(m->data, size);
		else
			m->data = (byte*) malloc(size);

		if (m->data == (byte*) 0)
		{
			free(m);
			m = (memchunk*) 0;
		}
		else
			/*@-nullderef@*/
			m->size = size;
			/*@=nullderef@*/
	}

	/*@-nullret -compdef @*/	/* LCL: m->data might be NULL */
	return m;
	/*@=nullret =compdef@*/
}
