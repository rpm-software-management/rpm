/* 
   HTTP request/response handling
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

/* This is the HTTP client request/response implementation.
 * The goal of this code is to be modular and simple.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef __EMX__
#include <sys/select.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h> /* just for Win32? */
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif 
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_SNPRINTF_H
#include "snprintf.h"
#endif

#include "ne_i18n.h"

#include "ne_alloc.h"
#include "ne_request.h"
#include "ne_string.h" /* for ne_buffer */
#include "ne_utils.h"
#include "ne_socket.h"
#include "ne_uri.h"

#include "ne_private.h"

#define HTTP_EXPECT_TIMEOUT 15
/* 100-continue only used if size > HTTP_EXPECT_MINSIZ */
#define HTTP_EXPECT_MINSIZE 1024

/* with thanks to Jim Blandy; this macro simplified loads of code. */
#define HTTP_ERR(x) do { int _ret = (x); if (_ret != NE_OK) return _ret; } while (0)

static int open_connection(ne_request *req);

/* The iterative step used to produce the hash value.  This is DJB's
 * magic "*33" hash function.  Ralf Engelschall has done some amazing
 * statistical analysis to show that *33 really is a good hash
 * function: check the new-httpd list archives, or his 'str' library
 * source code, for the details.
 *
 * TODO: due to limited range of characters used in header names,
 * could maybe get a better hash function to use? */
 
#define HH_ITERATE(hash, char) (((hash)*33 + char) % HH_HASHSIZE);

/* Produce the hash value for a header name, which MUST be in lower
 * case.  */
static unsigned int hdr_hash(const char *name)
{
    const char *pnt;
    unsigned int hash = 0;

    for (pnt = name; *pnt != '\0'; pnt++) {
	hash = HH_ITERATE(hash,*pnt);
    }

    return hash;
}

static int set_sockerr(ne_request *req, const char *doing, int code)
{
    switch(code) {
    case 0: /* FIXME: still needed? */
    case SOCK_CLOSED:
	if (req->use_proxy) {
	    snprintf(req->session->error, BUFSIZ,
		      _("%s: connection was closed by proxy server."), doing);
	} else {
	    snprintf(req->session->error, BUFSIZ,
		      _("%s: connection was closed by server."), doing);
	}
	return NE_ERROR;
    case SOCK_TIMEOUT:
	snprintf(req->session->error, BUFSIZ, 
		  _("%s: connection timed out."), doing);
	return NE_TIMEOUT;
    default:
	if (req->session->socket != NULL) {
	    const char *err = sock_get_error(req->session->socket);
	    if (err != NULL) {
		snprintf(req->session->error, BUFSIZ, "%s: %s", doing, err);
	    } else {
		snprintf(req->session->error, BUFSIZ, _("%s: socket error."),
			 doing);
	    }
	} else {
	    snprintf(req->session->error, BUFSIZ,
		     "%s: %s", doing, strerror(errno));
	}	    
	return NE_ERROR;
    }
}

static char *lower_string(const char *str)
{
    char *ret = ne_malloc(strlen(str) + 1), *pnt;
    
    for (pnt = ret; *str != '\0'; str++) {
	*pnt++ = tolower(*str);
    }
    
    *pnt = '\0';

    return ret;
}

static void notify_status(ne_session *sess, ne_conn_status status,
			  const char *info)
{
    if (sess->notify_cb) {
	sess->notify_cb(sess->notify_ud, status, info);
    }
}

void ne_duplicate_header(void *userdata, const char *value)
{
    char **location = userdata;
    *location = ne_strdup(value);
}

void ne_handle_numeric_header(void *userdata, const char *value)
{
    int *location = userdata;
    *location = atoi(value);
}

void ne_add_hooks(ne_session *sess, 
		    const ne_request_hooks *hooks, void *private,
		    ne_free_hooks free_hooks)
{
    struct hook *hk = ne_malloc(sizeof(struct hook));
    hk->hooks = hooks;
    hk->private = private;
    hk->next = sess->hooks;
    hk->free = free_hooks;
    sess->hooks = hk;
}

void *ne_request_hook_private(ne_request *req, const char *id)
{
    struct hook_request *hk;

    for (hk = req->hook_store; hk != NULL; hk = hk->next) {
	if (strcasecmp(hk->hook->hooks->id, id) == 0) {
	    return hk->cookie;
	}
    }

    return NULL;
}

void *ne_session_hook_private(ne_session *sess, const char *id)
{
    struct hook *hk;

    for (hk = sess->hooks; hk != NULL; hk = hk->next) {
	if (strcasecmp(hk->hooks->id, id) == 0) {
	    return hk->private;
	}
    }

    return NULL;
}

static ssize_t body_string_send(void *userdata, char *buffer, size_t count)
{
    ne_request *req = userdata;
    
    if (count == 0) {
	req->body_left = req->body_size;
	req->body_pnt = req->body_buffer;
    } else {
	/* if body_left == 0 we fall through and return 0. */
	if (req->body_left < count)
	    count = req->body_left;

	memcpy(buffer, req->body_pnt, count);
	req->body_pnt += count;
	req->body_left -= count;
    }

    return count;
}    

static ssize_t body_fd_send(void *userdata, char *buffer, size_t count)
{
    ne_request *req = userdata;

    if (count) {
	return read(req->body_fd, buffer, count);
    } else {
	/* rewind since we may have to send it again */
	return lseek(req->body_fd, SEEK_SET, 0);
    }
}

