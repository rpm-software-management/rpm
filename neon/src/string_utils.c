/* 
   String utility functions
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

   Id: string_utils.c,v 1.8 2000/05/09 18:32:07 joe Exp 
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "xalloc.h"

#include "string_utils.h"

struct sbuffer_s {
    char *data;
    size_t used; /* used bytes in buffer */
    size_t length; /* length of buffer */
};

/* TODO: These are both crap. Rewrite to be like strsep(). */

char **split_string( const char *str, const char separator,
		     const char *quotes, const char *whitespace ) {
    return split_string_c( str, separator, quotes, whitespace, NULL );
}

char **split_string_c( const char *str, const char separator,
		       const char *quotes, const char *whitespace,
		       int *give_count ) {
    char **comps;
    const char *pnt, *quot = NULL,
	*start, *end; /* The start of the current component */
    int count, /* The number of components */
	iswhite, /* is it whitespace */
	issep, /* is it the separator */
	curr, /* current component index */
	length, /* length of component */
	leading_wspace; /* in leading whitespace still? */

    /* Inefficient, but easier - first off, count the number of 
     * components we have. */
    count = 1;
    for( pnt = str; *pnt!='\0'; pnt++ ) {
	if( quotes != NULL ) {
	    quot = strchr( quotes, *pnt );
	}
	if( quot != NULL ) {
	    /* We found a quote, so skip till the next quote */
	    for( pnt++; (*pnt!=*quot) && (*pnt!='\0'); pnt++ )
		/* nullop */;
	} else if( *pnt == separator ) {
	    count++;
	}
    }

    if( give_count ) {
	/* Write the count */
	*give_count = count;
    }

    /* Now, have got the number of components.
     * Allocate the comps array. +1 for the NULL */
    comps = xmalloc( sizeof(char *) * (count + 1) );

    comps[count] = NULL;
    
    quot = end = start = NULL;
    curr = 0;
    leading_wspace = 1;

    /* Now fill in the array */
    for( pnt = str; *pnt != '\0'; pnt++ ) {
	/* What is the current character - quote, whitespace, separator? */
	if( quotes != NULL ) {
	    quot = strchr( quotes, *pnt );
	}
	iswhite = (whitespace!=NULL) && 
	    (strchr( whitespace, *pnt ) != NULL );
	issep = (*pnt == separator);
	/* What to do? */
	if( leading_wspace ) {
	    if( quot!=NULL ) {
		/* Quoted bit */
		start = pnt;
		length = 1;
		leading_wspace = 0;
	    } else if( issep ) {
		/* Zero-length component */
		comps[curr++] = xstrdup("");
	    } else if( !iswhite ) {
		start = end = pnt;
		length = 1;
		leading_wspace = 0;
	    }
	} else {
	    if( quot!=NULL ) {
		/* Quoted bit */
		length++;
	    } else if( issep ) {
		/* End of component - enter it into the array */
		length = (end - start) + 1;
		comps[curr] = xmalloc( length+1 );
		memcpy( comps[curr], start, length );
		comps[curr][length] = '\0';
		curr++;
		leading_wspace = 1;
	    } else if( !iswhite ) {
		/* Not whitespace - update end marker */
		end = pnt;
	    }
	}
	if( quot != NULL ) {
	    /* Skip to closing quote */
	    for( pnt++; *pnt!=*quot && *pnt != '\0'; ++pnt )
		/* nullop */;
	    /* Last non-wspace char is closing quote */
	    end = pnt;
	}
    }
    /* Handle final component */
    if( leading_wspace ) {
	comps[curr] = xstrdup( "" );
    } else {
	/* End of component - enter it into the array */
	length = (end - start) + 1;
	comps[curr] = xmalloc( length+1 );
	memcpy( comps[curr], start, length );
	comps[curr][length] = '\0';
    }
    return comps;
}

