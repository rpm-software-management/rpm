/* 
   HTTP session handling
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

#ifndef NE_SESSION_H
#define NE_SESSION_H 1

#include "ne_socket.h" /* for nssl_context */
#include "ne_defs.h"

BEGIN_NEON_DECLS

typedef struct ne_session_s ne_session;

/* 
 * Session handling:
 *  Call order:
 *  0.   sock_init             MANDATORY
 *  1.   ne_session_create   MANDATORY
 *  2.   ne_session_proxy    OPTIONAL
 *  3.   ne_session_server   MANDATORY
 *  4.   ne_set_*            OPTIONAL
 *  ...  --- Any HTTP request method ---
 *  n.   ne_session_destroy  MANDATORY
 */
 
/* Create a new HTTP session */
ne_session *ne_session_create(void);

/* Finish an HTTP session */
int ne_session_destroy(ne_session *sess);

/* Prematurely force the connection to be closed for the given
 * session.  */
int ne_close_connection(ne_session *sess);

/* Set the server or proxy server to be used for the session.
 * Returns:
 *   NE_LOOKUP if the DNS lookup for hostname failed.
 *   NE_OK otherwise.
 *
 * Note that if a proxy is being used, ne_session_proxy should be
 * called BEFORE ne_session_server, so the DNS lookup can be skipped
 * on the server. */
int ne_session_server(ne_session *sess, const char *hostname, int port);
int ne_session_proxy(ne_session *sess, const char *hostname, int port);

/* Callback to determine whether the proxy server should be used or
 * not for a request to the given hostname using the given scheme.
 * Scheme will be "http" or "https" etc.
 * Must return:
 *   Zero to indicate the proxy should NOT be used
 *   Non-zero to indicate the proxy SHOULD be used.
 */
typedef int (*ne_use_proxy)(void *userdata,
			    const char *scheme, const char *hostname);

/* Register the callback for determining whether the proxy server
 * should be used or not here.  'userdata' will be passed as the first
 * argument to the callback. The callback is only called if a proxy
 * server has been set up using ne_session_proxy. 
 *
 * This function MUST be called before calling ne_session_server.
 */
void ne_session_decide_proxy(ne_session *sess, ne_use_proxy use_proxy,
			     void *userdata);

/* Set protocol options for session:
 *   expect100: Defaults to OFF
 *   persist:   Defaults to ON
 *
 * expect100: When set, send the "Expect: 100-continue" request header
 * with requests with bodies.
 *
 * persist: When set, use a persistent connection. (Generally,
 * you don't want to turn this off.)
 * */
void ne_set_expect100(ne_session *sess, int use_expect100);
void ne_set_persist(ne_session *sess, int persist);

/* Set a progress callback for the session. */
void ne_set_progress(ne_session *sess, 
		     sock_progress progress, void *userdata);

typedef enum {
    ne_conn_namelookup, /* lookup up hostname (info = hostname) */
    ne_conn_connecting, /* connecting to host (info = hostname) */
    ne_conn_connected, /* connected to host (info = hostname) */
    ne_conn_secure /* connection now secure (info = crypto level) */
} ne_conn_status;

typedef void (*ne_notify_status)(void *userdata, 
				 ne_conn_status status,
				 const char *info);

/* Set a status notification callback for the session, to report
 * connection status. */
void ne_set_status(ne_session *sess,
		   ne_notify_status status, void *userdata);

/* Using SSL/TLS connections:
 *
 * Session settings:
 *   secure:                 Defaults to OFF
 *   secure_context:         No callbacks, any protocol allowed.
 *   request_secure_upgrade: Defaults to OFF
 *   accept_secure_ugprade:  Defaults to OFF
 *
 * secure_context: When set, specifies the settings to use for secure
 * connections, and any callbacks (see nsocket.h).  The lifetime of
 * the context object must be >= to the lifetime of the session
 * object.
 *
 * secure: When set, use a secure (SSL/TLS) connection to the origin
 * server. This will tunnel (using CONNECT) through the proxy server
 * if one is being used.
 *
 * NB: ne_set_scure will return -1 if SSL is not supported by the
 * library (i.e., it was not built against OpenSSL), or 0 if it is.
 * */
void ne_set_secure_context(ne_session *sess, nssl_context *ctx);
int ne_set_secure(ne_session *sess, int secure);

/*
 * NOTE: don't bother using the two _secure_update functions yet.
 * "secure upgrades" (RFC2817) are not currently supported by any HTTP
 * servers.
 * 
 * request_secure_upgrade: When set, notify the server with each
 * request made that an upgrade to a secure connection is desired, if
 * available.
 *
 * accept_secure_upgrade: When set, allow a server-requested upgrade
 * to a secure connection. 
 *
 * These return as per ne_set_secure.  */
int ne_set_request_secure_upgrade(ne_session *sess, int req_upgrade);
int ne_set_accept_secure_upgrade(ne_session *sess, int acc_upgrade);

/* Sets the user-agent string. neon/VERSION will be appended, to make
 * the full header "User-Agent: product neon/VERSION".
 * If this function is not called, the User-Agent header is not sent.
 * The product string must follow the RFC2616 format, i.e.
 *       product         = token ["/" product-version]
 *       product-version = token
 * where token is any alpha-numeric-y string [a-zA-Z0-9]*
 */
void ne_set_useragent(ne_session *sess, const char *product);

/* Determine if next-hop server claims HTTP/1.1 compliance. Returns:
 *   0 if next-hop server does NOT claim HTTP/1.1 compliance
 *   non-zero if next-hop server DOES claim HTTP/1.1 compliance
 * Not that the "next-hop" server is the proxy server if one is being
 * used, otherwise, the origin server.
 */
int ne_version_pre_http11(ne_session *sess);

/* Returns the 'hostport' URI segment for the end-server, e.g.
 *    "my.server.com:8080"    or    "www.server.com" 
 *  (port segment is ommitted if == 80) 
 */
const char *ne_get_server_hostport(ne_session *sess);

/* Returns the URL scheme being used for the current session.
 * Does NOT include a trailing ':'. 
 * Example returns: "http" or "https".
 */
const char *ne_get_scheme(ne_session *sess);

/* Set the error string for the session */
void ne_set_error(ne_session *sess, const char *errstring);
/* Retrieve the error string for the session */
const char *ne_get_error(ne_session *sess);

END_NEON_DECLS

#endif /* NE_SESSION_H */
