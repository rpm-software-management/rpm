/* 
   Basic cookie support for neon
   Copyright (C) 2000, Joe Orton <joe@orton.demon.co.uk>

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

   Id: http_cookies.c,v 1.5 2000/07/16 16:26:46 joe Exp 
*/

/* A nice demo of hooks, since it doesn't need any external
 * interface to muck with the stored states.
 */

#include <config.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <time.h>

#include "http_request.h"
#include "string_utils.h"
#include "http_cookies.h"
#include "xalloc.h"

static void set_cookie_hdl(void *userdata, const char *value)
{
    char **pairs = pair_string(value, ';', '=', HTTP_QUOTES, HTTP_WHITESPACE);
    http_cookie *cook;
    http_cookie_cache *cache = userdata;
    int n;

    /* Check sanity */
    if (pairs[0] == NULL || pairs[1] == NULL) {
	/* yagaboo */
	return;
    }

    DEBUG(DEBUG_HTTP, "Got cookie name=%s\n", pairs[0]);

    /* Search for a cookie of this name */
    DEBUG(DEBUG_HTTP, "Searching for existing cookie...\n");
    for (cook = cache->cookies; cook != NULL; cook = cook->next) {
	if (strcasecmp(cook->name, pairs[0]) == 0) {
	    break;
	}
    }
    
    if (cook == NULL) {
	DEBUG(DEBUG_HTTP, "New cookie.\n");
	cook = xmalloc(sizeof(http_cookie));
	memset(cook, 0, sizeof(http_cookie));
	cook->name = pairs[0];
	cook->next = cache->cookies;
	cache->cookies = cook;
    } else {
	/* Free the old value */
	free(cook->value);
    }

    cook->value = pairs[1];

    for (n = 2; pairs[n] != NULL; n+=2) {
	DEBUG(DEBUG_HTTP, "Cookie parm %s=%s\n", pairs[n], pairs[n+1]);
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

    DEBUG(DEBUG_HTTP, "End of parms.\n");

    pair_string_free(pairs);
}

static void *create(void *session, http_req *req, const char *method, const char *uri)
{
    http_cookie_cache *cache = session;
    http_add_response_header_handler(req, "Set-Cookie", set_cookie_hdl, cache);
    return cache;
}

/* Just before sending the request: let them add headers if they want */
static void pre_send(void *private, sbuffer request)
{
    http_cookie_cache *cache = private;
    http_cookie *cook;
    
    if (cache->cookies == NULL) {
	return;
    }
    
    sbuffer_zappend(request, "Cookie: ");

    for (cook = cache->cookies; cook != NULL; cook=cook->next) {
	sbuffer_concat(request, cook->name, "=", cook->value, NULL);
	if (cook->next != NULL) {
	    sbuffer_zappend(request, "; ");
	}
    }
    
    sbuffer_zappend(request, EOL);
    
}

static void destroy(void *private)
{
    /* FIXME */
    return;
}

http_request_hooks http_cookie_hooks = {
    "http://www.webdav.org/neon/hooks/cookies",
    create,
    NULL,
    pre_send,
    NULL,
    destroy
};
