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

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "ne_session.h"
#include "ne_request.h"
#include "ne_alloc.h"
#include "ne_uri.h"
#include "ne_redirect.h"
#include "ne_i18n.h"

/* TODO: should remove this. */
#include "ne_private.h"

#define REDIRECT_ID "http://www.webdav.org/neon/hooks/http-redirect"

struct redirect {
    /* per-request stuff. */
    char *location;
    ne_request *req;
    const char *method, *request_uri;

    /* per-session stuff. */
    ne_session *session;
    ne_redirect_confirm confirm;
    ne_redirect_notify notify;
    void *userdata;
};

static void *create(void *session, ne_request *req, 
		    const char *method, const char *uri);
static int post_send(void *private, const ne_status *status);

static const ne_request_hooks redirect_hooks = {
    REDIRECT_ID,
    create,
    NULL,
    post_send,
    NULL
};

static void *
create(void *session, ne_request *req, const char *method, const char *uri)
{
    struct redirect *red = session;
    
    /* Free up the location. */
    NE_FREE(red->location);

    /* for handling 3xx redirects */
    ne_add_response_header_handler(req, "Location",
				     ne_duplicate_header, &red->location);

    red->req = req;
    red->method = method;
    red->request_uri = uri;

    return red;
}

/* 2616 says we can't auto-redirect if the method is not GET or HEAD.
 * We extend this to PROPFIND and OPTIONS, which violates a 2616 MUST,
 * but is following the spirit of the spec, I think.
 *
 * In fact Roy Fielding said as much in a new-httpd posting
 * <20010224232203.G799@waka.ebuilt.net>: the spec should allow
 * auto-redirects of any read-only method.  
 *
 * We have no interface for saying "this method is read-only",
 * although this might be useful.
 */
static int auto_redirect(struct redirect *red)
{
    return (strcasecmp(red->method, "HEAD") == 0 ||
	    strcasecmp(red->method, "GET") == 0 || 
	    strcasecmp(red->method, "PROPFIND") == 0 ||
	    strcasecmp(red->method, "OPTIONS") == 0);
}

static int post_send(void *private, const ne_status *status)
{
    struct redirect *red = private;
    struct uri uri;
    int ret = NE_OK;

    if ((status->code != 302 && status->code != 301) ||
	red->location == NULL) {
	/* Nothing to do. */
	return NE_OK;
    }
    
    if (uri_parse(red->location, &uri, NULL)) {
	/* Couldn't parse the URI */
	ne_set_error(red->session, _("Could not parse redirect location."));
	return NE_ERROR;
    }
    
    if ((uri.host != NULL && 
	 strcasecmp(uri.host, red->session->server.hostname) != 0) ||
	(uri.port != -1 &&
	 uri.port != red->session->server.port) ||
	(uri.scheme != NULL &&
	 strcasecmp(uri.scheme, ne_get_scheme(red->session)) != 0)) {
	/* Cannot redirect to another server. Throw this back to the caller
	 * to have them start a new session. */
	NE_DEBUG(NE_DBG_HTTP, 
	      "Redirected to different host/port/scheme:\n"
	      "From %s://%s:%d to %s//%s:%d\n",
	      ne_get_scheme(red->session),
	      red->session->server.hostname,
	      red->session->server.port,
	      uri.scheme, uri.host, uri.port);
	ret = NE_REDIRECT;
	ne_set_error(red->session, _("Redirected to a different server.\n"));
    } else {

	/* Same-server redirect. Can we auto-redirect? */

	if (!auto_redirect(red) && 
	    (red->confirm == NULL ||
	     !(*red->confirm)(red->userdata, red->request_uri, uri.path))) {
	    /* No auto-redirect or confirm failed. */
	    ret = NE_ERROR;
	} else {
	    /* Okay, follow it: set the new URI and retry the request. */
	    ne_set_request_uri(red->req, uri.path);
	    ret = NE_RETRY;
	    /* Notify them that we're following the redirect. */
	    if (red->notify != NULL) {
		red->notify(red->userdata, red->request_uri, uri.path);
	    }
	}
    }

    /* Free up the URI. */
    uri_free(&uri);

    return ret;
}

static void free_redirect(void *cookie)
{
    struct redirect *red = cookie;
    NE_FREE(red->location);
    free(red);
}

void ne_redirect_register(ne_session *sess, 
			  ne_redirect_confirm confirm,
			  ne_redirect_notify notify,
			  void *userdata)
{
    struct redirect *red = ne_calloc(sizeof *red);
    
    red->confirm = confirm;
    red->notify = notify;
    red->userdata = userdata;
    red->session = sess;

    ne_add_hooks(sess, &redirect_hooks, red, free_redirect);
}

const char *ne_redirect_location(ne_session *sess)
{
    struct redirect *red = ne_session_hook_private(sess, REDIRECT_ID);
    return red->location;
}

