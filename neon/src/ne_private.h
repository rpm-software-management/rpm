/* 
   HTTP Request Handling
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

/* THIS IS NOT A PUBLIC INTERFACE. You CANNOT include this header file
 * from an application.  */
 
#ifndef NE_PRIVATE_H
#define NE_PRIVATE_H

#include "ne_request.h"

struct host_info {
    /* hostname is not const since it changes on redirects. */
    char *hostname;
    int port;
    struct in_addr addr;
    char *hostport; /* URI hostport segment */
};

/* This is called with each of the headers in the response */
struct header_handler {
    char *name;
    ne_header_handler handler;
    void *userdata;
    struct header_handler *next;
};

/* TODO: could unify these all into a generic callback list */

struct body_reader {
    ne_block_reader handler;
    ne_accept_response accept_response;
    unsigned int use:1;
    void *userdata;
    struct body_reader *next;
};

/* Store hook information */
struct hook {
    const ne_request_hooks *hooks;
    void *private;
    ne_free_hooks free;
    struct hook *next;
};

/* Per-request store for hooks.
 * This is a bit noddy really. */
struct hook_request {
    struct hook *hook;
    void *cookie;
    struct hook_request *next;
};

#define HAVE_HOOK(st,func) (st->hook->hooks->func != NULL)
#define HOOK_FUNC(st, func) (*st->hook->hooks->func)

/* Session support. */
struct ne_session_s {
    /* Connection information */
    nsocket *socket;

    struct host_info server, proxy;

    /* Connection states:
     *   0:  Not connected at all.
     *   1:  We have a TCP connection to the next-hop server.
     *   2:  We have a negotiated an SSL connection over the proxy's 
     *       TCP tunnel.
     *
     * Note, 1 is all we need if we don't have a proxy server, or
     * if we do have a proxy server and we're not using SSL.
     */
    unsigned int connected:2;

    /* Settings */
    unsigned int have_proxy:1; /* do we have a proxy server? */
    unsigned int no_persist:1; /* set to disable persistent connections */
    unsigned int use_secure:1; /* whether a secure connection is required */
    int expect100_works:2; /* known state of 100-continue support */
    unsigned int in_connect:1; /* doing a proxy CONNECT */
    unsigned int request_secure_upgrade:1; 
    unsigned int accept_secure_upgrade:1;

    ne_use_proxy proxy_decider;
    void *proxy_decider_udata;

    nssl_context *ssl_context;

    sock_progress progress_cb;
    void *progress_ud;

    ne_notify_status notify_cb;
    void *notify_ud;

    struct hook *hooks;

    char *user_agent; /* full User-Agent string */

    /* The last HTTP-Version returned by the server */
    int version_major;
    int version_minor;

    /* Error string */
    char error[BUFSIZ];
};

struct ne_request_s {
    const char *method;
    char *uri, *abs_path;
    
    /*** Request ***/

    ne_buffer *headers;
    ne_provide_body body_cb;
    void *body_ud;

    int body_fd;
    const char *body_buffer, *body_pnt;
    size_t body_size, body_left;

    /* temporary store for request. */
    ne_buffer *reqbuf;

    /* temporary store for response lines. */
    ne_buffer *respbuf;

    /**** Response ***/

    /* The transfer encoding types */
    struct ne_response {
	int length;            /* Response entity-body content-length */
	size_t left;              /* Bytes left to read */
	size_t chunk_left;        /* Bytes of chunk left to read */
	size_t total;             /* total bytes read so far. */
	unsigned int is_chunked; /* Are we using chunked TE? */
    } resp;

    /* List of callbacks which are passed response headers */
    struct header_handler *header_catchers;
    
    /* We store response header handlers in a hash table.  The hash is
     * derived from the header name in lower case. */

    /* 53 is magic, of course.  For a standard http_get (with
     * redirects), 9 header handlers are defined.  Two of these are
     * for Content-Length (which is a bug, and should be fixed
     * really).  Ignoring that hash clash, the 8 *different* handlers
     * all hash uniquely into the hash table of size 53.  */
#define HH_HASHSIZE 53
    
    struct header_handler *header_handlers[HH_HASHSIZE];
    /* List of callbacks which are passed response body blocks */
    struct body_reader *body_readers;

    /*** Miscellaneous ***/
    unsigned int method_is_head:1;
    unsigned int use_proxy:1;
    unsigned int use_expect100:1;
    unsigned int can_persist:1;
    unsigned int forced_close:1;
    unsigned int upgrade_to_tls:1;

    ne_session *session;
    ne_status status;

    /* stores request-private hook info */
    struct hook_request *hook_store;

#ifdef USE_DAV_LOCKS
    /* TODO: move this to hooks... list of locks to submit */
    struct dav_submit_locks *if_locks;
#endif

};

/* Set a new URI for the given request. */
void ne_set_request_uri(ne_request *req, const char *uri);

typedef int (*ne_push_fn)(void *userdata, const char *buf, size_t count);

/* Pulls the request body for the given request, passing blocks to the
 * given callback.
 */
int ne_pull_request_body(ne_request *req, ne_push_fn fn, void *ud);

#endif /* HTTP_PRIVATE_H */