/* Pulls the request body from the source and pushes it to the given
 * callback.  Returns 0 on success, or NE_* code */
int ne_pull_request_body(ne_request *req, ne_push_fn fn, void *ud)
{
    int ret = 0;
    char buffer[BUFSIZ];
    size_t bytes;
    
    /* tell the source to start again from the beginning. */
    (void) req->body_cb(req->body_ud, NULL, 0);
    
    while ((bytes = req->body_cb(req->body_ud, buffer, BUFSIZ)) > 0) {
	ret = fn(ud, buffer, bytes);
	if (ret < 0)
	    break;
    }

    if (bytes < 0) {
	ne_set_error(req->session, _("Error reading request body."));
	ret = NE_ERROR;
    }

    return ret;
}

/* Sends the request body down the socket.
 * Returns 0 on success, or NE_* code */
static int send_request_body(ne_request *req)
{
    int ret = ne_pull_request_body(req, (ne_push_fn)sock_fullwrite, 
				   req->session->socket);
    
    if (ret < 0) {
	/* transfer failed */
	req->forced_close = 1;
	ret = set_sockerr(req, _("Could not send request body"), ret);
    }

    return ret;
}

/* Lob the User-Agent, connection and host headers in to the request
 * headers */
static void add_fixed_headers(ne_request *req) 
{
    if (req->session->user_agent) {
	ne_buffer_concat(req->headers, 
			"User-Agent: ", req->session->user_agent, EOL, NULL);
    }
    /* Send Connection: Keep-Alive for pre-1.1 origin servers, so we
     * might get a persistent connection. 2068 sec 19.7.1 says we MUST
     * NOT do this for proxies, though. So we don't.  Note that on the
     * first request on any session, we don't know whether the server
     * is 1.1 compliant, so we presume that it is not. */
    if (ne_version_pre_http11(req->session) && !req->use_proxy) {
	ne_buffer_zappend(req->headers, "Keep-Alive: " EOL);
	ne_buffer_zappend(req->headers, "Connection: TE, Keep-Alive");
    } else {
	ne_buffer_zappend(req->headers, "Connection: TE");
    }
    if (req->upgrade_to_tls) {
	ne_buffer_zappend(req->headers, ", Upgrade");
    }
    ne_buffer_zappend(req->headers, EOL);
    if (req->upgrade_to_tls) {
	ne_buffer_zappend(req->headers, "Upgrade: TLS/1.0" EOL);
    }
    /* We send TE: trailers since we understand trailers in the chunked
     * response. */
    ne_buffer_zappend(req->headers, "TE: trailers" EOL);

}

int ne_accept_always(void *userdata, ne_request *req, ne_status *st)
{
    return 1;
}				   

int ne_accept_2xx(void *userdata, ne_request *req, ne_status *st)
{
    return (st->klass == 2);
}

/* Exposed since redirect hooks need this.
 *
 * DESIGN FLAW: mutating the URI does not get passed down to hooks.
 * auth hooks need the URI.  */
void ne_set_request_uri(ne_request *req, const char *uri)
{
    ne_buffer *real_uri = ne_buffer_create();
    req->abs_path = ne_strdup(uri);
    if (req->use_proxy && strcmp(uri, "*") != 0)
	ne_buffer_concat(real_uri, ne_get_scheme(req->session), "://", 
		       req->session->server.hostport, NULL);
    ne_buffer_zappend(real_uri, uri);
    req->uri = ne_buffer_finish(real_uri);
}

/* Handler for the "Transfer-Encoding" response header */
static void te_hdr_handler(void *userdata, const char *value) 
{
    struct ne_response *resp = userdata;
    if (strcasecmp(value, "chunked") == 0) {
	resp->is_chunked = 1;
    } else {
	resp->is_chunked = 0;
    }
}

/* Handler for the "Connection" response header */
static void connection_hdr_handler(void *userdata, const char *value)
{
    ne_request *req = userdata;
    if (strcasecmp(value, "close") == 0) {
	req->forced_close = 1;
    } else if (strcasecmp(value, "Keep-Alive") == 0) {
	req->can_persist = 1;
    }
}

/* Initializes the request with given method and URI.
 * URI must be abs_path - i.e., NO scheme+hostname. It will BREAK 
 * otherwise. */
