/* 
   Basic cookie support for neon
   Copyright (C) 1999-2003, Joe Orton <joe@manyfish.co.uk>

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

/* A nice demo of hooks, since it doesn't need any external
 * interface to muck with the stored states.
 */

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <time.h>

#include "ne_cookies.h"

#include "ne_request.h"
#include "ne_string.h"
#include "ne_alloc.h"

#define COOKIE_ID "http://www.webdav.org/neon/hooks/cookies"

static void set_cookie_hdl(void *userdata, const char *value)
{
    char **pairs = pair_string(value, ';', '=', "\"'", " \r\n\t");
    ne_cookie *cook;
    ne_cookie_cache *cache = userdata;
    int n;

    /* Check sanity */
    if (pairs[0] == NULL || pairs[1] == NULL) {
	/* yagaboo */
	return;
    }

    NE_DEBUG(NE_DBG_HTTP, "Got cookie name=%s\n", pairs[0]);

    /* Search for a cookie of this name */
    NE_DEBUG(NE_DBG_HTTP, "Searching for existing cookie...\n");
    for (cook = cache->cookies; cook != NULL; cook = cook->next) {
	if (strcasecmp(cook->name, pairs[0]) == 0) {
	    break;
	}
    }
    
    if (cook == NULL) {
	NE_DEBUG(NE_DBG_HTTP, "New cookie.\n");
	cook = ne_malloc(sizeof *cook);
	memset(cook, 0, sizeof *cook);
	cook->name = ne_strdup(ne_shave(pairs[0], " \t\r\n"));
	cook->next = cache->cookies;
	cache->cookies = cook;
    } else {
	/* Free the old value */
	ne_free(cook->value);
    }

    cook->value = ne_strdup(ne_shave(pairs[1], " \t\r\n"));

    for (n = 2; pairs[n] != NULL; n+=2) {
        if (!pairs[n+1]) continue;
	NE_DEBUG(NE_DBG_HTTP, "Cookie parm %s=%s\n", pairs[n], pairs[n+1]);
	if (strcasecmp(pairs[n], "path") == 0) {
	    cook->path = ne_strdup(pairs[n+1]);
	} else if (strcasecmp(pairs[n], "max-age") == 0) {
	    int t = atoi(pairs[n+1]);
	    cook->expiry = time(NULL) + (time_t)t;
	} else if (strcasecmp(pairs[n], "domain") == 0) {
	    cook->domain = ne_strdup(pairs[n+1]);
	}
    }

    NE_DEBUG(NE_DBG_HTTP, "End of parms.\n");

    pair_string_free(pairs);
}

static void create(ne_request *req, void *session,
		   const char *method, const char *uri)
{
    ne_cookie_cache *cache = session;
    ne_add_response_header_handler(req, "Set-Cookie", set_cookie_hdl, cache);
}

/* Just before sending the request: let them add headers if they want */
static void pre_send(ne_request *req, void *session, ne_buffer *request)
{
    ne_cookie_cache *cache = session;
    ne_cookie *cook;
    
    if (cache->cookies == NULL) {
	return;
    }
    
    ne_buffer_zappend(request, "Cookie: ");

    for (cook = cache->cookies; cook != NULL; cook=cook->next) {
	ne_buffer_concat(request, cook->name, "=", cook->value, NULL);
	if (cook->next != NULL) {
	    ne_buffer_zappend(request, "; ");
	}
    }
    
    ne_buffer_zappend(request, "\r\n");
}

/* Register cookie handling for given session using given cache. */
void ne_cookie_register(ne_session *sess, ne_cookie_cache *cache)
{
    ne_hook_create_request(sess, create, cache);
    ne_hook_pre_send(sess, pre_send, cache);
}

