/* 
   HTTP URI handling
   Copyright (C) 1999-2000, Joe Orton <joe@orton.demon.co.uk>

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

   Id: uri.c,v 1.7 2000/05/10 16:47:06 joe Exp 
*/

#include <config.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <ctype.h>
#include <stdio.h>

#include "http_utils.h" /* for 'min' */
#include "string_utils.h" /* for CONCAT3 */
#include "uri.h"

char *uri_parent( const char *uri ) {
    const char *pnt;
    char *ret;
    pnt = uri+strlen(uri)-1;
    while( *(--pnt) != '/' && pnt >= uri ) /* noop */;
    if( pnt < uri ) {
	/* not a valid absPath */
	return NULL;
    }
    /*  uri    
     *   V
     *   |---|
     *   /foo/bar/
     */
    ret = xmalloc( (pnt - uri) + 2 );
    memcpy( ret, uri, (pnt - uri) + 1 );
    ret[1+(pnt-uri)] = '\0';
    pnt++;
    return ret;
}

int uri_has_trailing_slash( const char *uri ) 
{
     return (uri[strlen(uri)-1] == '/');
}

const char *uri_abspath( const char *uri ) {
    const char *ret;
    /* Look for the scheme: */
    ret = strstr( uri, "://" );
    if( ret == NULL ) {
	/* No scheme */
	ret = uri;
    } else {
	/* Look for the abs_path */
	ret = strchr( ret+3, '/' );
	if( ret == NULL ) {
	    /* Uh-oh */
	    ret = uri;
	}
    }
    return ret;
}

/* TODO: not a proper URI parser */
int uri_parse( const char *uri, struct uri *parsed, 
	       const struct uri *defaults )
{
    const char *pnt, *slash, *colon;

    parsed->port = -1;
    parsed->host = NULL;
    parsed->path = NULL;
    parsed->scheme = NULL;

    pnt = strstr( uri, "://" );
    if( pnt ) {
	parsed->scheme = xstrndup( uri, pnt - uri );
	pnt += 3; /* start of hostport segment */
	slash = strchr( pnt, '/' );
	colon = strchr( pnt, ':' );
	if( slash == NULL ) {
	    parsed->path = xstrdup( "/" );
	    if( colon == NULL ) {
		if( defaults ) parsed->port = defaults->port;
		parsed->host = xstrdup( pnt );
	    } else {
		parsed->port = atoi(colon+1);
		parsed->host = xstrndup( pnt, colon - pnt );
	    }
	} else {
	    if( colon == NULL || colon > slash ) {
		/* No port segment */
		if( defaults ) parsed->port = defaults->port;
		parsed->host = xstrndup( pnt, slash - pnt );
	    } else {
		/* Port segment */
		parsed->port = atoi( colon + 1 );
		parsed->host = xstrndup( pnt, colon - pnt );
	    }
	    parsed->path = xstrdup( slash );
	}
    } else {
	if( defaults && defaults->scheme != NULL ) {
	    parsed->scheme = xstrdup( defaults->scheme );
	}
	if( defaults && defaults->host != NULL ) {
	    parsed->host = xstrdup( defaults->host );
	}
	if( defaults ) parsed->port = defaults->port;
	parsed->path = xstrdup(uri);
    }
    return 0;
}

void uri_free( struct uri *uri )
{
    HTTP_FREE( uri->host );
    HTTP_FREE( uri->path );
    HTTP_FREE( uri->scheme );
}

/* Returns an absoluteURI */
char *uri_absolute( const char *uri, const char *scheme, 
		    const char *hostport ) {
    char *ret;
    /* Is it absolute already? */
    if( strncmp( uri, scheme, strlen( scheme ) ) == 0 )  {
	/* Yes it is */
	ret = xstrdup( uri );
    } else {
	/* Oh no it isn't */
	CONCAT3( ret, scheme, hostport, uri );
    }
    return ret;
}

/* Un-escapes a URI. Returns xmalloc-allocated URI */
char *uri_unescape( const char *uri ) {
    const char *pnt;
    char *ret, *retpos, buf[5] = { "0x00\0" };
    retpos = ret = xmalloc( strlen( uri ) + 1 );
    for( pnt = uri; *pnt != '\0'; pnt++ ) {
	if( *pnt == '%' ) {
	    if( !isxdigit((unsigned char) pnt[1]) || 
		!isxdigit((unsigned char) pnt[2]) ) {
		/* Invalid URI */
		return NULL;
	    }
	    buf[2] = *++pnt; buf[3] = *++pnt; /* bit faster than memcpy */
	    *retpos++ = strtol( buf, NULL, 16 );
	} else {
	    *retpos++ = *pnt;
	}
    }
    *retpos = '\0';
    return ret;
}