ne_request *ne_request_create(ne_session *sess,
			      const char *method, const char *uri) 
{
    ne_request *req = ne_calloc(sizeof(ne_request));

    NE_DEBUG(NE_DBG_HTTP, "Creating request...\n");

    req->session = sess;
    req->headers = ne_buffer_create();
    req->reqbuf = ne_buffer_create();
    req->respbuf = ne_buffer_create_sized(BUFSIZ);

    /* Add in the fixed headers */
    add_fixed_headers(req);

    /* Set the standard stuff */
    req->method = method;
    req->method_is_head = (strcmp(req->method, "HEAD") == 0);
    
    /* FIXME: the proxy_decider is broken if they called
     * ne_session_proxy before ne_session_server, since in that
     * case we have not done a name lookup on the session server.  */
    if (sess->have_proxy && sess->proxy_decider != NULL) {
	req->use_proxy = 
	    (*sess->proxy_decider)(sess->proxy_decider_udata,
				   ne_get_scheme(sess), sess->server.hostname);
    }
    else {
	req->use_proxy = sess->have_proxy;
    }

    if (sess->request_secure_upgrade == 1) {
	req->upgrade_to_tls = 1;
    }

    /* Add in handlers for all the standard HTTP headers. */

    ne_add_response_header_handler(req, "Content-Length", 
				      ne_handle_numeric_header, &req->resp.length);
    ne_add_response_header_handler(req, "Transfer-Encoding", 
				      te_hdr_handler, &req->resp);
    ne_add_response_header_handler(req, "Connection", 
				      connection_hdr_handler, req);

    if (uri) {
	ne_set_request_uri(req, uri);
    }

    {
	struct hook *hk;
	struct hook_request *store;
	void *cookie;

	NE_DEBUG(NE_DBG_HTTP, "Running request create hooks.\n");

	for (hk = sess->hooks; hk != NULL; hk = hk->next) {
	    cookie = (*hk->hooks->create)(hk->private, req, method, uri);
	    if (cookie != NULL) {
		store = ne_malloc(sizeof(struct hook_request));
		store->hook = hk;
		store->cookie = cookie;
		store->next = req->hook_store;
		req->hook_store = store;
	    }
	}
    }

    NE_DEBUG(NE_DBG_HTTP, "Request created.\n");

    return req;
}

static void set_body_size(ne_request *req, size_t size)
{
    req->body_size = size;
    ne_print_request_header(req, "Content-Length", "%d", size);
}

void ne_set_request_body_buffer(ne_request *req, const char *buffer,
				size_t size)
{
    req->body_buffer = buffer;
    req->body_cb = body_string_send;
    req->body_ud = req;
    set_body_size(req, size);
}

void ne_set_request_body_provider(ne_request *req, size_t bodysize,
				  ne_provide_body provider, void *ud)
{
    req->body_cb = provider;
    req->body_ud = ud;
    set_body_size(req, bodysize);
}

int ne_set_request_body_fd(ne_request *req, int fd)
{
    struct stat bodyst;

    /* Get file length */
    if (fstat(fd, &bodyst) < 0) {
	const char *err = strerror(errno);
	/* Stat failed */
	snprintf(req->session->error, BUFSIZ,
		 _("Could not find file length: %s"), err);
	NE_DEBUG(NE_DBG_HTTP, "Stat failed: %s\n", err);
	return -1;
    }
    req->body_fd = fd;
    req->body_cb = body_fd_send;
    req->body_ud = req;
    set_body_size(req, bodyst.st_size);
    return 0;
}

void ne_add_request_header(ne_request *req, const char *name, 
			     const char *value)
{
    ne_buffer_concat(req->headers, name, ": ", value, EOL, NULL);
}

void ne_print_request_header(ne_request *req, const char *name,
			       const char *format, ...)
{
    va_list params;
    char buf[BUFSIZ];
    
    va_start(params, format);
    vsnprintf(buf, BUFSIZ, format, params);
    va_end(params);
    
    ne_buffer_concat(req->headers, name, ": ", buf, EOL, NULL);
}

void
ne_add_response_header_handler(ne_request *req, const char *name, 
				 ne_header_handler hdl, void *userdata)
{
    struct header_handler *new = ne_calloc(sizeof *new);
    int hash;
    new->name = lower_string(name);
    new->handler = hdl;
    new->userdata = userdata;
    hash = hdr_hash(new->name);
    new->next = req->header_handlers[hash];
    req->header_handlers[hash] = new;
}

void ne_add_response_header_catcher(ne_request *req, 
				      ne_header_handler hdl, void *userdata)
{
    struct header_handler *new = ne_calloc(sizeof  *new);
    new->handler = hdl;
    new->userdata = userdata;
    new->next = req->header_catchers;
    req->header_catchers = new;
}

void
ne_add_response_body_reader(ne_request *req, ne_accept_response acpt,
			       ne_block_reader rdr, void *userdata)
{
    struct body_reader *new = ne_malloc(sizeof(struct body_reader));
    new->accept_response = acpt;
    new->handler = rdr;
    new->userdata = userdata;
    new->next = req->body_readers;
    req->body_readers = new;
}

void ne_request_destroy(ne_request *req) 
{
    struct body_reader *rdr, *next_rdr;
    struct header_handler *hdlr, *next_hdlr;
    struct hook_request *st, *next_st;
    int n;

    NE_FREE(req->uri);
    NE_FREE(req->abs_path);

    for (rdr = req->body_readers; rdr != NULL; rdr = next_rdr) {
	next_rdr = rdr->next;
	free(rdr);
    }

    for (hdlr = req->header_catchers; hdlr != NULL; hdlr = next_hdlr) {
	next_hdlr = hdlr->next;
	free(hdlr);
    }

    for (n = 0; n < HH_HASHSIZE; n++) {
	for (hdlr = req->header_handlers[n]; hdlr != NULL; 
	     hdlr = next_hdlr) {
	    next_hdlr = hdlr->next;
	    free(hdlr->name);
	    free(hdlr);
	}
    }

    ne_buffer_destroy(req->headers);
    ne_buffer_destroy(req->reqbuf);
    ne_buffer_destroy(req->respbuf);

    NE_DEBUG(NE_DBG_HTTP, "Running destroy hooks.\n");
    for (st = req->hook_store; st!=NULL; st = next_st) {
	next_st = st->next;
	if (HAVE_HOOK(st,destroy)) {
	    HOOK_FUNC(st,destroy)(st->cookie);
	}
	free(st);
    }

    NE_DEBUG(NE_DBG_HTTP, "Request ends.\n");
    free(req);
}


