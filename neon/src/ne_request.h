/* 
   HTTP Request Handling
   Copyright (C) 1999-2004, Joe Orton <joe@manyfish.co.uk>

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
#include "ne_string.h" /* For ne_buffer */
#include "ne_session.h"

BEGIN_NEON_DECLS

#define NE_OK (0) /* Success */
#define NE_ERROR (1) /* Generic error; use ne_get_error(session) for message */
#define NE_LOOKUP (2) /* Server or proxy hostname lookup failed */
#define NE_AUTH (3) /* User authentication failed on server */
#define NE_PROXYAUTH (4) /* User authentication failed on proxy */
#define NE_CONNECT (5) /* Could not connect to server */
#define NE_TIMEOUT (6) /* Connection timed out */
#define NE_FAILED (7) /* The precondition failed */
#define NE_RETRY (8) /* Retry request (ne_end_request ONLY) */
#define NE_REDIRECT (9) /* See ne_redirect.h */

/* FIXME: remove this */
#define EOL "\r\n"

/* Opaque object representing a single HTTP request. */
typedef struct ne_request_s ne_request;

/***** Request Handling *****/

/* Create a request in session 'sess', with given method and path.
 * 'path' must conform to the 'abs_path' grammar in RFC2396, with an
 * optional "? query" part, and MUST be URI-escaped by the caller. */
ne_request *ne_request_create(ne_session *sess,
			      const char *method, const char *path)
	/*@*/;

/* The request body will be taken from 'size' bytes of 'buffer'. */
void ne_set_request_body_buffer(ne_request *req, const char *buffer,
				size_t size)
	/*@modifies req @*/;

/* The request body will be taken from 'length' bytes read from the
 * file descriptor 'fd', starting from file offset 'offset'. */
void ne_set_request_body_fd(ne_request *req, int fd,
                            off_t offset, off_t length)
	/*@modifies req @*/;

#ifdef NE_LFS
/* Alternate version of ne_set_request_body_fd taking off64_t 
 * offset type for systems supporting _LARGEFILE64_SOURCE. */
void ne_set_request_body_fd64(ne_request *req, int fd,
                              off64_t offset, off64_t length)
	/*@modifies req @*/;
#endif

/* "Pull"-based request body provider: a callback which is invoked to
 * provide blocks of request body on demand.
 *
 * Before each time the body is provided, the callback will be called
 * once with buflen == 0.  The body may have to be provided >1 time
 * per request (for authentication retries etc.).
 *
 * The callback must return:
 *        <0           : error, abort request.
 *         0           : ignore 'buffer' contents, end of body.
 *     0 < x <= buflen : buffer contains x bytes of body data.  */
typedef ssize_t (*ne_provide_body)(void *userdata, 
				   char *buffer, size_t buflen)
	/*@*/;

/* Install a callback which is invoked as needed to provide the
 * request body, a block at a time.  The total size of the request
 * body is 'length'; the callback must ensure that it returns no more
 * than 'length' bytes in total. */
void ne_set_request_body_provider(ne_request *req, off_t length,
				  ne_provide_body provider, void *userdata)
	/*@modifies req @*/;

#ifdef NE_LFS
/* Duplicate version of ne_set_request_body_provider, taking an off64_t
 * offset. */
void ne_set_request_body_provider64(ne_request *req, off64_t length,
                                    ne_provide_body provider, void *userdata)
	/*@modifies req @*/;
#endif

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
    void *userdata, ne_request *req, const ne_status *st)
	/*@*/;

/* An 'acceptance' callback which only accepts 2xx-class responses.
 * Ignores userdata. */
int ne_accept_2xx(void *userdata, ne_request *req, const ne_status *st)
	/*@*/;

/* An acceptance callback which accepts all responses.  Ignores
 * userdata. */
int ne_accept_always(void *userdata, ne_request *req, const ne_status *st)
	/*@*/;

/* Callback for reading a block of data.  Returns zero on success, or
 * -1 on error.  If returning an error, the response will be aborted
 * and the callback will not be invoked again.  The request dispatch
 * (or ne_read_response_block call) will fail with NE_ERROR; the
 * session error string should have been set by the callback. */
typedef int (*ne_block_reader)(void *userdata, const char *buf, size_t len)
	/*@*/;

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
 * zero.  */
void ne_add_response_body_reader(ne_request *req, ne_accept_response accpt,
				 ne_block_reader reader, void *userdata)
	/*@modifies req @*/;

/* Handle response headers. Each handler is associated with a specific
 * header field (indicated by name). The handler is then passed the
 * value of this header field. */

/* The header handler callback type */
typedef void (*ne_header_handler)(void *userdata, const char *value)
	/*@*/;

/* Adds a response header handler for the given request. userdata is passed
 * as the first argument to the header handler, and the 'value' is the
 * header field value (i.e. doesn't include the "Header-Name: " part").
 */
void ne_add_response_header_handler(ne_request *req, const char *name, 
				    ne_header_handler hdl, void *userdata)
	/*@modifies req @*/;

/* Add handler which is passed ALL header values regardless of name */
void ne_add_response_header_catcher(ne_request *req, 
				    ne_header_handler hdl, void *userdata)
	/*@modifies req @*/;

/* Stock header handlers:
 *  'duplicate': *(char **)userdata = strdup(value)
 *  'numeric':   *(int *)userdata = atoi(value)
 * e.g.
 *   int mynum;
 *   ne_add_response_header_handler(myreq, "Content-Length",
 *                                    ne_handle_numeric_handler, &mynum);
 * ... arranges mynum to be set to the value of the Content-Length header.
 */
void ne_duplicate_header(void *userdata, const char *value)
	/*@modifies userdata @*/;
