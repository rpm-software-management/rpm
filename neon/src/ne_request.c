/* 
   HTTP request/response handling
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

/* This is the HTTP client request/response implementation.
 * The goal of this code is to be modular and simple.
 */

#include "config.h"

#include <sys/types.h>

#ifdef HAVE_LIMITS_H
#include <limits.h> /* for UINT_MAX etc */
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
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
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

#define SOCK_ERR(req, op, msg) do { ssize_t sret = (op); \
if (sret < 0) return aborted(req, msg, sret); } while (0)

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

#ifdef NE_LFS
#define ne_lseek lseek64
typedef off64_t ne_off_t;
#define FMT_NE_OFF_T NE_FMT_OFF64_T
#if defined(LONG_LONG_MAX) && !defined(LLONG_MAX)
#define LLONG_MAX LONG_LONG_MAX
#endif
#define NE_OFFT_MAX LLONG_MAX
#ifdef HAVE_STRTOLL
#define ne_strtoff strtoll
#else
#define ne_strtoff strtoq
#endif
#else /* !NE_LFS */
typedef off_t ne_off_t;
#define ne_lseek lseek
#define FMT_NE_OFF_T NE_FMT_OFF_T
#define NE_OFFT_MAX LONG_MAX
#if SIZEOF_OFF_T > SIZEOF_LONG && defined(HAVE_STRTOLL)
#define ne_strtoff strtoll
#elif SIZEOF_OFF_T > SIZEOF_LONG && defined(HAVE_STRTOQ)
#define ne_strtoff strtoq
#else
#define ne_strtoff strtol
#endif
#endif /* NE_LFS */

struct ne_request_s {
    char *method, *uri; /* method and Request-URI */

    ne_buffer *headers; /* request headers */

    /* Request body. */
    ne_provide_body body_cb;
    void *body_ud;

    /* Request body source: file or buffer (if not callback). */
    union {
        struct {
            int fd;
            ne_off_t offset, length;
            ne_off_t remain; /* remaining bytes to send. */
        } file;
	struct {
            /* length bytes @ buffer = whole body.
             * remain bytes @ pnt = remaining bytes to send */
	    const char *buffer, *pnt;
	    size_t length, remain;
	} buf;
    } body;
	    
    ne_off_t body_length; /* length of request body */
    ne_off_t body_progress; /* number of bytes of body sent so far */

    /* temporary store for response lines. */
    char respbuf[BUFSIZ];

    /**** Response ***/

    /* The transfer encoding types */
    struct ne_response {
	enum {
	    R_TILLEOF = 0, /* read till eof */
	    R_NO_BODY, /* implicitly no body (HEAD, 204, 304) */
	    R_CHUNKED, /* using chunked transfer-encoding */
	    R_CLENGTH  /* using given content-length */
	} mode;
        union {
            /* clen: used if mode == R_CLENGTH; total and bytes
             * remaining to be read of response body. */
            struct {
                ne_off_t total, remain;
            } clen;
            /* chunk: used if mode == R_CHUNKED; total and bytes
             * remaining to be read of current chunk */
            struct {
                size_t total, remain;
            } chunk;
        } body;
        ne_off_t progress; /* number of bytes read of response */
    } resp;

    /* List of callbacks which are passed response headers */
    struct header_handler *header_catchers;
    
    struct hook *private;

    /* We store response header handlers in a hash table.  The hash is
     * derived from the header name in lower case. */

    /* FIXME: this comment is WAY out of date, and no longer in any
     * way true. */
	    
