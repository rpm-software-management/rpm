/* 
   HTTP authentication routines
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

#ifndef HTTPAUTH_H
#define HTTPAUTH_H

#include <sys/types.h>

#include "md5.h"

/* HTTP Authentication - a pretty complete client implementation of RFC2617.
 */

/*
  To use:
 
 1. A session state variable (http_auth_session) is needed for each of
 server state and proxy state. These may be statically declared (use
 _init/_finish), or dynamically (use _create/_destroy).
 
 2. To begin a new session, call http_auth_init() or http_auth_create().
 Set up a callback function with http_auth_set_creds_cb() for
 supplying the username and password on demand. See below for details.

 3. Before sending a request, pass http_auth_new_request its details,
 on BOTH auth session variables if you are using a proxy server too.
 
 4. Call http_auth_request_header() and add your 'Authentication:'
 header to the request if returns non-NULL. Similarly for
 Proxy-Authentication.
 
 5. Send the request.

 6. Read the response:
  - Pass the value of the '(Proxy|WWW)-Authenticate' header to 
    http_auth_challenge.
  - If there is 'Authentication-Info', save its value for later.
  - Pass each block of the response entity-body to http_auth_response_body.

 7. After reading the complete response, if an Auth-Info header was
 received, pass its value to http_auth_verify_response to check
 whether the SERVER was authenticated okay, passing the saved value.

 8. If a 401 or a 407 response is received, retry once for each, by
 going back to step 3. Note that http_auth_new_request MUST be called
 again if the SAME request is being retried.

*/

/* The authentication scheme we are using */
typedef enum {
    http_auth_scheme_basic,
    http_auth_scheme_digest
} http_auth_scheme;

typedef enum { 
    http_auth_alg_md5,
    http_auth_alg_md5_sess,
    http_auth_alg_unknown
} http_auth_algorithm;

/* Selected method of qop which the client is using */
typedef enum {
    http_auth_qop_none,
    http_auth_qop_auth,
    http_auth_qop_auth_int
} http_auth_qop;

/* The callback used to request the username and password in the given
 * realm. The username and password must be placed in malloc()-allocate
 * memory.
 * Must return:
 *   0 on success, 
 *  -1 to cancel.
 */
typedef int (*http_auth_request_creds)( 
    void *userdata, const char *realm,
    char **username, char **password );

/* Authentication session state. */
typedef struct {
    /* The scheme used for this authentication session */
    http_auth_scheme scheme;
    /* The callback used to request new username+password */
    http_auth_request_creds reqcreds;
    void *reqcreds_udata;

    /*** Session details ***/

    /* The username and password we are using to authenticate with */
    char *username;
    /* Whether we CAN supply authentication at the moment */
    unsigned int can_handle:1;
    /* This used for Basic auth */
    char *basic; 
    /* These all used for Digest auth */
    char *unq_realm;
    char *unq_nonce;
    char *unq_cnonce;
    char *opaque;
    /* A list of domain strings */
    unsigned int domain_count;
    char **domain;
    http_auth_qop qop;
    http_auth_algorithm alg;
    int nonce_count;
    /* The ASCII representation of the session's H(A1) value */
    char h_a1[33];
    /* Used for calculation of H(entity-body) of the response */
    struct md5_ctx response_body;
    /* Temporary store for half of the Request-Digest
     * (an optimisation - used in the response-digest calculation) */
    struct md5_ctx stored_rdig;

    /* Details of server... needed to reconstruct absoluteURI's when
     * necessary */
    const char *host;
    const char *uri_scheme;
    unsigned int port;

    /*** Details of current request ***/

    /* The method and URI we are using for the current request */
    const char *uri;
    const char *method;
    /* Whether we WILL supply authentication for this request or not */
    unsigned int will_handle:1;
    /* Whether we have a request body for the current request */
    unsigned int got_body:1;
    /* And what the body is - stream or buffer */
    FILE *body_stream;
    const char *body_buffer;

} http_auth_session;

/* Initializes the authentication state for the given session,
 * which will use the given username and password. */
void http_auth_init( http_auth_session *sess );

void http_auth_set_creds_cb(  http_auth_session *sess,
    http_auth_request_creds callback, void *userdata );

/* Finishes off the given authentication session, freeing
 * any memory used. */
void http_auth_finish( http_auth_session *sess );

/* Creates a new authentication session.
 * Returns non-NULL on success */
http_auth_session * http_auth_create( void );

/* Destroys an authentication session, freeing the session state
 * itself too. */
void http_auth_destroy( http_auth_session *sess ); 

/* Call this before sending a request.  Pass ONE OF body_buffer or
 * body_stream as non-NULL if the request will include an
 * entity-body. If body_buffer is non-NULL, it MUST be
 * \0-terminated. If body_stream is non-NULL, it may be read once
 * during http_auth_challenge, then rewound.  uri must identical to
 * Request-URI, EXCEPT for server auth state, where if the request is
 * passing through a proxy, then uri should be the same as abs_path.  */
void http_auth_new_request( http_auth_session *sess,
			    const char *method, const char *uri,
			    const char *body_buffer, FILE *body_stream );

/* Returns the value of the authentication field if one is to be sent,
 * else NULL. The return value will be taken from malloc()'ed memory,
 * so should be free()'ed after use. */
char *http_auth_request_header( http_auth_session *sess );

/* Pass this the value of the "(Proxy,WWW)-Authenticate: " header field.
 * Returns:
 *   0 if we can now authenticate ourselves with the server.
 *   non-zero if we can't
 */
int http_auth_challenge( http_auth_session *sess, const char *value );

/* As you receive sections of the response entity-body, pass them to 
 * this function. */
void http_auth_response_body( http_auth_session *sess, 
			      const char *buffer, size_t buffer_len );

/* If you receive a "(Proxy-)Authentication-Info:" header, pass its value to
 * this function. Returns zero if this successfully authenticates
 * the response as coming from the server, and false if it hasn't. */
int http_auth_verify_response( http_auth_session *sess, const char *value );

#endif /* HTTPAUTH_H */
