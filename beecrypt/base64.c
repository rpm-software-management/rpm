/**
 * \file base64.c
 *
 * Base64 encoding/decoding, code.
 */

/*
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

static int _debug = 0;

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef	DYING
#if HAVE_CTYPE_H
# include <ctype.h>
#endif

/*@observer@*/ static const char* to_b64 =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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
#else

#include <stdio.h>

int b64encode_chars_per_line = B64ENCODE_CHARS_PER_LINE;

const char * b64encode_eolstr = B64ENCODE_EOLSTR;

/*@-internalglobs -modfilesys @*/
char * b64encode (const void * data, int ns)
{
    static char b64enc[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const char *e;
    const unsigned char *s = data;
    unsigned char *t, *te;
    int nt;
    int lc;
    unsigned c;

    if (s == NULL)	return NULL;
    if (*s == '\0')	return calloc(1, sizeof(*t));

    if (ns == 0) ns = strlen(s);
    nt = ((ns + 2) / 3) * 4;

    /* Add additional bytes necessary for eol string(s). */
    if (b64encode_chars_per_line > 0 && b64encode_eolstr != NULL) {
	lc = (nt + b64encode_chars_per_line - 1) / b64encode_chars_per_line;
	if (((nt + b64encode_chars_per_line - 1) % b64encode_chars_per_line) != 0)
	    ++lc;
	nt += lc * strlen(b64encode_eolstr);
    }

    t = te = malloc(nt + 1);

    lc = 0;
    if (te)
    while (ns) {

if (_debug)
fprintf(stderr, "%7u %02x %02x %02x -> %02x %02x %02x %02x\n",
(unsigned)ns, (unsigned)s[0], (unsigned)s[1], (unsigned)s[2],
(unsigned)(s[0] >> 2),
(unsigned)((s[0] & 0x3) << 4) | (s[1] >> 4),
(unsigned)((s[1] & 0xf) << 2) | (s[2] >> 6),
(unsigned)(s[2]& 0x3f));
	c = *s++;
	*te++ = b64enc[ (c >> 2) ], lc++;
	*te++ = b64enc[ ((c & 0x3) << 4) | (*s >> 4) ], lc++;
	if (--ns == 0) {
	    *te++ = '=';
	    *te++ = '=';
	    continue;
	}
	c = *s++;
	*te++ = b64enc[ ((c & 0xf) << 2) | (*s >> 6) ], lc++;
	if (--ns == 0) {
	    *te++ = '=';
	    continue;
	}
	*te++ = b64enc[ (int)(*s & 0x3f) ], lc++;

	/* Append eol string if desired. */
	if (b64encode_chars_per_line > 0 && b64encode_eolstr != NULL) {
	    if (lc >= b64encode_chars_per_line) {
		for (e = b64encode_eolstr; *e; e++)
		    *te++ = *e;
		lc = 0;
	    }
	}
	s++;
	--ns;
    }

    if (te) {
	/* Append eol string if desired. */
	if (b64encode_chars_per_line > 0 && b64encode_eolstr != NULL) {
	    if (lc != 0) {
		for (e = b64encode_eolstr; *e; e++)
		    *te++ = *e;
	    }
	}
	*te = '\0';
    }

    /*@-mustfree@*/
    return t;
    /*@=mustfree@*/
}
/*@=internalglobs =modfilesys @*/
#endif

#ifdef	DYING
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
							/*@innercontinue@*/ continue;

						if (tmp[i] == '=')
						{
							/* we should check if we're close to the end of the string */
							rem = count % 4;

							/* rem must be either 2 or 3, otherwise no '=' should be here */
							if (rem < 2)
								return 0;

							/* end-of-message recognized */
							/*@innerbreak@*/ break;
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
						else
							{};

						switch (qw++)
						{
						case 0:
							data[tw+0] = (bits << 2) & 0xfc;
							/*@switchbreak@*/ break;
						case 1:
							data[tw+0] |= (bits >> 4) & 0x03;
							data[tw+1] = (bits << 4) & 0xf0;
							/*@switchbreak@*/ break;
						case 2:
							data[tw+1] |= (bits >> 2) & 0x0f;
							data[tw+2] = (bits << 6) & 0xc0;
							/*@switchbreak@*/ break;
						case 3:
							data[tw+2] |= bits & 0x3f;
							/*@switchbreak@*/ break;
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
#else

const char * b64decode_whitespace = B64DECODE_WHITESPACE;

/*@-internalglobs -modfilesys @*/
int b64decode (const char * s, void ** datap, int *lenp)
{
    unsigned char b64dec[255];
    const unsigned char *t;
    unsigned char *te;
    int ns, nt;
    unsigned a, b, c, d;

    if (s == NULL)	return 1;

    /* Setup character lookup tables. */
    memset(b64dec, 0x80, sizeof(b64dec));
    for (c = 'A'; c <= 'Z'; c++)
	b64dec[ c ] = 0 + (c - 'A');
    for (c = 'a'; c <= 'z'; c++)
	b64dec[ c ] = 26 + (c - 'a');
    for (c = '0'; c <= '9'; c++)
	b64dec[ c ] = 52 + (c - '0');
    b64dec[(unsigned)'+'] = 62;
    b64dec[(unsigned)'/'] = 63;
    b64dec[(unsigned)'='] = 0;

    /* Mark whitespace characters. */
    if (b64decode_whitespace) {
	const char *e;
	for (e = b64decode_whitespace; *e; e++) {
	    if (b64dec[ (unsigned)*e ] == 0x80)
	        b64dec[ (unsigned)*e ] = 0x81;
	}
    }
    
    /* Validate input buffer */
    ns = 0;
    for (t = s; *t; t++) {
	switch (b64dec[ (unsigned)*t ]) {
	case 0x80:	/* invalid chararcter */
	    return 3;
	    /*@notreached@*/ /*@switchbreak@*/ break;
	case 0x81:	/* white space */
	    /*@switchbreak@*/ break;
	default:
	    ns++;
	    /*@switchbreak@*/ break;
	}
    }
    
    if (ns & 0x3)	return 2;

    nt = (ns / 4) * 3;
    t = te = malloc(nt + 1);

    while (ns > 0) {

	/* Get next 4 characters, ignoring whitespace. */
	while ((a = b64dec[ (unsigned)*s++ ]) == 0x81)
	    {};
	while ((b = b64dec[ (unsigned)*s++ ]) == 0x81)
	    {};
	while ((c = b64dec[ (unsigned)*s++ ]) == 0x81)
	    {};
	while ((d = b64dec[ (unsigned)*s++ ]) == 0x81)
	    {};

if (_debug)
fprintf(stderr, "%7u %02x %02x %02x %02x -> %02x %02x %02x\n",
(unsigned)ns, a, b, c, d,
(((a << 2) | (b >> 4)) & 0xff),
(((b << 4) | (c >> 2)) & 0xff),
(((c << 6) | d) & 0xff));

	ns -= 4;
	*te++ = (a << 2) | (b >> 4);
	if (s[-2] == '=') break;
	*te++ = (b << 4) | (c >> 2);
	if (s[-1] == '=') break;
	*te++ = (c << 6) | d;
    }

    if (ns != 0) {		/* XXX can't happen, just in case */
	if (t) free((void *)t);
	return 3;
    }
    if (lenp)
	*lenp = (te - t);

    if (datap)
	*datap = (void *)t;
    else
	if (t) free((void *)t);
    else
	{};

    return 0;
}
/*@=internalglobs =modfilesys @*/
#endif