char **pair_string( const char *str, const char compsep, const char kvsep, 
		 const char *quotes, const char *whitespace ) 
{
    char **comps, **pairs, *split;
    int count = 0, n, length;
    comps = split_string_c( str, compsep, quotes, whitespace, &count );
    /* Allocate space for 2* as many components as split_string returned,
     * +2 for the NULLS. */
    pairs = xmalloc( (2*count+2) * sizeof(char *) );
    if( pairs == NULL ) {
	return NULL;
    }
    for( n = 0; n < count; n++ ) {
	/* Find the split */
	split = strchr( comps[n], kvsep );
	if( split == NULL ) {
	    /* No seperator found */
	    length = strlen(comps[n]);
	} else {
	    length = split-comps[n];
	}
	/* Enter the key into the array */
	pairs[2*n] = comps[n];
	/* Null-terminate the key */
	pairs[2*n][length] = '\0';
	pairs[2*n+1] = split?(split + 1):NULL;
    }
    free( comps );
    pairs[2*count] = pairs[2*count+1] = NULL;    
    return pairs;
}

void split_string_free( char **components ) 
{
    char **pnt = components;
    while( *pnt != NULL ) {
	free( *pnt );
	pnt++;
    }
    free( components );
}

void pair_string_free( char **pairs ) 
{
    int n;
    for( n = 0; pairs[n] != NULL; n+=2 ) {
	free( pairs[n] );
    }
    free( pairs );
}

char *shave_string( const char *str, const char ch ) {
    size_t len = strlen( str );
    char *ret;
    if( str[len-1] == ch ) {
	len--;
    }
    if( str[0] == ch ) {
	len--;
	str++;
    }
    ret = xmalloc( len + 1 );
    memcpy( ret, str, len );
    ret[len] = '\0';
    return ret;
}

char *sbuffer_data( sbuffer buf ) {
    return buf->data;
}

int sbuffer_size( sbuffer buf ) {
    return buf->used;
}

void sbuffer_clear( sbuffer buf ) {
    memset( buf->data, 0, buf->length );
    buf->used = 0;
}  

/* Grows for given size, 0 on success, -1 on error.
 * Could make this externally accessible, but, there hasn't been
 * a need currently. */
int sbuffer_grow( sbuffer buf, size_t newsize ) {
    size_t newlen, oldbuflen;

#define SBUFFER_GROWTH 512

    if( newsize <= buf->length ) return 0; /* big enough already */
    /* FIXME: ah, can't remember my maths... better way to do this? */
    newlen = ( (newsize / SBUFFER_GROWTH) + 1) * SBUFFER_GROWTH;
    
    oldbuflen = buf->length;
    /* Reallocate bigger buffer */
    buf->data = realloc( buf->data, newlen );
    if( buf->data == NULL ) return -1;
    buf->length = newlen;
    /* Zero-out the new bit of buffer */
    memset( buf->data+oldbuflen, 0, newlen-oldbuflen );

    return 0;
}

int sbuffer_concat( sbuffer buf, ... ) {
    va_list ap;
    char *next;
    size_t totallen = 1; /* initialized to 1 for the terminating \0 */    

    /* Find out how much space we need for all the args */
    va_start( ap, buf );
    do {
	next = va_arg( ap, char * );
	if( next != NULL ) {
	    totallen += strlen( next );
	}
    } while( next != NULL );
    va_end( ap );
    
    /* Grow the buffer */
    if( sbuffer_grow( buf, buf->used + totallen ) )
	return -1;
    
    /* Now append the arguments to the buffer */
    va_start( ap, buf );
    do {
	next = va_arg( ap, char * );
	if( next != NULL ) {
	    strcat( buf->data, next );
	}
    } while( next != NULL );
    va_end( ap );
    
    buf->used += totallen;
    return 0;
}

/* Append zero-terminated string... returns 0 on success or -1 on
 * realloc failure. */
int sbuffer_zappend( sbuffer buf, const char *str ) {
    size_t len = strlen( str );
    if( sbuffer_grow( buf, buf->used + len + 1 ) ) {
	return -1;
    }
    strcat( buf->data, str );
    buf->used += len;
    return 0;
}

int sbuffer_append( sbuffer buf, const char *data, size_t len ) {
    if( sbuffer_grow( buf, buf->used + len ) ) {
	return -1;
    }
    memcpy( buf->data+buf->used, data, len );
    buf->used += len;
    return 0;
}