/* Reads a block of the response into buffer, which is of size buflen.
 * Returns number of bytes read, 0 on end-of-response, or NE_* on error.
 * TODO?: only make one actual read() call in here... 
 */
static int read_response_block(ne_request *req, struct ne_response *resp, 
			       char *buffer, size_t *buflen) 
{
    int willread, readlen;
    nsocket *sock = req->session->socket;
    if (resp->is_chunked) {
	/* We are doing a chunked transfer-encoding.
	 * It goes:  `SIZE CRLF CHUNK CRLF SIZE CRLF CHUNK CRLF ...'
	 * ended by a `CHUNK CRLF 0 CRLF', a 0-sized chunk.
	 * The slight complication is that we have to cope with
	 * partial reads of chunks.
	 * For this reason, resp.chunk_left contains the number of
	 * bytes left to read in the current chunk.
	 */
	if (resp->chunk_left == 0) {
	    long int chunk_len;
	    /* We are at the start of a new chunk. */
	    NE_DEBUG(NE_DBG_HTTP, "New chunk.\n");
	    readlen = sock_readline(sock, buffer, *buflen);
	    if (readlen <= 0) {
		return set_sockerr(req, _("Could not read chunk size"), readlen);
	    }
	    NE_DEBUG(NE_DBG_HTTP, "[Chunk Size] < %s", buffer);
	    chunk_len = strtol(buffer, NULL, 16);
	    if (chunk_len == LONG_MIN || chunk_len == LONG_MAX) {
		NE_DEBUG(NE_DBG_HTTP, "Couldn't read chunk size.\n");
		ne_set_error(req->session, _("Could not parse chunk size"));
		return -1;
	    }
	    NE_DEBUG(NE_DBG_HTTP, "Got chunk size: %ld\n", chunk_len);
	    if (chunk_len == 0) {
		/* Zero-size chunk == end of response. */
		NE_DEBUG(NE_DBG_HTTP, "Zero-size chunk.\n");
		*buflen = 0;
		return NE_OK;
	    }
	    resp->chunk_left = chunk_len;
	}
	willread = min(*buflen - 1, resp->chunk_left);
    } else if (resp->length > 0) {
	/* Have we finished reading the body? */
	if (resp->left == 0) {
	    *buflen = 0;
	    return NE_OK;
	}
	willread = min(*buflen - 1, resp->left);
    } else {
	/* Read until socket-close */
	willread = *buflen - 1;
    }
    NE_DEBUG(NE_DBG_HTTP, "Reading %d bytes of response body.\n", willread);
    readlen = sock_read(sock, buffer, willread);

    /* EOF is valid if we don't know the response body length, or
     * we've read all of the response body, and we're not using
     * chunked. */
    if (readlen == SOCK_CLOSED && resp->length <= 0 && !resp->is_chunked) {
	NE_DEBUG(NE_DBG_HTTP, "Got EOF.\n");
	readlen = 0;
    } else if (readlen < 0) {
	return set_sockerr(req, _("Could not read response body"), readlen);
    } else {
	NE_DEBUG(NE_DBG_HTTP, "Got %d bytes.\n", readlen);
    }
    buffer[readlen] = '\0';
    *buflen = readlen;
    NE_DEBUG(NE_DBG_HTTPBODY, "Read block:\n%s\n", buffer);
    if (resp->is_chunked) {
	resp->chunk_left -= readlen;
	if (resp->chunk_left == 0) {
	    char crlfbuf[2];
	    /* If we've read a whole chunk, read a CRLF */
	    readlen = sock_fullread(sock, crlfbuf, 2);
	    if (readlen < 0 || strncmp(crlfbuf, EOL, 2) != 0) {
		return set_sockerr(req, 
				   _("Error reading chunked response body"),
				   readlen);
	    }
	}
    } else if (resp->length > 0) {
	resp->left -= readlen;
    }
    return NE_OK;
}

ssize_t ne_read_response_block(ne_request *req, char *buffer, size_t buflen)
{
    struct body_reader *rdr;
    size_t readlen = buflen;

    if (req->resp.length == 0) {
	return 0;
    }

    if (read_response_block(req, &req->resp, buffer, &readlen)) {
	/* it failed. */
	req->forced_close = 1;	
	return -1;
    }

    if (req->session->progress_cb) {
	req->resp.total += readlen;
	req->session->progress_cb(req->session->progress_ud, req->resp.total, 
				  req->resp.is_chunked?-1:req->resp.length);
    }

    /* TODO: call the readers when this fails too. */
    for (rdr = req->body_readers; rdr!=NULL; rdr=rdr->next) {
	if (rdr->use)
	    (*rdr->handler)(rdr->userdata, buffer, readlen);
    }
    
    return readlen;
}