/* RFC2396 spake:
 * "Data must be escaped if it does not have a representation 
 * using an unreserved character".
 * ...where...
 *  unreserved  = alphanum | mark
 *  mark = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
 * 
 * We need also to skip reserved characters
 * reserved    = ";" | "/" | "?" | ":" | "@" | "&" |
 *               "=" | "+" | "$" | ","
 */

/* Lookup table:
 * 1 marks an RESERVED character. 2 marks a UNRESERVED character.
 * 0 marks everything else. 
 */

#define RE 1
#define UN 2 
static const short uri_chars[128] = {
/* 0 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 16 */  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 32 */  0, UN, 0, 0, RE, 0, RE, UN, UN, UN, UN, RE, RE, UN, UN, RE,
/* 48 */ UN, UN, UN, UN, UN, UN, UN, UN, UN, UN, RE, RE, 0, RE, 0, RE,
/* 64 */ RE, UN, UN, UN, UN, UN, UN, UN, UN, UN, UN, UN, UN, UN, UN, UN,
/* 80 */ UN, UN, UN, UN, UN, UN, UN, UN, UN, UN, UN, 0, 0, 0, 0, UN,
/* 96 */ 0, UN, UN, UN, UN, UN, UN, UN, UN, UN, UN, UN, UN, UN, UN, UN,
/* 112 */ UN, UN, UN, UN, UN, UN, UN, UN, UN, UN, UN, 0, 0, 0, UN, 0 
};
#undef RE
#undef UN

/* Escapes the abspath segment of a URI.
 * Returns xmalloc-allocated string.
 */
char *uri_abspath_escape( const char *abs_path ) {
    const char *pnt;
    char *ret, *retpos;
    /* Rather than mess about growing the buffer, allocate as much as
     * the URI could possibly need, i.e. every character gets %XX
     * escaped. Hence 3 times input size. 
     */
    retpos = ret = xmalloc( strlen( abs_path ) * 3 + 1 );
    for( pnt = abs_path; *pnt != '\0'; pnt++ ) {
	/* Escape it:
	 *  - if it isn't 7-bit
	 *  - if it is a reserved character (but ignore '/')
	 *  - otherwise, if it is not an unreserved character
	 * (note, there are many characters that are neither reserved
	 * nor unreserved)
	 */
	if( *pnt<0 || (uri_chars[(int) *pnt] < 2 && *pnt!='/' )) {
	    /* Escape it - %<hex><hex> */
	    /* FIXME: Could overflow buffer with uri_abspath_escape("%") */
	    sprintf( retpos, "%%%02x", (unsigned char) *pnt );
	    retpos += 3;
	} else {
	    /* It's cool */
	    *retpos++ = *pnt;
	}
    }
    *retpos = '\0';
    return ret;
}

/* TODO: implement properly */
int uri_compare( const char *a, const char *b ) {
    int ret = strcasecmp( a, b );
    if( ret ) {
	/* TODO: joe: I'm not 100% sure this is logically sound.
	 * It feels right, though */
	int traila = uri_has_trailing_slash( a ),
	    trailb = uri_has_trailing_slash( b ),
	    lena = strlen(a), lenb = strlen(b);
	if( traila != trailb && abs(lena - lenb) == 1) {
	    /* They are the same length, apart from one has a trailing
	     * slash and the other doesn't. */
	    if( strncasecmp( a, b, min(lena, lenb) ) == 0 )
		ret = 0;
	}
    }
    return ret;
}

/* Hrm, well, this is kind of not very generic over URI schemes, but wth */
int uri_childof( const char *parent, const char *child ) 
{
    char *root = xstrdup(child);
    int ret;
    if( strlen(parent) >= strlen(child) ) {
	ret = 0;
    } else {
	/* root is the first of child, equal to length of parent */
	root[strlen(parent)] = '\0';
	ret = (uri_compare( parent, root ) == 0 );
    }
    free( root );
    return ret;
}

#ifdef URITEST

int main( int argc, char *argv[] ) {
    char *tmp;
    if( argc<2 || argc>3 ) {
	printf( "doh. usage:\nuritest uria [urib]\n"
		"e.g. uritest \"/this/is/a silly<filename>/but/hey\"\n" );
	exit(-1);
    }
    if( argv[2] ) {
	printf( "uri_compare: %s with %s: %s\n",
		argv[1], argv[2],
		uri_compare(argv[1], argv[2])==0?"true":"false" );
    } else {
	printf( "Input URI: %s\n", argv[1] );
	tmp = uri_abspath_escape( argv[1] );
	printf( "Encoded: %s\n", tmp );
	printf( "Decoded: %s\n", uri_unescape( tmp ) );
    }
    return 0;
}

#endif /* URITEST */
