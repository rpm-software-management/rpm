/* 
   HTTP/1.1 methods
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

#ifndef HTTP_BASIC_H
#define HTTP_BASIC_H

#include "config.h"

#include <sys/types.h> /* for time_t */

#include <stdio.h> /* for FILE * */

/* PUT resource at uri, reading request body from f */
int http_put( http_session *sess, const char *uri, FILE *f );

/* PUT resource at uri as above, only if it has not been modified
 * since given modtime. If server is HTTP/1.1, uses If-Unmodified-Since
 * header; guaranteed failure if resource is modified after 'modtime'.
 * If server is HTTP/1.0, HEAD's the resource first to fetch current
 * modtime; race condition if resource is modified between HEAD and PUT.
 */
int http_put_if_unmodified( http_session *sess,
			    const char *uri, FILE *stream, time_t modtime );

/* GET resource at uri, writing response body into f */
int http_get( http_session *sess, const char *uri, FILE *f );

/* GET resource at uri, passing response body blocks to 'reader' */
int http_read_file( http_session *sess, const char *uri, 
		    http_block_reader reader, void *userdata );

/* Retrieve modification time of resource at uri, place in *modtime.
 * (uses HEAD) */
int http_getmodtime( http_session *sess, const char *uri, time_t *modtime );

typedef struct {
    const char *type, *subtype;
    const char *charset;
    char *value;
} http_content_type;   

/* Sets (*http_content_type)userdata appropriately. 
 * Caller must free ->value after use */
void http_content_type_handler( void *userdata, const char *value );

/* Server capabilities: */
typedef struct {
    unsigned int broken_expect100:1; /* True if the server is known to
				      * have broken Expect:
				      * 100-continue support; Apache
				      * 1.3.6 and earlier. */

    unsigned int dav_class1; /* True if Class 1 WebDAV server */
    unsigned int dav_class2; /* True if Class 2 WebDAV server */
    unsigned int dav_executable; /* True if supports the 'executable'
				  * property a. la. mod_dav */
} http_server_capabilities;

/* Determines server capabilities (using OPTIONS). 
 * Pass uri="*" to determine proxy server capabilities if using
 * a proxy server. */
int http_options( http_session *sess, const char *uri, 
		  http_server_capabilities *caps );

#if 0 /* TODO: unimplemented */

typedef http_content_range {
    long start, end, total;
} http_content_range;

/* This will write to the CURRENT position of f; so if you want
 * to do a resume download, use:
 *      struct http_content_range range;
 *      range.start = resume_from; 
 *      range.end = range.total = 1000;
 *      fseek( myfile, resume_from, SEEK_SET );
 *      http_get_range( sess, uri, &range, myfile );
 */
int http_get_range( http_session *sess, const char *uri, 
		    http_content_range *range, FILE *f );

#endif


#endif
