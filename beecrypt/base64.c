/*
 * base64.c
 *
 * Base64 encoding/decoding, code
 *
 * Copyright (c) 2000 Virtual Unlimited B.V.
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

#include "base64.h"

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_CTYPE_H
# include <ctype.h>
#endif

/*@observer@*/ static const char* to_b64 =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* encode 72 characters per line */
#define CHARS_PER_LINE	72

char* b64enc(const memchunk* chunk)
{
	int div = chunk->size / 3;
	int rem = chunk->size % 3;
	int chars = div*4 + rem + 1;
	int newlines = (chars + CHARS_PER_LINE - 1) / CHARS_PER_LINE;

	const byte* data = chunk->data;
	char* string = (char*) malloc(chars + newlines + 1);

	if (string)
	{
		register char* buf = string;

		chars = 0;

		/*@+charindex@*/
		while (div > 0)
		{
			buf[0] = to_b64[ (data[0] >> 2) & 0x3f];
			buf[1] = to_b64[((data[0] << 4) & 0x30) + ((data[1] >> 4) & 0xf)];
			buf[2] = to_b64[((data[1] << 2) & 0x3c) + ((data[2] >> 6) & 0x3)];
			buf[3] = to_b64[  data[2] & 0x3f];
			data += 3;
			buf += 4;
			div--;
			chars += 4;
			if (chars == CHARS_PER_LINE)
			{
				chars = 0;
				*(buf++) = '\n';
			}
		}

		switch (rem)
		{
		case 2:
			buf[0] = to_b64[ (data[0] >> 2) & 0x3f];
			buf[1] = to_b64[((data[0] << 4) & 0x30) + ((data[1] >> 4) & 0xf)];
			buf[2] = to_b64[ (data[1] << 2) & 0x3c];
			buf[3] = '=';
			buf += 4;
			chars += 4;
			break;
		case 1:
			buf[0] = to_b64[ (data[0] >> 2) & 0x3f];
			buf[1] = to_b64[ (data[0] << 4) & 0x30];
			buf[2] = '=';
			buf[3] = '=';
			buf += 4;
			chars += 4;
			break;
		}
		/*@=charindex@*/

	/* 	*(buf++) = '\n'; This would result in a buffer overrun */
		*buf = '\0';
	}

	return string;
}

memchunk* b64dec(const char* string)
{
	/* return a decoded memchunk, or a null pointer in case of failure */

	memchunk* rc = 0;

	if (string)
	{
		register int length = strlen(string);

		/* do a format verification first */
		if (length > 0)
		{
			register int count = 0, rem = 0;
			register const char* tmp = string;

			while (length > 0)
			{
				register int skip = strspn(tmp, to_b64);
				count += skip;
				length -= skip;
				tmp += skip;
				if (length > 0)
				{
					register int i, vrfy = strcspn(tmp, to_b64);

					for (i = 0; i < vrfy; i++)
					{
						if (isspace(tmp[i]))
							continue;

						if (tmp[i] == '=')
						{
							/* we should check if we're close to the end of the string */
							rem = count % 4;

							/* rem must be either 2 or 3, otherwise no '=' should be here */
							if (rem < 2)
								return 0;

							/* end-of-message recognized */
							break;
						}
						else
						{
							/* Transmission error; RFC tells us to ignore this, but:
							 *  - the rest of the message is going to even more corrupt since we're sliding bits out of place
							 * If a message is corrupt, it should be dropped. Period.
							 */

							return 0;
						}
					}

					length -= vrfy;
					tmp += vrfy;
				}
			}

			rc = memchunkAlloc((count / 4) * 3 + (rem ? (rem - 1) : 0));

			if (rc)
			{
				if (count > 0)
				{
					register int i, qw = 0, tw = 0;
					register byte* data = rc->data;

					length = strlen(tmp = string);

					for (i = 0; i < length; i++)
					{
						register char ch = string[i];
						register byte bits;

						if (isspace(ch))
							continue;

						bits = 0;
						if ((ch >= 'A') && (ch <= 'Z'))
						{
							bits = (byte) (ch - 'A');
						}
						else if ((ch >= 'a') && (ch <= 'z'))
						{
							bits = (byte) (ch - 'a' + 26);
						}
						else if ((ch >= '0') && (ch <= '9'))
						{
							bits = (byte) (ch - '0' + 52);
						}
						else if (ch == '=')
							break;

						switch (qw++)
						{
						case 0:
							data[tw+0] = (bits << 2) & 0xfc;
							break;
						case 1:
							data[tw+0] |= (bits >> 4) & 0x03;
							data[tw+1] = (bits << 4) & 0xf0;
							break;
						case 2:
							data[tw+1] |= (bits >> 2) & 0x0f;
							data[tw+2] = (bits << 6) & 0xc0;
							break;
						case 3:
							data[tw+2] |= bits & 0x3f;
							break;
						}

						if (qw == 4)
						{
							qw = 0;
							tw += 3;
						}
					}
				}
			}
		}
	}

	return rc;
}
