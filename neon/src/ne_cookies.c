/* 
   Basic cookie support for neon
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

static void set_cookie_hdl(void *userdata, const char *value)
{
    char **pairs = pair_string(value, ';', '=', HTTP_QUOTES, HTTP_WHITESPACE);
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
	cook = ne_malloc(sizeof(ne_cookie));
	memset(cook, 0, sizeof(ne_cookie));
	cook->name = pairs[0];
	cook->next = cache->cookies;
	cache->cookies = cook;
    } else {
	/* Free the old value */
	free(cook->value);
    }

    cook->value = pairs[1];

    for (n = 2; pairs[n] != NULL; n+=2) {
	NE_DEBUG(NE_DBG_HTTP, "Cookie parm %s=%s\n", pairs[n], pairs[n+1]);
	if (strcasecmp(pairs[n], "path") == 0) {
	    cook->path = pairs[n+1];
	    pairs[n+1] = NULL;
	} else if (strcasecmp(pairs[n], "max-age") == 0) {
	    int t = atoi(pairs[n+1]);
	    cook->expiry = time(NULL) + (time_t)t;
	} else if (strcasecmp(pairs[n], "domain") == 0) {
	    cook->domain = pairs[n+1];
	    pairs[n+1] = NULL;
	}
    }

    NE_DEBUG(NE_DBG_HTTP, "End of parms.\n");

    pair_string_free(pairs);
}

static void *create(void *session, ne_request *req, const char *method, const char *uri)
{
    ne_cookie_cache *cache = session;
    ne_add_response_header_handler(req, "Set-Cookie", set_cookie_hdl, cache);
    return cache;
}

/* Just before sending the request: let them add headers if they want */
static void pre_send(void *private, ne_buffer *request)
{
    ne_cookie_cache *cache = private;
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
    
    ne_buffer_zappend(request, EOL);
    
}

static void destroy(void *private)
{
    /* FIXME */
    return;
}

ne_request_hooks ne_cookie_hooks = {
    "http://www.webdav.org/neon/hooks/cookies",
    create,
    pre_send,
    NULL,
    destroy
};