void ne_handle_numeric_header(void *userdata, const char *value)
	/*@modifies userdata @*/;

/* Adds a header to the request with given name and value. */
void ne_add_request_header(ne_request *req, const char *name, 
			   const char *value)
	/*@modifies req @*/;
/* Adds a header to the request with given name, using printf-like
 * format arguments for the value. */
void ne_print_request_header(ne_request *req, const char *name,
			     const char *format, ...) 
    ne_attribute((format(printf, 3, 4)))
	/*@modifies req @*/;

/* ne_request_dispatch: Sends the given request, and reads the
 * response. Response-Status information can be retrieve with
 * ne_get_status(req).
 *
 *  NE_OK         if request sent + response read okay.
 *  NE_AUTH       user not authorised on server
 *  NE_PROXYAUTH  user not authorised on proxy server
 *  NE_CONNECT    could not connect to server/proxy server
 *  NE_TIMEOUT    connection timed out mid-request
 *  NE_ERROR      for other errors, and ne_get_error() should
 *                  return a meaningful error string
 */
int ne_request_dispatch(ne_request *req)
	/*@globals internalState @*/
	/*@modifies req, internalState @*/;

/* Returns a pointer to the response status information for the given
 * request; pointer is valid until request object is destroyed. */
const ne_status *ne_get_status(const ne_request *req) ne_attribute((const))
	/*@*/;

/* Returns pointer to session associated with request. */
ne_session *ne_get_session(const ne_request *req)
	/*@*/;

/* Destroy memory associated with request pointer */
void ne_request_destroy(/*@only@*/ ne_request *req)
	/*@modifies req @*/;

/* "Caller-pulls" request interface.  This is an ALTERNATIVE interface
 * to ne_request_dispatch: either use that, or do all this yourself:
 *
 * caller must call:
 *  1. ne_begin_request (fail if returns non-NE_OK)
 *  2. while(ne_read_response_block(...) > 0) ... loop ...;
 *     (fail if ne_read_response_block returns <0)
 *  3. ne_end_request
 *
 * ne_end_request and ne_begin_request both return an NE_* code; if
 * ne_end_request returns NE_RETRY, you must restart the loop from (1)
 * above. */
int ne_begin_request(ne_request *req)
	/*@globals internalState @*/
	/*@modifies req, internalState @*/;
int ne_end_request(ne_request *req)
	/*@globals internalState @*/
	/*@modifies req, internalState @*/;

/* Read a block of the response.  buffer must be at least 128 bytes.
 * 'buflen' must be length of buffer.
 *
 * Returns:
 *  <0 - error, stop reading.
 *   0 - end of response
 *  >0 - number of bytes read into buffer.
 */
ssize_t ne_read_response_block(ne_request *req, char *buffer, size_t buflen)
	/*@globals internalState @*/
	/*@modifies req, buffer, internalState @*/;

/* Include the HTTP/1.1 header "Expect: 100-continue" in request 'req'
 * if 'flag' is non-zero.  Warning: 100-continue support is not
 * implemented correctly in some HTTP/1.1 servers, enabling this
 * feature may cause requests to hang or time out. */
void ne_set_request_expect100(ne_request *req, int flag)
	/*@modifies req @*/;

/**** Request hooks handling *****/

typedef void (*ne_free_hooks)(void *cookie)
	/*@*/;

/* Hook called when a create is created; passed the request method,
 * and the string used as the Request-URI (which may be an abs_path,
 * or an absoluteURI, depending on whether an HTTP proxy is in
 * use).  */
typedef void (*ne_create_request_fn)(ne_request *req, void *userdata,
				     const char *method, const char *requri)
	/*@*/;
void ne_hook_create_request(ne_session *sess, 
			    ne_create_request_fn fn, void *userdata)
	/*@modifies sess @*/;

/* Hook called before the request is sent.  'header' is the raw HTTP
 * header before the trailing CRLF is added: add in more here. */
typedef void (*ne_pre_send_fn)(ne_request *req, void *userdata, 
			       ne_buffer *header)
	/*@*/;
void ne_hook_pre_send(ne_session *sess, ne_pre_send_fn fn, void *userdata)
	/*@modifies sess @*/;

/* Hook called after the request is sent. May return:
 *  NE_OK     everything is okay
 *  NE_RETRY  try sending the request again.
 * anything else signifies an error, and the request is failed. The return
 * code is passed back the _dispatch caller, so the session error must
 * also be set appropriately (ne_set_error).
 */
typedef int (*ne_post_send_fn)(ne_request *req, void *userdata,
			       const ne_status *status)
	/*@*/;
void ne_hook_post_send(ne_session *sess, ne_post_send_fn fn, void *userdata)
	/*@modifies sess @*/;

/* Hook called when the function is destroyed. */
typedef void (*ne_destroy_req_fn)(ne_request *req, void *userdata)
	/*@*/;
void ne_hook_destroy_request(ne_session *sess,
			     ne_destroy_req_fn fn, void *userdata)
	/*@modifies sess @*/;

typedef void (*ne_destroy_sess_fn)(void *userdata)
	/*@*/;
/* Hook called when the session is destroyed. */
void ne_hook_destroy_session(ne_session *sess,
			     ne_destroy_sess_fn fn, void *userdata)
	/*@modifies sess @*/;

/* Store an opaque context for the request, 'priv' is returned by a
 * call to ne_request_get_private with the same ID. */
void ne_set_request_private(ne_request *req, const char *id, void *priv)
	/*@modifies req @*/;
/*@relnull@*/
void *ne_get_request_private(ne_request *req, const char *id)
	/*@*/;

END_NEON_DECLS

#endif /* NE_REQUEST_H */
