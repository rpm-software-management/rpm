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

*/

#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include <config.h>

#include <sys/types.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#include <stdio.h>

#ifdef HAVE_DEBUG_FUNC
#include "common.h"
#endif

#define HTTP_QUOTES "\"'"
#define HTTP_WHITESPACE " \r\n\t"

/* Handy macro to free things. */
#define HTTP_FREE(x) \
do { if( (x)!=NULL ) free( (x) ); (x) = NULL; } while(0)

#define HTTP_PORT 80

time_t http_dateparse( const char *date );

#undef min
#define min(a,b) ((a)<(b)?(a):(b))

#ifndef HAVE_DEBUG_FUNC
/* CONSIDER: mutt has a nicer way of way of doing debugging output... maybe
 * switch to like that. */
#ifdef DEBUGGING
#define DEBUG if(0) http_debug
#else /* !DEBUGGING */
#define DEBUG http_debug
#endif /* DEBUGGING */

#define DEBUG_SOCKET (1<<0)
#define DEBUG_HTTP (1<<3)
#define DEBUG_XML (1<<5)
#define DEBUG_HTTPAUTH (1<<7)
#define DEBUG_HTTPPLAIN (1<<8)
#define DEBUG_LOCKS (1<<10)
#define DEBUG_XMLPARSE (1<<11)
#define DEBUG_HTTPBODY (1<<12)
#define DEBUG_HTTPBASIC (1<<13)
#define DEBUG_FLUSH (1<<22)

extern FILE *http_debug_stream;
extern int http_debug_mask;

void http_debug( int ch, char *, ... )
#ifdef __GNUC__
                __attribute__ ((format (printf, 2, 3)))
#endif /* __GNUC__ */
;

#endif /* HAVE_DEBUG_FUNC */

/* Storing an HTTP status result */
typedef struct {
    int major_version;
    int minor_version;
    int code; /* Status-Code value */
    int class; /* Class of Status-Code (1-5) */
    const char *reason_phrase;
} http_status;

/* Parser for strings which follow the Status-Line grammar from 
 * RFC2616.
 *  Returns:
 *    0 on success, *s will be filled in.
 *   -1 on parse error.
 */
int http_parse_statusline( const char *status_line, http_status *s );

#endif /* HTTP_UTILS_H */
