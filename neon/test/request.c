/* 
   HTTP request handling tests
   Copyright (C) 2001, Joe Orton <joe@manyfish.co.uk>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "config.h"

#include <sys/types.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "ne_request.h"
#include "ne_socket.h"

#include "tests.h"
#include "child.h"

static char buffer[BUFSIZ];
static int clength;

static int discard_request(nsocket *sock)
{
    NE_DEBUG(NE_DBG_HTTP, "Discarding request...\n");
    do {
	ON(sock_readline(sock, buffer, 1024) < 0);
	if (strncasecmp(buffer, "content-length:", 15) == 0) {
	    clength = atoi(buffer + 16);
	}
	NE_DEBUG(NE_DBG_HTTP, "[req] %s", buffer);
    } while (strcmp(buffer, EOL) != 0);
    
    return OK;
}

#define RESP200 "HTTP/1.1 200 OK\r\n" "Server: neon-test-server\r\n"
#define TE_CHUNKED "Transfer-Encoding: chunked\r\n"

static int single_serve_string(nsocket *s, void *userdata)
{
    const char *str = userdata;

    ON(discard_request(s));
    ON(sock_send_string(s, str));

    return OK;
}

/* takes response body chunks and appends them to a buffer. */
static void collector(void *ud, const char *data, size_t len)
{
    ne_buffer *buf = ud;
    ne_buffer_append(buf, data, len);
}

#define ONREQ(x) do { int _ret = (x); if (_ret) { snprintf(on_err_buf, 500, "%s line %d: HTTP error:\n%s", __FUNCTION__, __LINE__, ne_get_error(sess)); i_am(on_err_buf); return FAIL; } } while (0);


typedef ne_request *(*construct_request)(ne_session *sess, void *userdata);

static ne_request *construct_get(ne_session *sess, void *userdata)
{
    ne_request *r = ne_request_create(sess, "GET", "/");
    ne_buffer *buf = userdata;

    ne_add_response_body_reader(r, ne_accept_2xx, collector, buf);

    return r;
}

static int run_request(ne_session *sess, int status,
		       construct_request cb, void *userdata)
{
    ne_request *req = cb(sess, userdata);

    ON(req == NULL);
    
    ONREQ(ne_request_dispatch(req));
 
    ONN("response status", ne_get_status(req)->code != status);

    ne_request_destroy(req);

    return OK;
}

static int expect_response(const char *expect, server_fn fn, void *userdata)
{
    ne_session *sess = ne_session_create();
    ne_buffer *buf = ne_buffer_create();
    int ret;

    ON(sess == NULL || buf == NULL);
    ON(ne_session_server(sess, "localhost", 7777));
    ON(spawn_server(7777, fn, userdata));

    ret = run_request(sess, 200, construct_get, buf);
    if (ret) {
	reap_server();
	return ret;
    }

    ON(await_server());

    ONN("response body match", strcmp(buf->data, expect));

    ne_session_destroy(sess);
    
    return OK;
}

static int single_get_eof(void)
{
    return expect_response("a", single_serve_string, 
			   RESP200
			   "Connection: close\r\n"
			   "\r\n"
			   "a");
}

static int single_get_clength(void)
{
    return expect_response("a", single_serve_string,
			   RESP200
			   "Content-Length: 1\r\n"
			   "\r\n"
			   "a"
			   "bbbbbbbbasdasd");
}

static int single_get_chunked(void) 
{
    return expect_response("a", single_serve_string,
			   RESP200 TE_CHUNKED
			   "\r\n"
			   "1\r\n"
			   "a\r\n"
			   "0\r\n" "\r\n"
			   "g;lkjalskdjalksjd");
}

#define CHUNK(len, data) #len "\r\n" data "\r\n"

#define ABCDE_CHUNKS CHUNK(1, "a") CHUNK(1, "b") \
 CHUNK(1, "c") CHUNK(1, "d") \
 CHUNK(1, "e") CHUNK(0, "")

static int chunk_syntax_1(void)
{
    /* lots of little chunks. */
    ON(expect_response("abcde", single_serve_string,
		       RESP200 TE_CHUNKED
		       "\r\n"
		       ABCDE_CHUNKS));
    return OK;
}

static int chunk_syntax_2(void)
{    
    /* leading zero's */
    ON(expect_response("0123456789abcdef", single_serve_string,
		       RESP200 TE_CHUNKED
		       "\r\n"
		       "000000010\r\n" "0123456789abcdef\r\n"
		       "000000000\r\n" "\r\n"));
    return OK;
}

static int chunk_syntax_3(void)
{
    /* chunk-extensions. */
    ON(expect_response("0123456789abcdef", single_serve_string,
		       RESP200 TE_CHUNKED
		       "\r\n"
		       "000000010; foo=bar; norm=fish\r\n" 
		       "0123456789abcdef\r\n"
		       "000000000\r\n" "\r\n"));
    return OK;
}

static int chunk_syntax_4(void)
{
    /* trailers. */
    ON(expect_response("abcde", single_serve_string,
		       RESP200 TE_CHUNKED
		       "\r\n"
		       "00000005; foo=bar; norm=fish\r\n" 
		       "abcde\r\n"
		       "000000000\r\n" 
		       "X-Hello: world\r\n"
		       "X-Another: header\r\n"
		       "\r\n"));
    return OK;
}