    /* 53 is magic, of course.  For a standard ne_get() (with
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
    unsigned int use_expect100:1;
    unsigned int can_persist:1;

    ne_session *session;
    ne_status status;
};

static int open_connection(ne_request *req)
	/*@globals internalState @*/
	/*@modifies req, internalState @*/;

/* The iterative step used to produce the hash value.  This is DJB's
 * magic "*33" hash function.  Ralf Engelschall has done some amazing
 * statistical analysis to show that *33 really is a good hash
 * function: check the new-httpd list archives, or his 'str' library
 * source code, for the details.
 *
 * TODO: due to limited range of characters used in header names,
 * could maybe get a better hash function to use? */
 
#define HH_ITERATE(hash, ch) (((hash)*33 + (ch)) % HH_HASHSIZE);

/* Returns hash value for header 'name', converting it to lower-case
 * in-place. */
static inline unsigned int hash_and_lower(char *name)
	/*@modifies name @*/
{
    char *pnt;
    unsigned int hash = 0;

    for (pnt = name; *pnt != '\0'; pnt++) {
	*pnt = tolower(*pnt);
	hash = HH_ITERATE(hash,*pnt);
    }

    return hash;
}

/* Abort a request due to an non-recoverable HTTP protocol error,
 * whilst doing 'doing'.  'code', if non-zero, is the socket error
 * code, NE_SOCK_*, or if zero, is ignored. */
static int aborted(ne_request *req, const char *doing, ssize_t code)
	/*@globals internalState @*/
	/*@modifies req, internalState @*/
{
    ne_session *sess = req->session;
    int ret = NE_ERROR;

    NE_DEBUG(NE_DBG_HTTP, "Aborted request (%" NE_FMT_SSIZE_T "): %s\n",
	     code, doing);

    switch(code) {
    case NE_SOCK_CLOSED:
	if (sess->use_proxy) {
	    ne_set_error(sess, _("%s: connection was closed by proxy server."),
			 doing);
	} else {
	    ne_set_error(sess, _("%s: connection was closed by server."),
			 doing);
	}
	break;
    case NE_SOCK_TIMEOUT:
	ne_set_error(sess, _("%s: connection timed out."), doing);
	ret = NE_TIMEOUT;
	break;
    case NE_SOCK_ERROR:
    case NE_SOCK_RESET:
    case NE_SOCK_TRUNC:
        ne_set_error(sess, "%s: %s", doing, ne_sock_error(sess->socket));
        break;
    case 0:
	ne_set_error(sess, "%s", doing);
	break;
    }

    ne_close_connection(sess);
    return ret;
}

static void notify_status(ne_session *sess, ne_conn_status status,
			  const char *info)
	/*@*/
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

/*@null@*/
static void *get_private(const struct hook *hk, const char *id)
	/*@*/
{
    for (; hk != NULL; hk = hk->next)
	if (strcmp(hk->id, id) == 0)
	    return hk->userdata;
    return NULL;
}

void *ne_get_request_private(ne_request *req, const char *id)
{
    return get_private(req->private, id);
}

void *ne_get_session_private(ne_session *sess, const char *id)
{
    return get_private(sess->private, id);
}

typedef void (*void_fn)(void);

#define ADD_HOOK(hooks, fn, ud) add_hook(&(hooks), NULL, (void_fn)(fn), (ud))

static void add_hook(struct hook **hooks, const char *id, void_fn fn, void *ud)
	/*@modifies *hooks @*/
{
    struct hook *hk = ne_malloc(sizeof (struct hook)), *pos;

    if (*hooks != NULL) {
	for (pos = *hooks; pos->next != NULL; pos = pos->next)
	    /* nullop */;
	pos->next = hk;
    } else {
	*hooks = hk;
    }

    hk->id = id;
    hk->fn = fn;
    hk->userdata = ud;
    hk->next = NULL;
}

void ne_hook_create_request(ne_session *sess, 
			    ne_create_request_fn fn, void *userdata)
{
    ADD_HOOK(sess->create_req_hooks, fn, userdata);
}

void ne_hook_pre_send(ne_session *sess, ne_pre_send_fn fn, void *userdata)
{
    ADD_HOOK(sess->pre_send_hooks, fn, userdata);
}

void ne_hook_post_send(ne_session *sess, ne_post_send_fn fn, void *userdata)
{
    ADD_HOOK(sess->post_send_hooks, fn, userdata);
}

void ne_hook_destroy_request(ne_session *sess,
			     ne_destroy_req_fn fn, void *userdata)
{
    ADD_HOOK(sess->destroy_req_hooks, fn, userdata);    
}

void ne_hook_destroy_session(ne_session *sess,
			     ne_destroy_sess_fn fn, void *userdata)
{
    ADD_HOOK(sess->destroy_sess_hooks, fn, userdata);
}

void ne_set_session_private(ne_session *sess, const char *id, void *userdata)
{
    add_hook(&sess->private, id, NULL, userdata);
}

void ne_set_request_private(ne_request *req, const char *id, void *userdata)
{
    add_hook(&req->private, id, NULL, userdata);
}

static ssize_t body_string_send(void *userdata, char *buffer, size_t count)
	/*@modifies buffer @*/
{
    ne_request *req = userdata;
    
    if (count == 0) {
	req->body.buf.remain = req->body.buf.length;
	req->body.buf.pnt = req->body.buf.buffer;
    } else {
	/* if body_left == 0 we fall through and return 0. */
	if (req->body.buf.remain < count)
	    count = req->body.buf.remain;

	memcpy(buffer, req->body.buf.pnt, count);
	req->body.buf.pnt += count;
	req->body.buf.remain -= count;
    }

    return count;
}    

static ssize_t body_fd_send(void *userdata, char *buffer, size_t count)
	/*@modifies buffer @*/
{
    ne_request *req = userdata;

    if (count) {
        if (req->body.file.remain == 0)
            return 0;
        if ((off_t)count > req->body.file.remain)
            count = req->body.file.remain;
	return read(req->body.file.fd, buffer, count);
    } else {
        /* rewind for next send. */
        if (ne_lseek(req->body.file.fd, req->body.file.offset, SEEK_SET)
            == req->body.file.offset) {
            req->body.file.remain = req->body.file.length;
            return 0;
        } else {
            return -1;
        }
    }
}

/* Pulls the request body from the source and pushes it to the given
 * callback.  Returns 0 on success, or NE_* code */
int ne__pull_request_body(ne_request *req, ne_push_fn fn, void *ud)
{
    int ret = 0;
    char buffer[BUFSIZ];
    ssize_t bytes;
    
    /* tell the source to start again from the beginning. */
    (void) req->body_cb(req->body_ud, NULL, 0);
    
    /* TODO: should this attempt to pull exactly the number of bytes
     * they specified were in the body? Currently it just pulls until
     * they return zero. That makes it possible to extend this to do
     * chunked request bodies (i.e. indefinitely long, no C-L), so
     * this is probably a better long-term interface. */
    while ((bytes = req->body_cb(req->body_ud, buffer, sizeof buffer)) > 0) {
	ret = fn(ud, buffer, bytes);
	if (ret < 0)
	    break;
	NE_DEBUG(NE_DBG_HTTPBODY, 
		 "Body block (%" NE_FMT_SSIZE_T " bytes):\n[%.*s]\n",
		 bytes, (int)bytes, buffer);
    }

    if (bytes < 0) {
	ne_set_error(req->session, _("Error reading request body."));
	ret = NE_ERROR;
    }

    return ret;
}

static int send_with_progress(void *userdata, const char *data, size_t n)
	/*@*/
{
    ne_request *req = userdata;
    int ret;
    
    ret = ne_sock_fullwrite(req->session->socket, data, n);
    if (ret == 0) {
	req->body_progress += n;
	req->session->progress_cb(req->session->progress_ud,
				  req->body_progress, req->body_length);
    }
    
    return ret;    
}

/* Sends the request body down the socket.
 * Returns 0 on success, or NE_* code */
static int send_request_body(ne_request *req)
	/*@modifies req @*/
{
    int ret; 
    NE_DEBUG(NE_DBG_HTTP, "Sending request body...\n");
    if (req->session->progress_cb) {
	/* with progress callbacks. */
	req->body_progress = 0;
	ret = ne__pull_request_body(req, send_with_progress, req);
    } else {
	/* without progress callbacks. */
	ret = ne__pull_request_body(req, (ne_push_fn)ne_sock_fullwrite,
				   req->session->socket);
    }

    NE_DEBUG(NE_DBG_HTTP, "Request body sent: %s.\n", ret?"failed":"okay");
    return ret;
}

/* Lob the User-Agent, connection and host headers in to the request
 * headers */
static void add_fixed_headers(ne_request *req) 
	/*@modifies req @*/
{
    if (req->session->user_agent) {
        ne_buffer_zappend(req->headers, req->session->user_agent);
    }

    /* Send Connection: Keep-Alive to pre-1.1 origin servers to try
     * harder to get a persistent connection, except if using a proxy
     * as per 2068 sec 19.7.1.  Always add TE: trailers since those
     * are understood. */
    if (!req->session->no_persist && !req->session->is_http11
        && !req->session->use_proxy) {
	ne_buffer_zappend(req->headers, 
                          "Keep-Alive: " EOL
                          "Connection: TE, Keep-Alive" EOL
                          "TE: trailers" EOL);
    } else {
	ne_buffer_zappend(req->headers, 
                          "Connection: TE" EOL
                          "TE: trailers" EOL);
    }

}

int ne_accept_always(void *userdata, ne_request *req, const ne_status *st)
{
    return 1;
}				   

int ne_accept_2xx(void *userdata, ne_request *req, const ne_status *st)
{
    return (st->klass == 2);
}

/* Handler for the "Transfer-Encoding" response header: treat *any*
 * such header as implying a chunked response, per the "Protocol
 * Compliance" statement in the manual. */
static void te_hdr_handler(void *userdata, const char *value) 
	/*@*/
{
    struct ne_response *resp = userdata;

    resp->mode = R_CHUNKED;
    resp->body.chunk.remain = 0;
}

/* Handler for the "Connection" response header */
static void connection_hdr_handler(void *userdata, const char *value)
	/*@*/
{
    ne_request *req = userdata;
    if (strcasecmp(value, "close") == 0) {
	req->can_persist = 0;
    } else if (strcasecmp(value, "Keep-Alive") == 0) {
	req->can_persist = 1;
    }
}

static void clength_hdr_handler(void *userdata, const char *value)
	/*@*/
{
    struct ne_response *resp = userdata;
    ne_off_t len = ne_strtoff(value, NULL, 10);
    if (len != NE_OFFT_MAX && len >= 0 && resp->mode == R_TILLEOF) {
	resp->mode = R_CLENGTH;
	resp->body.clen.total = resp->body.clen.remain = len;
    }
}

ne_request *ne_request_create(ne_session *sess,
			      const char *method, const char *path) 
{
    ne_request *req = ne_calloc(sizeof *req);

    NE_DEBUG(NE_DBG_HTTP, "Creating request...\n");

    req->session = sess;
    req->headers = ne_buffer_create();

    /* Add in the fixed headers */
    add_fixed_headers(req);

    /* Set the standard stuff */
    req->method = ne_strdup(method);
    req->method_is_head = (strcmp(method, "HEAD") == 0);

    /* Add in handlers for all the standard HTTP headers. */
    ne_add_response_header_handler(req, "Content-Length", 
				   clength_hdr_handler, &req->resp);
    ne_add_response_header_handler(req, "Transfer-Encoding", 
				   te_hdr_handler, &req->resp);
    ne_add_response_header_handler(req, "Connection", 
				   connection_hdr_handler, req);

    /* Only use an absoluteURI here when absolutely necessary: some
     * servers can't parse them. */
    if (req->session->use_proxy && !req->session->use_ssl && path[0] == '/')
	req->uri = ne_concat(req->session->scheme, "://", 
			     req->session->server.hostport, path, NULL);
    else
	req->uri = ne_strdup(path);

    {
	struct hook *hk;

	NE_DEBUG(NE_DBG_HTTP, "Running request create hooks.\n");

	for (hk = sess->create_req_hooks; hk != NULL; hk = hk->next) {
	    ne_create_request_fn fn = (ne_create_request_fn)hk->fn;
	    fn(req, hk->userdata, method, req->uri);
	}
    }

    NE_DEBUG(NE_DBG_HTTP, "Request created.\n");

    return req;
}

/* Set the request body length to 'length' */
static void set_body_length(ne_request *req, ne_off_t length)
	/*@modifies req @*/
{
    req->body_length = length;
    ne_print_request_header(req, "Content-Length", "%" FMT_NE_OFF_T, length);
}

void ne_set_request_body_buffer(ne_request *req, const char *buffer,
				size_t size)
{
    req->body.buf.buffer = buffer;
    req->body.buf.length = size;
    req->body_cb = body_string_send;
    req->body_ud = req;
    set_body_length(req, size);
}

void ne_set_request_body_provider(ne_request *req, off_t bodysize,
				  ne_provide_body provider, void *ud)
{
    req->body_cb = provider;
    req->body_ud = ud;
    set_body_length(req, bodysize);
}

void ne_set_request_body_fd(ne_request *req, int fd,
                            off_t offset, off_t length)
{
    req->body.file.fd = fd;
    req->body.file.offset = offset;
    req->body.file.length = length;
    req->body_cb = body_fd_send;
    req->body_ud = req;
    set_body_length(req, length);
}

#ifdef NE_LFS
void ne_set_request_body_fd64(ne_request *req, int fd,
                              off64_t offset, off64_t length)
{
    req->body.file.fd = fd;
    req->body.file.offset = offset;
    req->body.file.length = length;
    req->body_cb = body_fd_send;
    req->body_ud = req;
    set_body_length(req, length);
}

void ne_set_request_body_provider64(ne_request *req, off64_t bodysize,
                                    ne_provide_body provider, void *ud)
{
    req->body_cb = provider;
    req->body_ud = ud;
    set_body_length(req, bodysize);
}
#endif

void ne_set_request_expect100(ne_request *req, int flag)
{
    req->use_expect100 = flag;
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
    ne_vsnprintf(buf, sizeof buf, format, params);
    va_end(params);
    
    ne_buffer_concat(req->headers, name, ": ", buf, EOL, NULL);
}

void
ne_add_response_header_handler(ne_request *req, const char *name, 
			       ne_header_handler hdl, void *userdata)
{
    struct header_handler *new = ne_calloc(sizeof *new);
    unsigned int hash;
    new->name = ne_strdup(name);
    new->handler = hdl;
    new->userdata = userdata;
    hash = hash_and_lower(new->name);
    new->next = req->header_handlers[hash];
    req->header_handlers[hash] = new;
}

void ne_add_response_header_catcher(ne_request *req, 
				    ne_header_handler hdl, void *userdata)
{
    struct header_handler *new = ne_calloc(sizeof *new);
    new->handler = hdl;
    new->userdata = userdata;
    new->next = req->header_catchers;
    req->header_catchers = new;
}

void ne_add_response_body_reader(ne_request *req, ne_accept_response acpt,
				 ne_block_reader rdr, void *userdata)
{
    struct body_reader *new = ne_malloc(sizeof *new);
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
    struct hook *hk, *next_hk;
    int n;

    ne_free(req->uri);
    ne_free(req->method);

    for (rdr = req->body_readers; rdr != NULL; rdr = next_rdr) {
	next_rdr = rdr->next;
	ne_free(rdr);
    }

    for (hdlr = req->header_catchers; hdlr != NULL; hdlr = next_hdlr) {
	next_hdlr = hdlr->next;
	ne_free(hdlr);
    }

    for (n = 0; n < HH_HASHSIZE; n++) {
	for (hdlr = req->header_handlers[n]; hdlr != NULL; 
	     hdlr = next_hdlr) {
	    next_hdlr = hdlr->next;
	    ne_free(hdlr->name);
	    ne_free(hdlr);
	}
    }

    ne_buffer_destroy(req->headers);

    NE_DEBUG(NE_DBG_HTTP, "Running destroy hooks.\n");
    for (hk = req->session->destroy_req_hooks; hk; hk = hk->next) {
	ne_destroy_req_fn fn = (ne_destroy_req_fn)hk->fn;
	fn(req, hk->userdata);
    }

    for (hk = req->private; hk; hk = next_hk) {
	next_hk = hk->next;
	ne_free(hk);
    }

    if (req->status.reason_phrase)
	ne_free(req->status.reason_phrase);

    NE_DEBUG(NE_DBG_HTTP, "Request ends.\n");
    ne_free(req);
}


/* Reads a block of the response into BUFFER, which is of size
 * *BUFLEN.  Returns zero on success or non-zero on error.  On
 * success, *BUFLEN is updated to be the number of bytes read into
 * BUFFER (which will be 0 to indicate the end of the repsonse).  On
 * error, the connection is closed and the session error string is
 * set.  */
static int read_response_block(ne_request *req, struct ne_response *resp, 
			       char *buffer, size_t *buflen) 
	/*@globals internalState @*/
	/*@modifies req, resp, buffer, *buflen, internalState @*/
{
    ne_socket *const sock = req->session->socket;
    size_t willread;
    ssize_t readlen;
    
    switch (resp->mode) {
    case R_CHUNKED:
        /* Chunked transfer-encoding: chunk syntax is "SIZE CRLF CHUNK
         * CRLF SIZE CRLF CHUNK CRLF ..." followed by zero-length
         * chunk: "CHUNK CRLF 0 CRLF".  resp.chunk.remain contains the
         * number of bytes left to read in the current chunk. */
	if (resp->body.chunk.remain == 0) {
	    unsigned long chunk_len;
	    char *ptr;

            /* The start of a new chunk. */
	    SOCK_ERR(req, ne_sock_readline(sock, buffer, *buflen),
		     _("Could not read chunk size"));
	    NE_DEBUG(NE_DBG_HTTP, "[chunk] < %s", buffer);
	    chunk_len = strtoul(buffer, &ptr, 16);
	    /* limit chunk size to <= UINT_MAX, so it will probably
	     * fit in a size_t. */
	    if (ptr == buffer || 
		chunk_len == ULONG_MAX || chunk_len > UINT_MAX) {
		return aborted(req, _("Could not parse chunk size"), 0);
	    }
	    NE_DEBUG(NE_DBG_HTTP, "Got chunk size: %lu\n", chunk_len);
	    resp->body.chunk.remain = chunk_len;
	}
	willread = resp->body.chunk.remain > *buflen
            ? *buflen : resp->body.chunk.remain;
	break;
    case R_CLENGTH:
	willread = resp->body.clen.remain > (off_t)*buflen 
            ? *buflen : (size_t)resp->body.clen.remain;
	break;
    case R_TILLEOF:
	willread = *buflen;
	break;
    case R_NO_BODY:
    default:
	willread = 0;
	break;
    }
    if (willread == 0) {
	*buflen = 0;
	return 0;
    }
    NE_DEBUG(NE_DBG_HTTP,
	     "Reading %" NE_FMT_SIZE_T " bytes of response body.\n", willread);
    readlen = ne_sock_read(sock, buffer, willread);

    /* EOF is only valid when response body is delimited by it.
     * Strictly, an SSL truncation should not be treated as an EOF in
     * any case, but SSL servers are just too buggy.  */
    if (resp->mode == R_TILLEOF && 
	(readlen == NE_SOCK_CLOSED || readlen == NE_SOCK_TRUNC)) {
	NE_DEBUG(NE_DBG_HTTP, "Got EOF.\n");
	req->can_persist = 0;
	readlen = 0;
    } else if (readlen < 0) {
	return aborted(req, _("Could not read response body"), readlen);
    } else {
	NE_DEBUG(NE_DBG_HTTP, "Got %" NE_FMT_SSIZE_T " bytes.\n", readlen);
    }
    /* safe to cast: readlen guaranteed to be >= 0 above */
    *buflen = (size_t)readlen;
    NE_DEBUG(NE_DBG_HTTPBODY,
	     "Read block (%" NE_FMT_SSIZE_T " bytes):\n[%.*s]\n",
	     readlen, (int)readlen, buffer);
    if (resp->mode == R_CHUNKED) {
	resp->body.chunk.remain -= readlen;
	if (resp->body.chunk.remain == 0) {
	    char crlfbuf[2];
	    /* If we've read a whole chunk, read a CRLF */
	    readlen = ne_sock_fullread(sock, crlfbuf, 2);
            if (readlen < 0)
                return aborted(req, _("Could not read chunk delimiter"),
                               readlen);
            else if (crlfbuf[0] != '\r' || crlfbuf[1] != '\n')
                return aborted(req, _("Chunk delimiter was invalid"), 0);
	}
    } else if (resp->mode == R_CLENGTH) {
	resp->body.clen.remain -= readlen;
    }
    resp->progress += readlen;
    return NE_OK;
}

ssize_t ne_read_response_block(ne_request *req, char *buffer, size_t buflen)
{
    struct body_reader *rdr;
    size_t readlen = buflen;
    struct ne_response *const resp = &req->resp;

    if (read_response_block(req, resp, buffer, &readlen))
	return -1;

    if (req->session->progress_cb) {
	req->session->progress_cb(req->session->progress_ud, resp->progress, 
				  resp->mode==R_CLENGTH ? resp->body.clen.total:-1);
    }

    for (rdr = req->body_readers; rdr!=NULL; rdr=rdr->next) {
	if (rdr->use && rdr->handler(rdr->userdata, buffer, readlen) != 0) {
            ne_close_connection(req->session);
            return -1;
        }
    }
    
    return readlen;
}

/* Build the request string, returning the buffer. */
static ne_buffer *build_request(ne_request *req) 
	/*@*/
{
    struct hook *hk;
    ne_buffer *buf = ne_buffer_create();

    /* Add Request-Line and Host header: */
    ne_buffer_concat(buf, req->method, " ", req->uri, " HTTP/1.1" EOL,
		     "Host: ", req->session->server.hostport, EOL, NULL);
    
    /* Add custom headers: */
    ne_buffer_append(buf, req->headers->data, ne_buffer_size(req->headers));

#define E100 "Expect: 100-continue" EOL
    if (req->use_expect100)
	ne_buffer_append(buf, E100, strlen(E100));

    NE_DEBUG(NE_DBG_HTTP, "Running pre_send hooks\n");
    for (hk = req->session->pre_send_hooks; hk!=NULL; hk = hk->next) {
	ne_pre_send_fn fn = (ne_pre_send_fn)hk->fn;
	fn(req, hk->userdata, buf);
    }
    
    ne_buffer_append(buf, "\r\n", 2);
    return buf;
}

#ifdef NE_DEBUGGING
#define DEBUG_DUMP_REQUEST(x) dump_request(x)

static void dump_request(const char *request)
	/*@*/
{ 
    if (ne_debug_mask & NE_DBG_HTTPPLAIN) { 
	/* Display everything mode */
	NE_DEBUG(NE_DBG_HTTP, "Sending request headers:\n%s", request);
    } else if (ne_debug_mask & NE_DBG_HTTP) {
	/* Blank out the Authorization paramaters */
	char *reqdebug = ne_strdup(request), *pnt = reqdebug;
	while ((pnt = strstr(pnt, "Authorization: ")) != NULL) {
	    for (pnt += 15; *pnt != '\r' && *pnt != '\0'; pnt++) {
		*pnt = 'x';
	    }
	}
	NE_DEBUG(NE_DBG_HTTP, "Sending request headers:\n%s", reqdebug);
	ne_free(reqdebug);
    }
}

#else
#define DEBUG_DUMP_REQUEST(x)
#endif /* DEBUGGING */

/* remove trailing EOL from 'buf', where strlen(buf) == *len.  *len is
 * adjusted in accordance with any changes made to the string to
 * remain equal to strlen(buf). */
static inline void strip_eol(char *buf, ssize_t *len)
	/*@modifies *len @*/
{
    char *pnt = &buf[*len-1];
    while (pnt >= buf && (*pnt == '\r' || *pnt == '\n')) {
	*pnt-- = '\0';
	(*len)--;
    }
}

/* For accurate persistent connection handling, for any write() or
 * read() operation for a new request on an already-open connection,
 * an EOF or RST error MUST be treated as a persistent connection
 * timeout, and the request retried on a new connection.  Once a
 * read() operation has succeeded, any subsequent error MUST be
 * treated as fatal.  A 'retry' flag is used; retry=1 represents the
 * first case, retry=0 the latter. */

/* RETRY_RET() crafts a function return value given the 'retry' flag,
 * the socket error 'code', and the return value 'acode' from the
 * aborted() function. */
#define RETRY_RET(retry, code, acode) \
((((code) == NE_SOCK_CLOSED || (code) == NE_SOCK_RESET || \
 (code) == NE_SOCK_TRUNC) && retry) ? NE_RETRY : (acode))

/* Read and parse response status-line into 'status'.  'retry' is non-zero
 * if an NE_RETRY should be returned if an EOF is received. */
static int read_status_line(ne_request *req, ne_status *status, int retry)
	/*@globals internalState @*/
	/*@modifies req, status, internalState @*/
{
    char *buffer = req->respbuf;
    ssize_t ret;

    ret = ne_sock_readline(req->session->socket, buffer, sizeof req->respbuf);
    if (ret <= 0) {
	int aret = aborted(req, _("Could not read status line"), ret);
	return RETRY_RET(retry, ret, aret);
    }
    
    NE_DEBUG(NE_DBG_HTTP, "[status-line] < %s", buffer);
    strip_eol(buffer, &ret);
    
    if (status->reason_phrase) ne_free(status->reason_phrase);
    memset(status, 0, sizeof *status);

    if (ne_parse_statusline(buffer, status))
	return aborted(req, _("Could not parse response status line."), 0);

    return 0;
}

/* Discard a set of message headers. */
static int discard_headers(ne_request *req)
	/*@globals internalState @*/
	/*@modifies req, internalState @*/
{
    do {
	SOCK_ERR(req, ne_sock_readline(req->session->socket, req->respbuf, 
				       sizeof req->respbuf),
		 _("Could not read interim response headers"));
	NE_DEBUG(NE_DBG_HTTP, "[discard] < %s", req->respbuf);
    } while (strcmp(req->respbuf, EOL) != 0);
    return NE_OK;
}

/* Send the request, and read the response Status-Line. Returns:
 *   NE_RETRY   connection closed by server; persistent connection
 *		timeout
 *   NE_OK	success
 *   NE_*	error
 * On NE_RETRY and NE_* responses, the connection will have been 
 * closed already.
 */
static int send_request(ne_request *req, const ne_buffer *request)
	/*@globals internalState @*/
	/*@modifies req, internalState @*/
{
    ne_session *sess = req->session;
    ssize_t ret = NE_OK;
    int sentbody = 0; /* zero until body has been sent. */
    int retry; /* non-zero whilst the request should be retried */
    ne_status *status = &req->status;

    /* Send the Request-Line and headers */
    NE_DEBUG(NE_DBG_HTTP, "Sending request-line and headers:\n");
    /* Open the connection if necessary */
    HTTP_ERR(open_connection(req));

    /* Allow retry if a persistent connection has been used. */
    retry = sess->persisted;
    
    ret = ne_sock_fullwrite(req->session->socket, request->data, 
			    ne_buffer_size(request));
    if (ret < 0) {
	int aret = aborted(req, _("Could not send request"), ret);
	return RETRY_RET(retry, ret, aret);
    }
    
    if (!req->use_expect100 && req->body_length > 0) {
	/* Send request body, if not using 100-continue. */
	ret = send_request_body(req);
	if (ret < 0) {
	    int aret = aborted(req, _("Could not send request body"), ret);
	    return RETRY_RET(sess, ret, aret);
	}
    }
    
    NE_DEBUG(NE_DBG_HTTP, "Request sent; retry is %d.\n", retry);

    /* Loop eating interim 1xx responses (RFC2616 says these MAY be
     * sent by the server, even if 100-continue is not used). */
    while ((ret = read_status_line(req, status, retry)) == NE_OK 
	   && status->klass == 1) {
	NE_DEBUG(NE_DBG_HTTP, "Interim %d response.\n", status->code);
	retry = 0; /* successful read() => never retry now. */
	/* Discard headers with the interim response. */
	if ((ret = discard_headers(req)) != NE_OK) break;

	if (req->use_expect100 && (status->code == 100)
            && req->body_length > 0 && !sentbody) {
	    /* Send the body after receiving the first 100 Continue */
	    if ((ret = send_request_body(req)) != NE_OK) break;	    
	    sentbody = 1;
	}
    }

    return ret;
}

/* Read a message header from sock into buf, which has size 'buflen'.
 *
 * Returns:
 *   NE_RETRY: Success, read a header into buf.
 *   NE_OK: End of headers reached.
 *   NE_ERROR: Error (session error is set).
 */
static int read_message_header(ne_request *req, char *buf, size_t buflen)
	/*@globals internalState @*/
	/*@modifies req, buf, internalState @*/
{
    ssize_t n;
    ne_socket *sock = req->session->socket;

    n = ne_sock_readline(sock, buf, buflen);
    if (n <= 0)
	return aborted(req, _("Error reading response headers"), n);
    NE_DEBUG(NE_DBG_HTTP, "[hdr] %s", buf);

    strip_eol(buf, &n);

    if (n == 0) {
	NE_DEBUG(NE_DBG_HTTP, "End of headers.\n");
	return NE_OK;
    }

    buf += n;
    buflen -= n;

    while (buflen > 0) {
	char ch;

	/* Collect any extra lines into buffer */
	SOCK_ERR(req, ne_sock_peek(sock, &ch, 1),
		 _("Error reading response headers"));

	if (ch != ' ' && ch != '\t') {
	    /* No continuation of this header: stop reading. */
	    return NE_RETRY;
	}

	/* Otherwise, read the next line onto the end of 'buf'. */
	n = ne_sock_readline(sock, buf, buflen);
	if (n <= 0) {
	    return aborted(req, _("Error reading response headers"), n);
	}

	NE_DEBUG(NE_DBG_HTTP, "[cont] %s", buf);

	strip_eol(buf, &n);
	
	/* assert(buf[0] == ch), which implies len(buf) > 0.
	 * Otherwise the TCP stack is lying, but we'll be paranoid.
	 * This might be a \t, so replace it with a space to be
	 * friendly to applications (2616 says we MAY do this). */
	if (n) buf[0] = ' ';

	/* ready for the next header. */
	buf += n;
	buflen -= n;
    }

    ne_set_error(req->session, _("Response header too long"));
    return NE_ERROR;
}

/* Apache's default is 100, seems reasonable. */
#define MAX_HEADER_FIELDS 100

/* Read response headers.  Returns NE_* code, sets session error. */
static int read_response_headers(ne_request *req) 
	/*@globals internalState @*/
	/*@modifies req, internalState @*/
{
    char hdr[8192]; /* max header length */
    int ret, count = 0;
    
    while ((ret = read_message_header(req, hdr, sizeof hdr)) == NE_RETRY 
	   && ++count < MAX_HEADER_FIELDS) {
	struct header_handler *hdl;
	char *pnt;
	unsigned int hash = 0;
	
	for (hdl = req->header_catchers; hdl != NULL; hdl = hdl->next)
	    hdl->handler(hdl->userdata, hdr);

	/* Strip any trailing whitespace */
	pnt = hdr + strlen(hdr) - 1;
	while (pnt > hdr && (*pnt == ' ' || *pnt == '\t'))
	    *pnt-- = '\0';

	/* Convert the header name to lower case and hash it. */
	for (pnt = hdr; (*pnt != '\0' && *pnt != ':' && 
			 *pnt != ' ' && *pnt != '\t'); pnt++) {
	    *pnt = tolower(*pnt);
	    hash = HH_ITERATE(hash,*pnt);
	}

	/* Skip over any whitespace before the colon. */
	while (*pnt == ' ' || *pnt == '\t')
	    *pnt++ = '\0';

	/* ignore header lines which lack a ':'. */
	if (*pnt != ':')
	    continue;
	
	/* NUL-terminate at the colon (when no whitespace before) */
	*pnt++ = '\0';

	/* Skip any whitespace after the colon... */
	while (*pnt == ' ' || *pnt == '\t')
	    pnt++;

	/* pnt now points to the header value. */
	NE_DEBUG(NE_DBG_HTTP, "Header Name: [%s], Value: [%s]\n", hdr, pnt);
	
	/* Iterate through the header handlers */
	for (hdl = req->header_handlers[hash]; hdl != NULL; hdl = hdl->next) {
	    if (strcmp(hdr, hdl->name) == 0)
		hdl->handler(hdl->userdata, pnt);
	}
    }

    if (count == MAX_HEADER_FIELDS)
	ret = aborted(
	    req, _("Response exceeded maximum number of header fields."), 0);

    return ret;
}

/* Perform any necessary DNS lookup for the host given by *info;
 * return NE_ code. */
static int lookup_host(ne_session *sess, struct host_info *info)
	/*@modifies sess, info @*/
{
    if (sess->addrlist) return NE_OK;

    NE_DEBUG(NE_DBG_HTTP, "Doing DNS lookup on %s...\n", info->hostname);
    if (sess->notify_cb)
	sess->notify_cb(sess->notify_ud, ne_conn_namelookup, info->hostname);
    info->address = ne_addr_resolve(info->hostname, 0);
    if (ne_addr_result(info->address)) {
	char buf[256];
	ne_set_error(sess, _("Could not resolve hostname `%s': %s"), 
		     info->hostname,
		     ne_addr_error(info->address, buf, sizeof buf));
	ne_addr_destroy(info->address);
	info->address = NULL;
	return NE_LOOKUP;
    } else {
	return NE_OK;
    }
}

int ne_begin_request(ne_request *req)
{
    struct body_reader *rdr;
    struct host_info *host;
    ne_buffer *data;
    const ne_status *const st = &req->status;
    int ret;

    /* Resolve hostname if necessary. */
    host = req->session->use_proxy?&req->session->proxy:&req->session->server;
    if (host->address == NULL)
	HTTP_ERR(lookup_host(req->session, host));

    req->resp.mode = R_TILLEOF;
    
    /* Build the request string, and send it */
    data = build_request(req);
    DEBUG_DUMP_REQUEST(data->data);
    ret = send_request(req, data);
    /* Retry this once after a persistent connection timeout. */
    if (ret == NE_RETRY && !req->session->no_persist) {
	NE_DEBUG(NE_DBG_HTTP, "Persistent connection timed out, retrying.\n");
	ret = send_request(req, data);
    }
    ne_buffer_destroy(data);
    if (ret != NE_OK) return ret;

    /* Determine whether server claims HTTP/1.1 compliance. */
    req->session->is_http11 = (st->major_version == 1 && 
                               st->minor_version > 0) || st->major_version > 1;

    /* Persistent connections supported implicitly in HTTP/1.1 */
    if (req->session->is_http11) req->can_persist = 1;

    ne_set_error(req->session, "%d %s", st->code, st->reason_phrase);

    /* Read the headers */
    HTTP_ERR(read_response_headers(req));

#ifdef NE_HAVE_SSL
    /* Special case for CONNECT handling: the response has no body,
     * and the connection can persist. */
    if (req->session->in_connect && st->klass == 2) {
	req->resp.mode = R_NO_BODY;
	req->can_persist = 1;
    }
#endif

    /* HEAD requests and 204, 304 responses have no response body,
     * regardless of what headers are present. */
    if (req->method_is_head || st->code == 204 || st->code == 304)
    	req->resp.mode = R_NO_BODY;

    /* Prepare for reading the response entity-body.  Call each of the
     * body readers and ask them whether they want to accept this
     * response or not. */
    for (rdr = req->body_readers; rdr != NULL; rdr=rdr->next) {
	rdr->use = rdr->accept_response(rdr->userdata, req, st);
    }
    
    return NE_OK;
}

int ne_end_request(ne_request *req)
{
    struct hook *hk;
    int ret = NE_OK;

    /* Read headers in chunked trailers */
    if (req->resp.mode == R_CHUNKED)
	HTTP_ERR(read_response_headers(req));
    
    NE_DEBUG(NE_DBG_HTTP, "Running post_send hooks\n");
    for (hk = req->session->post_send_hooks; 
	 ret == NE_OK && hk != NULL; hk = hk->next) {
	ne_post_send_fn fn = (ne_post_send_fn)hk->fn;
	ret = fn(req, hk->userdata, &req->status);
    }
    
    /* Close the connection if persistent connections are disabled or
     * not supported by the server. */
    if (req->session->no_persist || !req->can_persist)
	ne_close_connection(req->session);
    else
	req->session->persisted = 1;
    
    return ret;
}

int ne_request_dispatch(ne_request *req) 
{
    int ret;

    /* Loop sending the request:
     * Retry whilst authentication fails and we supply it. */
    
    do {
	ssize_t len;
	
	HTTP_ERR(ne_begin_request(req));
	
	do {
	    len = ne_read_response_block(req, req->respbuf,
					 sizeof req->respbuf);
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

const ne_status *ne_get_status(const ne_request *req)
{
    return &req->status;
}

ne_session *ne_get_session(const ne_request *req)
{
    return req->session;
}

#ifdef NE_HAVE_SSL
/* Create a CONNECT tunnel through the proxy server.
 * Returns HTTP_* */
static int proxy_tunnel(ne_session *sess)
	/*@globals internalState @*/
	/*@modifies sess, internalState @*/
{
    /* Hack up an HTTP CONNECT request... */
    ne_request *req;
    int ret = NE_OK;
    char ruri[200];

    /* Can't use server.hostport here; Request-URI must include `:port' */
    ne_snprintf(ruri, sizeof ruri, "%s:%u", sess->server.hostname,  
		sess->server.port);
    req = ne_request_create(sess, "CONNECT", ruri);

    sess->in_connect = 1;
    ret = ne_request_dispatch(req);
    sess->in_connect = 0;

    sess->persisted = 0; /* don't treat this is a persistent connection. */

    if (ret != NE_OK || !sess->connected || req->status.klass != 2) {
	ne_set_error
	    (sess, _("Could not create SSL connection through proxy server"));
	ret = NE_ERROR;
    }

    ne_request_destroy(req);
    return ret;
}
#endif

/* Return the first resolved address for the given host. */
static const ne_inet_addr *resolve_first(ne_session *sess, 
                                         struct host_info *host)
	/*@modifies sess, host @*/
{
    if (sess->addrlist) {
        sess->curaddr = 0;
        return sess->addrlist[0];
    } else {
        return ne_addr_first(host->address);
    }
}

/* Return the next resolved address for the given host or NULL if
 * there are no more addresses. */
static const ne_inet_addr *resolve_next(ne_session *sess,
                                        struct host_info *host)
	/*@modifies sess, host @*/
{
    if (sess->addrlist) {
        if (sess->curaddr++ < sess->numaddrs)
            return sess->addrlist[sess->curaddr];
        else
            return NULL;
    } else {
        return ne_addr_next(host->address);
    }
}

/* Make new TCP connection to server at 'host' of type 'name'.  Note
 * that once a connection to a particular network address has
 * succeeded, that address will be used first for the next attempt to
 * connect. */
static int do_connect(ne_request *req, struct host_info *host, const char *err)
	/*@globals internalState @*/
	/*@modifies req, host, internalState @*/
{
    ne_session *const sess = req->session;
    int ret;

    if ((sess->socket = ne_sock_create()) == NULL) {
        ne_set_error(sess, _("Could not create socket"));
        return NE_ERROR;
    }

    if (host->current == NULL)
	host->current = resolve_first(sess, host);

    do {
	notify_status(sess, ne_conn_connecting, host->hostport);
#ifdef NE_DEBUGGING
	if (ne_debug_mask & NE_DBG_HTTP) {
	    char buf[150];
	    NE_DEBUG(NE_DBG_HTTP, "Connecting to %s\n",
		     ne_iaddr_print(host->current, buf, sizeof buf));
	}
#endif
	ret = ne_sock_connect(sess->socket, host->current, host->port);
    } while (ret && /* try the next address... */
	     (host->current = resolve_next(sess, host)) != NULL);

    if (ret) {
        ne_set_error(sess, "%s: %s", err, ne_sock_error(sess->socket));
        ne_sock_close(sess->socket);
	return NE_CONNECT;
    }

    notify_status(sess, ne_conn_connected, host->hostport);
    
    if (sess->rdtimeout)
	ne_sock_read_timeout(sess->socket, sess->rdtimeout);

    sess->connected = 1;
    /* clear persistent connection flag. */
    sess->persisted = 0;
    return NE_OK;
}

static int open_connection(ne_request *req) 
{
    ne_session *sess = req->session;
    int ret;
    
    if (sess->connected) return NE_OK;

    if (!sess->use_proxy)
	ret = do_connect(req, &sess->server, _("Could not connect to server"));
    else
	ret = do_connect(req, &sess->proxy,
			 _("Could not connect to proxy server"));

    if (ret != NE_OK) return ret;

#ifdef NE_HAVE_SSL
    /* Negotiate SSL layer if required. */
    if (sess->use_ssl && !sess->in_connect) {
        /* CONNECT tunnel */
        if (req->session->use_proxy)
            ret = proxy_tunnel(sess);
        
        if (ret == NE_OK)
            ret = ne__negotiate_ssl(req);

        /* This is probably only really needed for ne_negotiate_ssl
         * failures as proxy_tunnel will fail via aborted(). */
        if (ret != NE_OK)
            ne_close_connection(sess);
    }
#endif
    
    return ret;
}
