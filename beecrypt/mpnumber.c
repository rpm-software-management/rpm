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

/*!\file mpnumber.c
 * \brief Multi-precision numbers.
 * \author Bob Deblier <bob.deblier@pandora.be>
 * \ingroup MP_m
 */

#define BEECRYPT_DLL_EXPORT

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "beecrypt/mpnumber.h"

void mpnzero(mpnumber* n)
{
	n->size = 0;
	n->data = (mpw*) 0;
}

void mpnsize(mpnumber* n, size_t size)
{
	if (size)
	{
		if (n->data)
		{
			if (n->size != size)
			{
				if (size < n->size)
				{
					register size_t offset = n->size - size;

					memmove(n->data, n->data + offset, offset * sizeof(mpw));
				}
				n->data = (mpw*) realloc(n->data, size * sizeof(mpw));
			}
		}
		else
			n->data = (mpw*) malloc(size * sizeof(mpw));

		if (n->data == (mpw*) 0)
			n->size = 0;
		else
			n->size = size;

	}
	else if (n->data)
	{
		free(n->data);
		n->data = (mpw*) 0;
		n->size = 0;
	}
}

void mpninit(mpnumber* n, size_t size, const mpw* data)
{
	n->size = size;
	n->data = (mpw*) malloc(size * sizeof(mpw));

	if (n->data)
		mpcopy(size, n->data, data);
}

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
	if (n->data != (mpw*) 0)
		mpzero(n->size, n->data);
}

void mpnset(mpnumber* n, size_t size, const mpw* data)
{
	if (size)
	{
		if (n->data)
		{
			if (n->size != size)
				n->data = (mpw*) realloc(n->data, size * sizeof(mpw));
		}
		else
			n->data = (mpw*) malloc(size * sizeof(mpw));

		if (n->data)
			mpcopy(n->size = size, n->data, data);
		else
			n->size = 0;
	}
	else if (n->data)
	{
		free(n->data);
		n->data = (mpw*) 0;
		n->size = 0;
	}
}

void mpnsetw(mpnumber* n, mpw val)
{
	if (n->data)
	{
		if (n->size != 1)
			n->data = (mpw*) realloc(n->data, sizeof(mpw));
	}
	else
		n->data = (mpw*) malloc(sizeof(mpw));

	if (n->data)
	{
		n->size = 1;
		n->data[0] = val;
	}
	else
		n->size = 0;
}

int mpnsetbin(mpnumber* n, const byte* osdata, size_t ossize)
{
	int rc = -1;
	size_t size;

	/* skip zero bytes */
	while ((*osdata == 0) && ossize)
	{
		osdata++;
		ossize--;
	}

	size = MP_BYTES_TO_WORDS(ossize + MP_WBYTES - 1);

	if (n->data)
	{
		if (n->size != size)
			n->data = (mpw*) realloc(n->data, size * sizeof(mpw));
	}
	else
		n->data = (mpw*) malloc(size * sizeof(mpw));

	if (n->data)
	{
		n->size = size;

		rc = os2ip(n->data, size, osdata, ossize);
	}
	else
		n->size = 0;

	return rc;
}

int mpnsethex(mpnumber* n, const char* hex)
{
	int rc = -1;
	size_t len = strlen(hex);
	size_t size = MP_NIBBLES_TO_WORDS(len + MP_WNIBBLES - 1);

	if (n->data)
	{
		if (n->size != size)
			n->data = (mpw*) realloc(n->data, size * sizeof(mpw));
	}
	else
		n->data = (mpw*) malloc(size * sizeof(mpw));

	if (n->data)
	{
		n->size = size;

		rc = hs2ip(n->data, size, hex, len);
	}
	else
		n->size = 0;

	return rc;
}

int mpninv(mpnumber* inv, const mpnumber* k, const mpnumber* mod)
{
	int rc = 0;
	size_t size = mod->size;
	mpw* wksp = (mpw*) malloc((7*size+6) * sizeof(mpw));

	if (wksp)
	{
		mpnsize(inv, size);
		mpsetx(size, wksp, k->size, k->data);
		rc = mpextgcd_w(size, mod->data, wksp, inv->data, wksp+size);
		free(wksp);
	}

	return rc;
}

size_t mpntrbits(mpnumber* n, size_t bits)
{
	size_t sigbits = mpbits(n->size, n->data);
	size_t offset = 0;

	if (sigbits < bits)
	{
		/* no need to truncate */
		return sigbits;
	}
	else
	{
		size_t allbits = MP_BITS_TO_WORDS(n->size + MP_WBITS - 1);

		while ((allbits - bits) > MP_WBITS)
		{
			/* zero a word */
			n->data[offset++] = 0;
			allbits -= MP_WBITS;
		}

		if ((allbits - bits))
		{
			/* mask the next word */
			n->data[offset] &= (MP_ALLMASK >> (MP_WBITS - bits));

			/* resize the number */
			mpnsize(n, n->size - offset);

			/* finally return the number of remaining bits */
			return bits;
		}
		else
		{
			/* nothing remains */
			mpnsetw(n, 0);
			return 0;
		}
	}
}

size_t mpnbits(const mpnumber* n)
{
	return mpbits(n->size, n->data);
}