static int chunk_syntax_5(void)
{   
    /* T-E dominates over C-L. */
    ON(expect_response("abcde", single_serve_string,
		       RESP200 TE_CHUNKED
		       "Content-Length: 300\r\n" 
		       "\r\n"
		       ABCDE_CHUNKS));
    return OK;
}

static int fold_headers(void)
{
    ON(expect_response("abcde", single_serve_string,
		       RESP200 "Content-Length: \r\n   5\r\n"
		       "\r\n"
		       "abcde"));
    return OK;
}

static int fold_many_headers(void)
{
    ON(expect_response("abcde", single_serve_string,
		       RESP200 "Content-Length: \r\n \r\n \r\n \r\n  5\r\n"
		       "\r\n"
		       "abcde"));
    return 0;    
}

static void mh_header(void *ctx, const char *value)
{
    int *state = ctx;
    static const char *hdrs[] = { "jim", "jab", "jar" };

    if (*state < 0 || *state > 2) {
	/* already failed. */
	return;
    }

    if (strcmp(value, hdrs[*state]))
	*state = -*state;
    else
	(*state)++;
}

/* check headers callbacks are working correctly. */
static int multi_header(void)
{
    ne_session *sess = ne_session_create();
    ne_request *req;
    int ret, state = 0;

    ON(sess == NULL);
    ON(ne_session_server(sess, "localhost", 7777));
    ON(spawn_server(7777, single_serve_string, 
		    RESP200 
		    "X-Header: jim\r\n" 
		    "x-header: jab\r\n"
		    "x-Header: jar\r\n"
		    "Content-Length: 0\r\n\r\n"));

    req = ne_request_create(sess, "GET", "/");
    ON(req == NULL);

    ne_add_response_header_handler(req, "x-header", mh_header, &state);

    ret = ne_request_dispatch(req);
    if (ret) {
	reap_server();
	ONREQ(ret);
	ONN("shouldn't get here.", 1);
    }

    ON(await_server());

    ON(state != 3);

    ne_request_destroy(req);
    ne_session_destroy(sess);

    return OK;
}

struct body {
    char *body;
    size_t size;
};

static int want_body(nsocket *sock, void *userdata)
{
    struct body *b = userdata;
    char *buf = ne_malloc(b->size);

    clength = 0;
    CALL(discard_request(sock));
    ONN("request has c-l header", clength == 0);
    
    ONN("request length", clength != (int)b->size);
    
    NE_DEBUG(NE_DBG_HTTP, "reading body of %d bytes...\n", b->size);
    
    ON(sock_fullread(sock, buf, b->size));
    
    ON(sock_send_string(sock, RESP200 "Content-Length: 0\r\n\r\n"));

    ON(memcmp(buf, b->body, b->size));

    return OK;
}

static ssize_t provide_body(void *userdata, char *buf, size_t buflen)
{
    static const char *pnt;
    static size_t left;
    struct body *b = userdata;

    if (buflen == 0) {
	pnt = b->body;
	left = b->size;
    } else {
	if (left < buflen) buflen = left;
	memcpy(buf, pnt, buflen);
	left -= buflen;
    }
    
    return buflen;
}

static int send_bodies(void)
{
    int n, m;

#define B(x) { x, strlen(x) }
    struct body bodies[] = { 
	B("abcde"), 
	{ "\0\0\0\0\0\0", 6 },
	{ NULL, 50000 },
	{ NULL }
    };

#define BIG 2
    /* make the body with some cruft. */
    bodies[BIG].body = ne_malloc(bodies[BIG].size);
    for (n = 0; n < bodies[BIG].size; n++) {
	bodies[BIG].body[n] = (char)n%80;
    }

    for (m = 0; m < 2; m++) {
	for (n = 0; bodies[n].body != NULL; n++) {
	    ne_session *sess = ne_session_create();
	    ne_request *req;
	    
	    ON(sess == NULL);
	    ON(ne_session_server(sess, "localhost", 7777));
	    ON(spawn_server(7777, want_body, &(bodies[n])));

	    req = ne_request_create(sess, "PUT", "/");
	    ON(req == NULL);

	    if (m == 0) {
		ne_set_request_body_buffer(req, bodies[n].body, bodies[n].size);
	    } else {
		ne_set_request_body_provider(req, bodies[n].size, 
					     provide_body, &bodies[n]);
	    }

	    ONREQ(ne_request_dispatch(req));
	    
	    CALL(await_server());
	    
	    ne_request_destroy(req);
	    ne_session_destroy(sess);
	}
    }

    return OK;
}

/* TODO: */

/* test that header folding is bounded, e.g. have the server process do:
 *   send("Foo: norman");
 *   while(1) send("  Jim\r\n");
 */

/* test sending request body. */
static int send_body(void)
{
    return FAIL;
}

test_func tests[] = {
    lookup_localhost,
    single_get_clength,
    single_get_eof,
    single_get_chunked,
    chunk_syntax_1,
    chunk_syntax_2,
    chunk_syntax_3,
    chunk_syntax_4,
    chunk_syntax_5,
    fold_headers,
    fold_many_headers,
    multi_header,
    send_bodies,
    NULL,
    send_body,
    NULL
};
