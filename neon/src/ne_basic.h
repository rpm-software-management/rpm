/* 
   HTTP/1.1 methods
   Copyright (C) 1999-2001, Joe Orton <joe@light.plus.com>

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

#ifndef NE_BASIC_H
#define NE_BASIC_H

#include <sys/types.h> /* for time_t */

#include "ne_request.h"

BEGIN_NEON_DECLS

/* PUT resource at uri, reading request body from f */
int ne_put(ne_session *sess, const char *uri, int fd);

/* GET resource at uri, writing response body into f */
int ne_get(ne_session *sess, const char *uri, int fd);

#ifndef NEON_NODAV

/* Basic WebDAV methods:
 *   ne_copy:  copy resoure from src to dest
 *   ne_move:  move resource from src to dest
 *     -> if overwrite is non-zero, the destination resource
 *	will be overwritten if it exists.
 *   ne_delete: delete resource at uri
 *   ne_mkcol: create a collection at uri (uri MUST have a trailing slash).
 */
int ne_copy(ne_session *sess, int overwrite, const char *src, const char *dest);
int ne_move(ne_session *sess, int overwrite, const char *src, const char *dest);
int ne_delete(ne_session *sess, const char *uri);
int ne_mkcol(ne_session *sess, const char *uri);

#define NE_DEPTH_ZERO (0)
#define NE_DEPTH_ONE (1)
#define NE_DEPTH_INFINITE (2)

/* Adds a Depth: header to a request */
void ne_add_depth_header(ne_request *req, int depth);

#endif /* NEON_NODAV */

/* PUT resource at uri as above, only if it has not been modified
 * since given modtime. If server is HTTP/1.1, uses If-Unmodified-Since
 * header; guaranteed failure if resource is modified after 'modtime'.
 * If server is HTTP/1.0, HEAD's the resource first to fetch current
 * modtime; race condition if resource is modified between HEAD and PUT.
 */
int ne_put_if_unmodified(ne_session *sess,
			 const char *uri, int fd, time_t modtime);

/* GET resource at uri, passing response body blocks to 'reader' */
int ne_read_file(ne_session *sess, const char *uri, 
		 ne_block_reader reader, void *userdata);

/* Retrieve modification time of resource at uri, place in *modtime.
 * (uses HEAD) */
int ne_getmodtime(ne_session *sess, const char *uri, time_t *modtime);

typedef struct {
    const char *type, *subtype;
    const char *charset;
    char *value;
} ne_content_type;   

/* Sets (*ne_content_type)userdata appropriately. 
 * Caller must free ->value after use */
void ne_content_type_handler(void *userdata, const char *value);

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
} ne_server_capabilities;

/* Determines server capabilities (using OPTIONS). 
 * Pass uri="*" to determine proxy server capabilities if using
 * a proxy server. */
int ne_options(ne_session *sess, const char *uri, 
	       ne_server_capabilities *caps);

/* Defines a range of bytes, starting at 'start' and ending
 * at 'end'.  'total' is the number of bytes in the range.
 */
typedef struct {
    off_t start, end, total;
} ne_content_range;

/* Partial GET. range->start must be >= 0. range->total is ignored.
 *
 * If range->end is -1, then the rest of the resource from start is
 * requested, and range->total and end are filled in on success.
 *
 * Otherwise, bytes from range->start to range->end are requested.
 *
 * This will write to the CURRENT position of f; so if you want
 * to do a resume download, use:
 *      struct ne_content_range range;
 *      range.start = resume_from; 
 *      range.end = range.start + 999;  (= 1000 bytes)
 *      fseek(myfile, resume_from, SEEK_SET);
 *      ne_get_range(sess, uri, &range, myfile); */
int ne_get_range(ne_session *sess, const char *uri, 
		 ne_content_range *range, int fd);

/* Post using buffer as request-body: stream response into f */
int ne_post(ne_session *sess, const char *uri, int fd, const char *buffer);

END_NEON_DECLS

#endif /* NE_BASIC_H */
