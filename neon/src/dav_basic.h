/* 
   Basic WebDAV support
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

#ifndef DAV_BASIC_H
#define DAV_BASIC_H

#include "http_request.h"

#include "dav_207.h"

#define DAV_DEPTH_ZERO (0)
#define DAV_DEPTH_ONE (1)
#define DAV_DEPTH_INFINITE (2)

/* Adds a Depth: header to a request */
void dav_add_depth_header( http_req *req, int depth );

/* Handle a simple WebDAV request.
 *
 * Usage:
 *  1. Create the request using http_request_create()
 *  2. Set any headers, the request body, whatever.
 *  3. Call dav_simple_request to dispatch and destroy the request.
 *
 * (note the request IS destroyed by this function, don't do it 
 * yourself).
 *
 * Returns HTTP_* as http_request_dispatch() would. If the response is
 * a 207, a user-friendly error message is written to the session
 * error buffer; e.g.  DELETE /foo/ might give the error:
 *     /foo/bar: HTTP/1.1 423 Locked
 */
int dav_simple_request( http_session *sess, http_req *req );

/* Basic WebDAV methods:
 *   dav_copy:  copy resoure from src to dest
 *   dav_move:  move resource from src to dest
 *   dav_delete: delete resource at uri
 *   dav_mkcol: create a collection at uri (uri MUST have a trailing slash).
 */
int dav_copy( http_session *sess, const char *src, const char *dest );
int dav_move( http_session *sess, const char *src, const char *dest );
int dav_delete( http_session *sess, const char *uri );
int dav_mkcol( http_session *sess, const char *uri );

#endif
