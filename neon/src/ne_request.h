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

#ifndef NE_REQUEST_H
#define NE_REQUEST_H

#include "ne_utils.h" /* For ne_status */
#include "ne_string.h" /* For sbuffer */
#include "ne_session.h"

BEGIN_NEON_DECLS

#define NE_OK (0)
#define NE_ERROR (1) /* Generic error; use ne_get_error(session) for message */
#define NE_LOOKUP (3) /* Name lookup failed */
#define NE_AUTH (4) /* User authentication failed on server */
#define NE_AUTHPROXY (5) /* User authentication failed on proxy */
#define NE_SERVERAUTH (6) /* Server authentication failed */
#define NE_PROXYAUTH (7) /* Proxy authentication failed */
#define NE_CONNECT (8) /* Could not connect to server */
#define NE_TIMEOUT (9) /* Connection timed out */
#define NE_FAILED (10) /* The precondition failed */
#define NE_RETRY (11) /* Retry request (ne_end_request ONLY) */
#define NE_REDIRECT (12) /* See ne_redirect.h */

#define EOL "\r\n"

typedef struct ne_request_s ne_request;

/***** Request Handling *****/

/* Create a new request, with given method and URI. 
 * Returns request pointer. */
ne_request *
ne_request_create(ne_session *sess, const char *method, const char *uri);

/* 'buffer' will be sent as the request body with given request. */
void ne_set_request_body_buffer(ne_request *req, const char *buffer,
				size_t size);

/* Contents of stream will be sent as the request body with the given
 * request. Returns:
 *  0 on okay.
 *  non-zero if could not determine length of file.
 */
int ne_set_request_body_fd(ne_request *req, int fd);

/* Callback for providing request body blocks.
 *
 * Before each time the body is provided, the callback will be called
 * once with buflen == 0.  The body may have to be provided >1 time
 * per request (for authentication retries etc.).
 *
 * The callback must return:
 *        <0           : error, abort.
 *         0           : ignore 'buffer' contents, end of body.
 *     0 < x <= buflen : buffer contains x bytes of body data.
 */
typedef ssize_t (*ne_provide_body)(void *userdata, 
				   char *buffer, size_t buflen);

/* Callback is called to provide blocks of request body. */
void ne_set_request_body_provider(ne_request *req, size_t size,
				  ne_provide_body provider, void *userdata);

/* Handling response bodies... you provide TWO callbacks:
 *
 * 1) 'acceptance' callback: determines whether you want to handle the
 * response body given the response-status information, e.g., if you
 * only want 2xx responses, say so here.
 *
 * 2) 'reader' callback: passed blocks of the response-body as they
 * arrive, if the acceptance callback returned non-zero.  */

/* 'acceptance' callback type. Return non-zero to accept the response,
 * else zero to ignore it. */
typedef int (*ne_accept_response)(
    void *userdata, ne_request *req, ne_status *st);

/* An 'acceptance' callback which only accepts 2xx-class responses.
 * Ignores userdata. */
int ne_accept_2xx(void *userdata, ne_request *req, ne_status *st);

/* An acceptance callback which accepts all responses.  Ignores
 * userdata. */
int ne_accept_always(void *userdata, ne_request *req, ne_status *st);

/* The 'reader' callback type */
typedef void (*ne_block_reader)(
    void *userdata, const char *buf, size_t len);

/* Add a response reader for the given request, with the given
 * acceptance function. userdata is passed as the first argument to
 * the acceptance + reader callbacks. 
 *
 * The acceptance callback is called once each time the request is
 * sent: it may be sent >1 time because of authentication retries etc.
 * For each time the acceptance callback is called, if it returns
 * non-zero, blocks of the response body will be passed to the reader
 * callback as the response is read.  After all the response body has
 * been read, the callback will be called with a 'len' argument of
 * zero.
 */
void ne_add_response_body_reader(ne_request *req, ne_accept_response accpt,
				 ne_block_reader rdr, void *userdata);

/* Handle response headers. Each handler is associated with a specific
 * header field (indicated by name). The handler is then passed the
 * value of this header field. */

/* The header handler callback type */
typedef void (*ne_header_handler)(void *userdata, const char *value);

/* Adds a response header handler for the given request. userdata is passed
 * as the first argument to the header handler, and the 'value' is the
 * header field value (i.e. doesn't include the "Header-Name: " part").
 */
void ne_add_response_header_handler(ne_request *req, const char *name, 
				    ne_header_handler hdl, void *userdata);

/* Add handler which is passed ALL header values regardless of name */
void ne_add_response_header_catcher(ne_request *req, 
				    ne_header_handler hdl, void *userdata);

