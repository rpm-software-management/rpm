/* 
   Text->Base64 convertion, from RFC1521
   Copyright (C) 1998,1999,2000 Joe Orton <joe@orton.demon.co.uk>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

   Id: base64.c,v 1.3 2000/05/09 18:26:58 joe Exp 
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "xalloc.h"
#include "base64.h"

/* Base64 specification from RFC 1521 */

static const char b64_alphabet[65] = { 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/=" };
    
char *base64( const char *text ) 
{
    /* The tricky thing about this is doing the padding at the end,
     * doing the bit manipulation requires a bit of concentration only */
    char *buffer, *point;
    int inlen, outlen;
    
    /* Use 'buffer' to store the output. Work out how big it should be...
     * This must be a multiple of 4 bytes */

    inlen = strlen( text );
    outlen = (inlen*4)/3;
    if( (inlen % 3) > 0 ) /* got to pad */
	outlen += 4 - (inlen % 3);
    
    buffer = xmalloc( outlen + 1 ); /* +1 for the \0 */
    
    /* now do the main stage of conversion, 3 bytes at a time,
     * leave the trailing bytes (if there are any) for later */

    for( point=buffer; inlen>=3; inlen-=3, text+=3 ) {
	*(point++) = b64_alphabet[ (*text)>>2 ]; 
	*(point++) = b64_alphabet[ ((*text)<<4 & 0x30) | (*(text+1))>>4 ]; 
	*(point++) = b64_alphabet[ ((*(text+1))<<2 & 0x3c) | (*(text+2))>>6 ];
	*(point++) = b64_alphabet[ (*(text+2)) & 0x3f ];
    }

    /* Now deal with the trailing bytes */
    if( inlen ) {
	/* We always have one trailing byte */
	*(point++) = b64_alphabet[ (*text)>>2 ];
	*(point++) = b64_alphabet[ ( ((*text)<<4 & 0x30) |
				     (inlen==2?(*(text+1))>>4:0) ) ]; 
	*(point++) = (inlen==1?'=':b64_alphabet[ (*(text+1))<<2 & 0x3c ] );
	*(point++) = '=';
    }

    /* Null-terminate */
    *point = '\0';

    return buffer;
}

#ifdef BASE64_TEST

/* Standalone tester */

#include <stdio.h>

/* Tester for Base64 algorithm */
int main( int argc, char **argv ) {
    char *out;
    if( argc != 2 ) {
	printf( "Usage: %s plaintext\n", argv[0] );
	exit(-1);
    }
    out = base64( argv[1] );
    printf( "Plain: [%s], Base64: [%s]\n", argv[1], out );
    return 0;
}

#endif /* BASE64_TEST */