/* Build the request. */
static const char *build_request(ne_request *req) 
{
    const char *uri;
    struct hook_request *st;
    ne_buffer *buf = req->reqbuf;

    /* If we are talking to a proxy, we send them the absoluteURI
     * as the Request-URI. If we are talking to a server, we just 
     * send abs_path. */
    if (req->use_proxy)
	uri = req->uri;
    else
	uri = req->abs_path;
    
    ne_buffer_clear(buf);

    /* Add in the request and the user-supplied headers */
    ne_buffer_concat(buf, req->method, " ", uri, " HTTP/1.1" EOL,
		   req->headers->data, NULL);
    
    /* And the all-important Host header.  This is done here since it
     * might change for a new server. */
    ne_buffer_concat(buf, "Host: ", req->session->server.hostport, 
		   EOL, NULL);

    
    /* Now handle the body. */
    req->use_expect100 = 0;
    if ((req->session->expect100_works > -1) &&
	(req->body_size > HTTP_EXPECT_MINSIZE) && 
	!ne_version_pre_http11(req->session)) {
	/* Add Expect: 100-continue. */
	ne_buffer_zappend(buf, "Expect: 100-continue" EOL);
	req->use_expect100 = 1;
    }

    NE_DEBUG(NE_DBG_HTTP, "Running pre_send hooks\n");
    for (st = req->hook_store; st!=NULL; st = st->next) {
	if (HAVE_HOOK(st,pre_send)) {
	    HOOK_FUNC(st,pre_send)(st->cookie, buf);
	}
    }						    
    
    /* Final CRLF */
    ne_buffer_zappend(buf, EOL);

    return buf->data;
}

/* FIXME: this function need re-writing.
 *
 * Returns HTTP_*, and sets session error appropriately.
 */
static int send_request(ne_request *req)
{
    ne_session *sess = req->session;
    int ret, try_again, send_attempt;
    const char *request = build_request(req);
    char *buffer = req->respbuf->data;

    try_again = 1;
    
    do {

	/* FIXME: this is broken */
	try_again--;

#ifdef DEBUGGING
	{ 
	    if ((NE_DBG_HTTPPLAIN&ne_debug_mask) == NE_DBG_HTTPPLAIN) { 
		/* Display everything mode */
		NE_DEBUG(NE_DBG_HTTP, "Sending request headers:\n%s", request);
	    } else {
		/* Blank out the Authorization paramaters */
		char *reqdebug = ne_strdup(request), *pnt = reqdebug;
		while ((pnt = strstr(pnt, "Authorization: ")) != NULL) {
		    for (pnt += 15; *pnt != '\r' && *pnt != '\0'; pnt++) {
			*pnt = 'x';
		    }
		}
		NE_DEBUG(NE_DBG_HTTP, "Sending request headers:\n%s", reqdebug);
		free(reqdebug);
	    }
	}
#endif /* DEBUGGING */
	
	/* Send the Request-Line and headers */
	for (send_attempt = 0; send_attempt < 2; send_attempt++) {
	    NE_DEBUG(NE_DBG_HTTP, "Sending headers: attempt %d\n", send_attempt);
	    /* Open the connection if necessary */
	    ret = open_connection(req);
	    if (ret != NE_OK) {
		return ret;
	    }
	    ret = sock_send_string(req->session->socket, request);
	    if (ret == SOCK_CLOSED) {
		/* Could happen due to a persistent connection timeout.
		 * Or the server being restarted. */
		NE_DEBUG(NE_DBG_HTTP, "Connection was closed by server.\n");
		ne_close_connection(req->session);
	    } else {
		break;
	    }
	}
	
	if (ret < 0) {
	    return set_sockerr(req, _("Could not send request"), ret);
	}

	NE_DEBUG(NE_DBG_HTTP, "Request sent\n");
	
	/* Now, if we are doing a Expect: 100, hang around for a short
	 * amount of time, to see if the server actually cares about the 
	 * Expect and sends us a 100 Continue response if the request
	 * is valid, else an error code if it's not. This saves sending
	 * big files to the server when they will be rejected.
	 */
	
	if (req->use_expect100) {
	    NE_DEBUG(NE_DBG_HTTP, "Waiting for response...\n");
	    ret = sock_block(sess->socket, HTTP_EXPECT_TIMEOUT);
	    switch(ret) {
	    case SOCK_TIMEOUT: 
		/* Timed out - i.e. Expect: ignored. There is a danger
		 * here that the server DOES respect the Expect: header,
		 * but was going SO slowly that it didn't get time to
		 * respond within HTTP_EXPECT_TIMEOUT.
		 * TODO: while sending the body, check to see if the
		 * server has sent anything back - if it HAS, then
		 * stop sending - this is a spec compliance SHOULD */
		NE_DEBUG(NE_DBG_HTTP, "Wait timed out.\n");
		sess->expect100_works = -1; /* don't try that again */
		/* Try sending the request again without using 100-continue */
		try_again++;
		continue;
		break;
	    case SOCK_CLOSED:
	    case SOCK_ERROR: /* error */
		return set_sockerr(req, _("Error waiting for response"), ret);
	    default:
		NE_DEBUG(NE_DBG_HTTP, "Wait got data.\n");
		sess->expect100_works = 1; /* it works - use it again */
		break;
	    }
	} else if (req->body_size) {
	    /* Just chuck the file down the socket */
	    NE_DEBUG(NE_DBG_HTTP, "Sending body...\n");
	    ret = send_request_body(req);
	    if (ret == SOCK_CLOSED) {
		/* This happens if the persistent connection times out:
		 * the first write() of the headers gets a delayed write
		 * seemingly, so the write doesn't fail till this one.
		 */
		NE_DEBUG(NE_DBG_HTTP, "Connection closed before request sent, retrying\n");
		try_again++;
		ne_close_connection(req->session);
		continue;
	    } else if (ret < 0) {
		NE_DEBUG(NE_DBG_HTTP, "Body send failed.\n");
		return ret;
	    }
	    NE_DEBUG(NE_DBG_HTTP, "Body sent.\n");
	    
	}
	
	/* Now, we have either:
	 *   - Sent the header and body, or
	 *   - Sent the header incl. Expect: line, and got some response.
	 * In any case, we get the status line of the response.
	 */
	
	/* HTTP/1.1 says that the server MAY emit any number of
	 * interim 100 (Continue) responses prior to the normal
	 * response.  So loop while we get them.  */
	
	do {
	    if (sock_readline(sess->socket, buffer, BUFSIZ) <= 0) {
		if (try_again) {
		    return set_sockerr(req, _("Could not read status line"), ret);
		}
		NE_DEBUG(NE_DBG_HTTP, "Failed to read status line.\n");
		try_again++;
		break;
	    }

	    NE_DEBUG(NE_DBG_HTTP, "[Status Line] < %s", buffer);
	    
	    /* Got the status line - parse it */
	    if (ne_parse_statusline(buffer, &req->status)) {
		ne_set_error(sess, _("Could not parse response status line."));
		return -1;
	    }

	    sess->version_major = req->status.major_version;
	    sess->version_minor = req->status.minor_version;
	    snprintf(sess->error, BUFSIZ, "%d %s", 
		     req->status.code, req->status.reason_phrase);
	    STRIP_EOL(sess->error);

	    if (req->status.klass == 1) {
		NE_DEBUG(NE_DBG_HTTP, "Got 1xx-class.\n");
		/* Skip any headers, we don't need them */
		do {
		    ret = sock_readline(sess->socket, buffer, BUFSIZ);
		    if (ret <= 0) {
			return set_sockerr(
			    req, _("Error reading response headers"), ret);
		    }
		    NE_DEBUG(NE_DBG_HTTP, "[Ignored header] < %s", buffer);
		} while (strcmp(buffer, EOL) != 0);
	
		if (req->use_expect100 && (req->status.code == 100)) {
		    /* We are using Expect: 100, and we got a 100-continue 
		     * return code... send the request body */
		    NE_DEBUG(NE_DBG_HTTP, "Got continue... sending body now.\n");
		    HTTP_ERR(send_request_body(req));
		    NE_DEBUG(NE_DBG_HTTP, "Body sent.\n");
		} else if (req->upgrade_to_tls && (req->status.code == 101)) {
		    /* Switch to TLS on the fly */
		    if (sock_make_secure(sess->socket, sess->ssl_context)) {
			set_sockerr(req, _("Could not negotiate SSL session"), 
				    SOCK_ERROR);
			ne_close_connection(sess);
			return NE_ERROR;
		    }
		}
	    }
	} while (req->status.klass == 1);

	if (try_again == 1) {
	    /* If we're trying again, close the conn first */
	    NE_DEBUG(NE_DBG_HTTP, "Retrying request, closing connection first.\n");
	    ne_close_connection(sess);
	}

    } while (try_again == 1);

    return NE_OK;
}

