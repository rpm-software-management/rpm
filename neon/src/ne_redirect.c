/* 
   HTTP-redirect support
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
#include "ne_string.h"

#define REDIRECT_ID "http://www.webdav.org/neon/hooks/http-redirect"

struct redirect {
    char *location;
    char *requri;
    int valid; /* non-zero if .uri contains a redirect */
    ne_uri uri;
    ne_session *sess;
};

static void
create(ne_request *req, void *session, const char *method, const char *uri)
	/*@modifies req @*/
{
    struct redirect *red = session;
    NE_FREE(red->location);
    NE_FREE(red->requri);
    red->requri = ne_strdup(uri);
    ne_add_response_header_handler(req, "Location", ne_duplicate_header,
				   &red->location);
}

#define REDIR(n) ((n) == 301 || (n) == 302 || (n) == 303 || \
		  (n) == 307)

static int post_send(ne_request *req, void *private, const ne_status *status)
	/*@*/
{
    struct redirect *red = private;

    /* Don't do anything for non-redirect status or no Location header. */
    if (!REDIR(status->code) || red->location == NULL)
	return NE_OK;

    if (strstr(red->location, "://") == NULL && red->location[0] != '/') {
	/* FIXME: remove this relative URI resolution hack. */
	ne_buffer *path = ne_buffer_create();
	char *pnt;
	
	ne_buffer_zappend(path, red->requri);
	pnt = strrchr(path->data, '/');

	if (pnt && *(pnt+1) != '\0') {
	    /* Chop off last path segment. */
	    *(pnt+1) = '\0';
	    ne_buffer_altered(path);
	}
	ne_buffer_zappend(path, red->location);
	ne_free(red->location);
	red->location = ne_buffer_finish(path);
    }

    /* free last uri. */
    ne_uri_free(&red->uri);
    
    /* Parse the Location header */
    if (ne_uri_parse(red->location, &red->uri) || red->uri.path == NULL) {
        red->valid = 0;
	ne_set_error(red->sess, _("Could not parse redirect location."));
	return NE_ERROR;
    }

    /* got a valid redirect. */
    red->valid = 1;
    
    if (!red->uri.host) {
	/* Not an absoluteURI: breaks 2616 but everybody does it. */
	ne_fill_server_uri(red->sess, &red->uri);
    }

    return NE_REDIRECT;
}

static void free_redirect(/*@only@*/ void *cookie)
	/*@modifies cookie @*/
{
    struct redirect *red = cookie;
    NE_FREE(red->location);
    ne_uri_free(&red->uri);
    if (red->requri)
        ne_free(red->requri);
    ne_free(red);
}

void ne_redirect_register(ne_session *sess)
{
    struct redirect *red = ne_calloc(sizeof *red);
    
    red->sess = sess;

    ne_hook_create_request(sess, create, red);
    ne_hook_post_send(sess, post_send, red);
    ne_hook_destroy_session(sess, free_redirect, red);

    ne_set_session_private(sess, REDIRECT_ID, red);
}

const ne_uri *ne_redirect_location(ne_session *sess)
{
    struct redirect *red = ne_get_session_private(sess, REDIRECT_ID);

    if (red && red->valid)
        return &red->uri;
    else
        return NULL;
}