/* Stock header handlers:
 *  'duplicate': *(char **)userdata = strdup(value)
 *  'numeric':   *(int *)userdata = atoi(value)
 * e.g.
 *   int mynum;
 *   ne_add_response_header_handler(myreq, "Content-Length",
 *                                    ne_handle_numeric_handler, &mynum);
 * ... arranges mynum to be set to the value of the Content-Length header.
 */
void ne_duplicate_header(void *userdata, const char *value);
void ne_handle_numeric_header(void *userdata, const char *value);

/* Adds a header to the request with given name and value. */
void ne_add_request_header(ne_request *req, const char *name, 
			   const char *value);
/* Adds a header to the request with given name, using printf-like
 * format arguments for the value. */
void ne_print_request_header(ne_request *req, const char *name,
			     const char *format, ...)
#ifdef __GNUC__
                __attribute__ ((format (printf, 3, 4)))
#endif /* __GNUC__ */
;

/* ne_request_dispatch: Sends the given request, and reads the
 * response. Response-Status information can be retrieve with
 * ne_get_status(req).
 *
 * Returns:
 *  NE_OK         if request sent + response read okay.
 *  NE_AUTH       if user authentication failed on origin server
 *  NE_AUTHPROXY  if user authentication failed on proxy server
 *  NE_SERVERAUTH server authentication failed
 *  NE_PROXYAUTH  proxy authentication failed
 *  NE_CONNECT    could not connect to server/proxy server
 *  NE_TIMEOUT    connection timed out mid-request
 *  NE_ERROR      for other errors, and ne_get_error() should
 *                  return a meaningful error string
 *
 * NB: NE_AUTH and NE_AUTHPROXY mean that the USER supplied the
 * wrong username/password.  SERVER/PROXYAUTH mean that, after the
 * server has accepted a valid username/password, the server/proxy did
 * not authenticate the response message correctly.
 * */
int ne_request_dispatch(ne_request *req);

/* Returns a pointer to the response status information for the
 * given request. */
const ne_status *ne_get_status(ne_request *req)

/* Declare this with attribute const, since we often call it >1 times
 * with the same argument, and it will return the same thing each
 * time. This lets the compiler optimize away any subsequent calls
 * (theoretically).  */
#ifdef __GNUC__
                __attribute__ ((const))
#endif /* __GNUC__ */
    ;

/* Destroy memory associated with request pointer */
void ne_request_destroy(ne_request *req);

/* "Caller-pulls" request interface.  This is an ALTERNATIVE interface
 * to ne_request_dispatch: either use that, or do all this yourself:
 *
 * caller must call:
 *  1. ne_begin_request (fail if returns non-NE_OK)
 *  2. while(ne_read_response_block(...) > 0) ... loop ...;
 *  3. ne_end_request
 * If ne_end_request returns NE_RETRY, MUST go back to 1.
 */
int ne_begin_request(ne_request *req);
int ne_end_request(ne_request *req);

/* Read a block of the response.  buffer must be at least 128 bytes.
 * 'buflen' must be length of buffer.
 *
 * Returns:
 *  <0 - error, stop reading.
 *   0 - end of response
 *  >0 - number of bytes read into buffer.
 */
ssize_t ne_read_response_block(ne_request *req, char *buffer, size_t buflen);

/**** Request hooks handling *****/

typedef struct {
    /* A slight hack? Allows access to the hook private information,
     * externally... */
    const char *id; /* id could be a URI to be globally unique */
    /* e.g. "http://webdav.org/neon/hooks/davlock" */
    
    /* Register a new request: return value passed to subsequent
     * handlers as '_private'. Session cookie passed as 'session'. */
    void *(*create)(void *cookie, ne_request *req, 
		    const char *method, const char *uri);
    /* Just before sending the request: let them add headers if they want */
    void (*pre_send)(void *_private, ne_buffer *request);
    /* After sending the request. May return:
     *  NE_OK     everything is okay
     *  NE_RETRY  try sending the request again.
     *  otherwise: request fails.  Returned to _dispatch caller.
     */
    int (*post_send)(void *_private, const ne_status *status);
    /* Clean up after yourself. */
    void (*destroy)(void *_private);
} ne_request_hooks;

typedef void (*ne_free_hooks)(void *cookie);

/* Add in hooks.
 *  'cookie' will be passed to each call to the 'create' handler of the hooks.
 * If 'free_hooks' is non-NULL, it will called when the session is destroyed,
 * to free any data associated with 'cookie'.
 */
void ne_add_hooks(ne_session *sess, 
		  const ne_request_hooks *hooks, void *cookie,
		  ne_free_hooks free_cookie);

/* Return private data for a new request */ 
void *ne_request_hook_private(ne_request *req, const char *id);

/* Return private data for the session */
void *ne_session_hook_private(ne_session *sess, const char *id);

END_NEON_DECLS

#endif /* NE_REQUEST_H */