/* Read a message header from sock into buf, which has size 'buflen'.
 *
 * Returns:
 *   NE_RETRY: Success, read a header into buf.
 *   NE_OK: End of headers reached.
 *   NE_ERROR: Error (session error is set).
 */
static int read_message_header(ne_request *req, char *buf, size_t buflen)
{
    int ret, n;
    size_t len; /* holds strlen(buf) */
    nsocket *sock = req->session->socket;

    n = sock_readline(sock, buf, buflen);
    if (n <= 0)
	return set_sockerr(req, _("Error reading response headers"), n);
    NE_DEBUG(NE_DBG_HTTP, "[hdr] %s", buf);

    STRIP_EOL(buf);

    len = strlen(buf);

    if (len == 0) {
	NE_DEBUG(NE_DBG_HTTP, "End of headers.\n");
	return NE_OK;
    }

    while (buflen > 0) {
	char ch;

	/* Collect any extra lines into buffer */
	ret = sock_peek(sock, &ch, 1);
	if (ret <= 0) {
	    /* FIXME: can sock_peek return 0? */
	    return set_sockerr(req, _("Error reading response headers"), ret);
	}
	if (ch != ' ' && ch != '\t') {
	    /* No continuation of this header. NUL-terminate to be paranoid. */
	    return NE_RETRY;
	}

	/* Otherwise, read the next line onto the end of 'buf'. */
	buf += len;
	buflen -= len;

	/* Read BUFSIZ-1 bytes to guarantee that we have a \0 */
	ret = sock_readline(sock, buf, buflen);
	if (ret <= 0) {
	    return set_sockerr(req, _("Error reading response headers"), ret);
	}

	NE_DEBUG(NE_DBG_HTTP, "[cont] %s", buf);

	STRIP_EOL(buf);
	len = strlen(buf);
	
	/* assert(buf[0] == ch), which implies len(buf) > 0.
	 * Otherwise the TCP stack is lying, but we'll be paranoid.
	 * This might be a \t, so replace it with a space to be
	 * friendly to applications (2616 says we MAY do this). */
	if (len) buf[0] = ' ';
    }

    ne_set_error(req->session, _("Response header too long"));
    return NE_ERROR;
}