sbuffer sbuffer_create( void ) {
    return sbuffer_create_sized( 512 );
}

sbuffer sbuffer_create_sized( size_t s ) {
    sbuffer buf = xmalloc( sizeof(struct sbuffer_s) );
    buf->data = xmalloc( s );
    memset( buf->data, 0, s );
    buf->length = s;
    buf->used = 0;
    return buf;
}

void sbuffer_destroy( sbuffer buf ) {
    if( buf->data ) {
	free( buf->data );
    }
    free( buf );
}

char *sbuffer_finish( sbuffer buf )
{
    char *ret = buf->data;
    free( buf );
    return ret;
}

void sbuffer_altered( sbuffer buf )
{
    buf->used = strlen( buf->data );
}

/* Writes the ASCII representation of the MD5 digest into the
 * given buffer, which must be at least 33 characters long. */
void md5_to_ascii( const unsigned char md5_buf[16], char *buffer ) 
{
    int count;
    for( count = 0; count<16; count++ ) {
	buffer[count*2] = HEX2ASC( md5_buf[count] >> 4 );
	buffer[count*2+1] = HEX2ASC( md5_buf[count] & 0x0f );
    }
    buffer[32] = '\0';
}

/* Reads the ASCII representation of an MD5 digest. The buffer must
 * be at least 32 characters long. */
void ascii_to_md5( const char *buffer, unsigned char md5_buf[16] ) 
{
    int count;
    for( count = 0; count<16; count++ ) {
	md5_buf[count] = ((ASC2HEX(buffer[count*2])) << 4) |
	    ASC2HEX(buffer[count*2+1]);
    }
}

#ifdef SPLIT_STRING_TEST

#include <stdio.h>

int main( int argc, char *argv[] ) {
    char *str, sep, **comps, *wspace, *quotes;
    int count;
    if( argc < 3 ) {
	printf( "Usage: split_string <sep> <string> [whitespace] [quotes]\n" );
	return -1;
    }
    sep = *argv[1];
    str = argv[2];
    if( argc > 3 ) {
        wspace = argv[3];
    } else {
	wspace = " ";
    }
    if( argc > 4 ) {
	quotes = argv[4];
    } else {
	quotes = "\"";
    }
    printf( "String: [%s]  Separator: `%c'  Whitespace: [%s]  Quotes: [%s]\n", str, sep, wspace, quotes );
    comps = split_string( str, sep, quotes, wspace );
    count = 0;
    do {
	printf( "Component #%d: [%s]\n", count, comps[count] );
    } while( comps[++count] != NULL );
    return 0;
}

#endif

#ifdef PAIR_STRING_TEST

#include <stdio.h>

int main( int argc, char *argv[] ) {
    char *str, compsep, kvsep, **comps, *wspace, *quotes;
    int count;
    if( argc < 4 ) {
	printf( "Usage: pair_string <compsep> <kvsep> <string> [whitespace] [quotes]\n" );
	return -1;
    }
    compsep = *argv[1];
    kvsep = *argv[2];
    str = argv[3];
    if( argc > 4 ) {
        wspace = argv[4];
    } else {
	wspace = " ";
    }
    if( argc > 5 ) {
	quotes = argv[5];
    } else {
	quotes = "\"";
    }
    printf( "String: [%s]  CompSep: `%c' K/VSep: `%c'\nWhitespace: [%s]  Quotes: [%s]\n", str, compsep, kvsep, wspace, quotes );
    comps = pair_string( str, compsep, kvsep, quotes, wspace );
    count = 0;
    do {
	printf( "Component #%d: Key [%s] Value [%s]\n", count, 
		comps[count], comps[count+1] );
    } while( comps[(count+=2)] != NULL );
    return 0;
}

#endif

/* variables:
 *
 * Local variables:
 *  compile-command: "gcc -g -O2 -Wall -I.. -ansi -DHAVE_CONFIG_H -DSPLIT_STRING_TEST -o split_string string_utils.c"
 * End:
 */
