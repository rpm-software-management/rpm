/* 
   HTTP authentication routines
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

#ifndef NE_AUTH_H
#define NE_AUTH_H

#include "ne_session.h" /* for http_session */

BEGIN_NEON_DECLS

/* The callback used to request the username and password in the given
 * realm. The username and password must be placed in malloc()-allocated
 * memory.
 * Must return:
 *   0 on success: *username and *password must be non-NULL, and will
 *                 be free'd by the HTTP layer when necessary
 *  -1 to cancel (*username and *password are ignored.)
 */
typedef int (*ne_request_auth)(
    void *userdata, const char *realm,
    char **username, char **password);

/* Set callbacks to handle server and proxy authentication.
 * userdata is passed as the first argument to the callback. */
void ne_set_server_auth(ne_session *sess, ne_request_auth callback, 
			void *userdata);
void ne_set_proxy_auth(ne_session *sess, ne_request_auth callback, 
		       void *userdata);

/* Clear any stored authentication details for the given session. */
void ne_forget_auth(ne_session *sess);

END_NEON_DECLS

#endif /* NE_AUTH_H */