static void normalize_response_length(ne_request *req)
{
    /* Response entity-body length calculation, bit icky.
     * Here, we set:
     * length==-1 if we DO NOT know the exact body length
     * length>=0 if we DO know the body length.
     *
     * RFC2616, section 4.4: 
     * NO body is returned if the method is HEAD, or the resp status
     * is 204 or 304
     */
    if (req->method_is_head || req->status.code==204 || 
	req->status.code==304) {
	req->resp.length = 0;
    } else {
	/* RFC2616, section 4.4: if we have a transfer encoding
	 * and a content-length, then ignore the content-length. */
	if ((req->resp.length>-1) && 
	    (req->resp.is_chunked)) {
	    req->resp.length = -1;
	}
    }
    /* Noddy noddy noddy. Testing from Apache/mod_proxy, CONNECT does
     * not return a Content-Length... */
    if (req->resp.length == -1 && req->session->in_connect &&
	req->status.klass == 2) {
	req->resp.length = 0;
    }
       
}

/* Apache's default is 100, seems reasonable. */
#define MAX_HEADER_FIELDS 100

#define MAX_HEADER_LENGTH 8192

/* Read response headers, using buffer buffer.
 * Returns NE_* code, sets session error. */
static int read_response_headers(ne_request *req) 
{
    char hdr[MAX_HEADER_LENGTH] = {0};
    int ret, count = 0;
    
    /* Read response headers.  This loop was once optimized so that
     * GCC will put all the local vars in registers, but that was a
     * long time ago. */
    while ((ret = read_message_header(req, hdr, MAX_HEADER_LENGTH)) == NE_RETRY 
	   && ++count < MAX_HEADER_FIELDS) {
	struct header_handler *hdl;
	/* hint to the compiler that we'd like these in registers */
	register char *pnt;
	register int hash = 0;
	
	for (hdl = req->header_catchers; hdl != NULL; hdl = hdl->next) {
	    (*hdl->handler)(hdl->userdata, hdr);
	}
	
	/* Iterate over the header name, converting it to lower case and 
	 * calculating the hash value as we go. */
	for (pnt = hdr; *pnt != '\0' && *pnt != ':'; pnt++) {
	    *pnt = tolower(*pnt);
	    hash = HH_ITERATE(hash,*pnt);
	}

	if (*pnt != '\0') {
	    /* Null-term name at the : */
	    *pnt = '\0';
	    
	    /* Value starts after any whitespace... */
	    do {
		pnt++;
	    } while (*pnt == ' ' || *pnt == '\t');
	    
	    NE_DEBUG(NE_DBG_HTTP, "Header Name: [%s], Value: [%s]\n", hdr, pnt);
	    
	    /* Iterate through the header handlers */
	    for (hdl = req->header_handlers[hash]; hdl != NULL; 
		 hdl = hdl->next) {
		if (strcmp(hdr, hdl->name) == 0) {
		    (*hdl->handler)(hdl->userdata, pnt);
		}
	    }
	} else {
	    ne_set_error(req->session, _("Malformed header line."));
	    return NE_ERROR;
	}
    }

    if (count == MAX_HEADER_FIELDS) {
	ne_set_error(req->session, 
		       _("Response exceeded maximum number of header fields."));
	ret = NE_ERROR;
    }

    return ret;
}

int ne_begin_request(ne_request *req)
{
    struct body_reader *rdr;

    NE_DEBUG(NE_DBG_HTTPBASIC, "Request: %s %s\n", req->method, req->uri);

    req->can_persist = 0;
    req->forced_close = 0;
    req->resp.length = -1;
    req->resp.is_chunked = 0;
    
    /* Now send the request, and read the Status-Line */
    HTTP_ERR(send_request(req));
    
    /* Read the headers */
    HTTP_ERR(read_response_headers(req));

    /* response message logic */
    normalize_response_length(req);

    /* Prepare for reading the response entity-body.  call each of the
     * body readers and ask them whether they want to accept this
     * response or not. */
    for (rdr = req->body_readers; rdr != NULL; rdr=rdr->next) {
	rdr->use = (*rdr->accept_response)(rdr->userdata, req, &req->status);
    }
    
    req->resp.left = req->resp.length;
    req->resp.chunk_left = 0;

    return NE_OK;
}

int ne_end_request(ne_request *req)
{
    struct hook_request *st;
    int ret = NE_OK;

    NE_DEBUG(NE_DBG_HTTPBASIC, "Response: %s\n", req->session->error);

    /* Read headers in chunked trailers */
    if (req->resp.is_chunked) {
	HTTP_ERR(read_response_headers(req));
    }
    
    NE_DEBUG(NE_DBG_HTTP, "Running post_send hooks\n");
    for (st = req->hook_store; ret == NE_OK && st != NULL; st = st->next) {
	if (HAVE_HOOK(st,post_send)) {
	    ret = HOOK_FUNC(st,post_send)(st->cookie, &req->status);
	}
    }
    
    NE_DEBUG(NE_DBG_HTTP, "Connection status: %s, %s, %s\n",
	  req->forced_close?"forced close":"no forced close",
	  req->session->no_persist?"no persistent connection":"persistent connection",
	  ne_version_pre_http11(req->session)?"pre-HTTP/1.1":"HTTP/1.1 or later");
    
    /* Close the connection if any of the following are true:
     *  - We have a forced close (e.g. "Connection: close" header)
     *  - We are not using persistent connections for this session
     *  - All of the following are true:
     *    * this is HTTP/1.0
     *    * and they haven't said they can do persistent connections 
     *    * we've not just done a successful CONNECT
     */
    if (req->forced_close || req->session->no_persist ||
	(ne_version_pre_http11(req->session) && 
	 !req->can_persist && 
	 (!req->session->in_connect || req->status.klass != 2))) {
	ne_close_connection(req->session);
    }
    
    /* Retry if any hook asked us too (i.e. authentication hooks) */

    return ret;
}

