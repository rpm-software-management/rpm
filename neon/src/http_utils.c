/* 
   HTTP utility functions
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

   Id: http_utils.c,v 1.4 2000/05/10 13:26:07 joe Exp 
*/

#include <config.h>

#include <sys/types.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <ctype.h> /* isdigit() for http_parse_statusline */

#include "dates.h"

#include "http_utils.h"

int http_debug_mask;
FILE *http_debug_stream;

void http_debug( int ch, char *template, ...) 
{
#ifdef DEBUGGING
    va_list params;
    if( (ch&http_debug_mask) != ch ) return;
    fflush( stdout );
    va_start( params, template );
    vfprintf( http_debug_stream, template, params );
    va_end( params );
    if( (ch&DEBUG_FLUSH) == DEBUG_FLUSH ) {
	fflush( http_debug_stream );
    }
#else
    /* No debugging here */
#endif
}

/* HTTP-date parser */
time_t http_dateparse( const char *date ) {
    time_t tmp;
    tmp = rfc1123_parse( date );
    if( tmp == -1 ) {
        tmp = rfc1036_parse( date );
	if( tmp == -1 )
	    tmp = asctime_parse( date );
    }
    return tmp;
}

int http_parse_statusline( const char *status_line, http_status *st ) {
    const char *part;
    int major, minor, status_code, class;
    /* Check they're speaking the right language */
    if( strncmp( status_line, "HTTP/", 5 ) != 0 ) {
	return -1;
    }
    /* And find out which dialect of this peculiar language
     * they can talk... */
    major = 0;
    minor = 0; 
    /* Note, we're good children, and accept leading zero's on the
     * version numbers */
    for( part = status_line + 5; *part != '\0' && isdigit(*part); part++ ) {
	major = major*10 + (*part-'0');
    }
    if( *part != '.' ) { 
	return -1;
    }
    for( part++ ; *part != '\0' && isdigit(*part); part++ ) {
	minor = minor*10 + (*part-'0');
    }
    if( *part != ' ' ) {
	return -1;
    }
    /* Skip any spaces */
    for( ; *part == ' ' ; part++ ) /* noop */;
    /* Now for the Status-Code. part now points at the first Y in
     * "HTTP/x.x YYY". We want value of YYY... could use atoi, but
     * probably quicker this way. */
    if( !isdigit(part[0]) || !isdigit(part[1]) || !isdigit(part[2]) ) {
	return -1;
    }
    status_code = 100*(part[0]-'0') + 10*(part[1]-'0') + (part[2]-'0');
    class = part[0]-'0';
    /* Skip whitespace between status-code and reason-phrase */
    for( part+=3; *part == ' ' || *part == '\t'; part++ ) /* noop */;
    if( *part == '\0' ) {
	return -1;
    }
    /* Fill in the results */
    st->major_version = major;
    st->minor_version = minor;
    st->reason_phrase = part;
    st->code = status_code;
    st->class = class;
    return 0;
}
