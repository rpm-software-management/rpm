/* 
   HTTP-redirect support
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

#ifndef NE_REDIRECT_H
#define NE_REDIRECT_H

#include "ne_request.h"

BEGIN_NEON_DECLS

/* The redirect code does not handle redirects to a different server.
 * For a redirect to a different server, the request fails and returns
 * HTTP_REDIRECT.  You can then retrieve the redirect location using the
 * call http_redirect_location.
 *
 * (you must have called http_redirect_register for this to happen).
 * */

/* Get confirmation from the user that a redirect from
 * URI 'src' to URI 'dest' is acceptable. Should return:
 *   Non-Zero to FOLLOW the redirect
 *   Zero to NOT follow the redirect
 */
typedef int (*ne_redirect_confirm)(void *userdata,
				   const char *src, const char *dest);

/* Notify the user that a redirect has been automatically 
 * followed from URI 'src' to URI 'dest' */
typedef void (*ne_redirect_notify)(void *userdata,
				   const char *src, const char *dest);

/* Register redirect handling for the given session.
 * Some redirect responses will be automatically followed.
 * If the redirect is automatically followed, the 'notify' callback
 * is called.
 * For redirects which are NOT automatically followed, the
 * 'confirm' callback is called: if this returns zero, the redirect
 * is ignored.
 * 
 * 'confirm' may be passed as NULL: in this case, only automatic
 * redirects are followed.  'notify' may also be passed as NULL,
 * automatic redirects are still followed.
 *
 * 'userdata' is passed as the first argument to the confirm and
 * notify callbacks.  */
void ne_redirect_register(ne_session *sess,
			  ne_redirect_confirm confirm,
			  ne_redirect_notify notify,
			  void *userdata);

/* Return location of last redirect. */
const char *ne_redirect_location(ne_session *sess);

END_NEON_DECLS

#endif /* NE_REDIRECT_H */