/* HTTP/1.x request/response mechanism 
 *
 * Returns an NE_* return code. 
 *   
 * The status information is placed in status. The error string is
 * placed in req->session->error
 */
int ne_request_dispatch(ne_request *req) 
{
    int ret;
    ssize_t len;

    /* Loop sending the request:
     * Retry whilst authentication fails and we supply it. */
    
    do {
	char buffer[BUFSIZ];
	
	HTTP_ERR(ne_begin_request(req));
	
	do {
	    len = ne_read_response_block(req, buffer, BUFSIZ);
	} while (len > 0);

	if (len < 0) {
	    return NE_ERROR;
	}

	ret = ne_end_request(req);

    } while (ret == NE_RETRY);

    NE_DEBUG(NE_DBG_HTTP | NE_DBG_FLUSH, 
	   "Request ends, status %d class %dxx, error line:\n%s\n", 
	   req->status.code, req->status.klass, req->session->error);

    return ret;
}

const ne_status *ne_get_status(ne_request *req)
{
    return &(req->status);
}

/* Create a CONNECT tunnel through the proxy server.
 * Returns HTTP_* */
static int proxy_tunnel(ne_session *sess)
{
    /* Hack up an HTTP CONNECT request... */
    ne_request *req = ne_request_create(sess, "CONNECT", NULL);
    int ret = NE_OK;

    /* Fudge the URI to be how we want it */
    req->uri = ne_strdup(sess->server.hostport);

    sess->connected = 1;
    sess->in_connect = 1;

    ret = ne_request_dispatch(req);

    sess->in_connect = 0;

    if (ret != NE_OK || !sess->connected || 
	req->status.klass != 2) {
	/* It failed */
	ne_set_error(sess, 
		       _("Could not create SSL connection through proxy server"));
	ret = NE_ERROR;
    }

    ne_request_destroy(req);
    
    return ret;
}

static int open_connection(ne_request *req) 
{
    ne_session *sess = req->session;

    if (req->use_proxy) {
	switch(sess->connected) {
	case 0:
	    /* Make the TCP connection to the proxy */
	    NE_DEBUG(NE_DBG_SOCKET, "Connecting to proxy at %s:%d...\n", 
		   sess->proxy.hostname, sess->proxy.port);
	    notify_status(sess, ne_conn_connecting, sess->proxy.hostport);
	    sess->socket = sock_connect(sess->proxy.addr, sess->proxy.port);
	    if (sess->socket == NULL) {
		(void) set_sockerr(req, _("Could not connect to proxy server"), SOCK_ERROR);
		return NE_CONNECT;
	    }
	    
	    notify_status(sess, ne_conn_connected, sess->proxy.hostport);

	    if (sess->progress_cb) {
		sock_register_progress(sess->socket, sess->progress_cb,
				       sess->progress_ud);
	    }
	    sess->connected = 1;
	    /* FALL-THROUGH */
	case 1:
	    if (sess->use_secure && !sess->in_connect) {
		int ret;
		ret = proxy_tunnel(sess);
		if (ret != NE_OK) {
		    ne_close_connection(sess);
		    return ret;
		}
		if (sock_make_secure(sess->socket, sess->ssl_context)) {
		    (void) set_sockerr(req, _("Could not negotiate SSL session"), SOCK_ERROR);
		    ne_close_connection(sess);
		    return NE_CONNECT;
		}
		sess->connected = 2;
		notify_status(sess, ne_conn_secure, 
			   sock_get_version(sess->socket));
	    } else {
		break;
	    }
	    break;
	default:
	    /* We've got everything we need */
	    break;	    
	}
    } else if (sess->connected == 0) {

	NE_DEBUG(NE_DBG_SOCKET, "Connecting to server at %s:%d...\n", 
	       sess->server.hostname, sess->server.port);

	notify_status(sess, ne_conn_connecting, sess->server.hostport);
	sess->socket = sock_connect(sess->server.addr, sess->server.port);
	    
	if (sess->socket == NULL) {
	    (void) set_sockerr(req, _("Could not connect to server"), -1);
	    return NE_CONNECT;
	}

	notify_status(sess, ne_conn_connected, sess->server.hostport);

	if (sess->progress_cb) {
	    sock_register_progress(sess->socket, sess->progress_cb,
				   sess->progress_ud);
	}

	if (sess->use_secure) {
	    NE_DEBUG(NE_DBG_SOCKET, "Starting SSL...\n");
	    if (sock_make_secure(sess->socket, sess->ssl_context)) {
		(void) set_sockerr(req, _("Could not negotiate SSL session"), SOCK_ERROR);
		ne_close_connection(sess);
		return NE_CONNECT;
	    }

	    notify_status(sess, ne_conn_secure, sock_get_version(sess->socket));

	}

	sess->connected = 1;
    }
    return NE_OK;
}
