/* 
   HTTP Request Handling
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

/* THIS IS NOT A PUBLIC INTERFACE. You CANNOT include this header file
 * from an application.  */
 
#ifndef HTTP_PRIVATE_H
#define HTTP_PRIVATE_H

#include "http_auth.h"

struct host_info {
    const char *hostname;
    int port;
    struct in_addr addr;
    char *hostport; /* URI hostport segment */
    http_auth_session auth;
    http_request_auth auth_callback;
    void *auth_userdata;
};

typedef enum {
    body_buffer,
    body_stream,
    body_none
} request_body;

/* This is called with each of the headers in the response */
struct header_handler {
    const char *name;
    http_header_handler handler;
    void *userdata;
    struct header_handler *next;
};

/* TODO: could unify these all into a generic callback list */

struct body_reader {
    http_block_reader handler;
    http_accept_response accept_response;
    unsigned int use:1;
    void *userdata;
    struct body_reader *next;
};

/* Store hook information */
struct hook {
    http_request_hooks *hooks;
    void *private;
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
struct http_session_s {
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

    http_use_proxy proxy_decider;
    void *proxy_decider_udata;

    nssl_context *ssl_context;

    struct hook *hooks;

    char *location; /* 302 redirect location of last request */

    char *user_agent; /* full User-Agent string */

    /* The last HTTP-Version returned by the server */
    int version_major;
    int version_minor;

    /* Error string */
    char error[BUFSIZ];
};

struct http_req_s {
    const char *method;
    char *uri, *abs_path;
    
    /*** Request ***/

    sbuffer headers;
    request_body body;
    FILE *body_stream;
    const char *body_buffer;
    size_t body_size;

    /**** Response ***/

    /* The transfer encoding types */
    struct http_response {
	unsigned int is_chunked; /* Are we using chunked TE? */
	int length;            /* Response entity-body content-length */
	int left;              /* Bytes left to read */
	long int chunk_left;   /* Bytes of chunk left to read */
    } resp;

    /* List of callbacks which are passed response headers */
    struct header_handler *header_handlers;
    /* List of callbacks which are passed response body blocks */
    struct body_reader *body_readers;

    /*** Miscellaneous ***/
    unsigned int method_is_head:1;
    unsigned int use_proxy:1;
    unsigned int use_expect100:1;
    unsigned int can_persist:1;
    unsigned int forced_close:1;
    unsigned int upgrade_to_tls:1;

    http_session *session;
    http_status *status; /* TODO: get rid of this */

    /* stores request-private hook info */
    struct hook_request *hook_store;

#ifdef USE_DAV_LOCKS
    /* TODO: move this to hooks... list of locks to submit */
    struct dav_submit_locks *if_locks;
#endif

};

#endif /* HTTP_PRIVATE_H */
