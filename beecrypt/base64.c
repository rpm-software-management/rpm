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
		for (e = b64encode_eolstr; *e != '\0'; e++)
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
		for (e = b64encode_eolstr; *e != '\0'; e++)
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

const char * b64decode_whitespace = B64DECODE_WHITESPACE;

/*@-internalglobs -modfilesys @*/
int b64decode (const char * s, void ** datap, int *lenp)
{
    unsigned char b64dec[256];
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
	for (e = b64decode_whitespace; *e != '\0'; e++) {
	    if (b64dec[ (unsigned)*e ] == 0x80)
	        b64dec[ (unsigned)*e ] = 0x81;
	}
    }
    
    /* Validate input buffer */
    ns = 0;
    for (t = s; *t != '\0'; t++) {
	switch (b64dec[ (unsigned)*t ]) {
	case 0x80:	/* invalid chararcter */
if (_debug)
fprintf(stderr, "--- b64decode %c(%02x) %02x\n", *t, (unsigned)(*t & 0xff), (unsigned)b64dec[ (unsigned)*t ]);
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
	return 1;
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
