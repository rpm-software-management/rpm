/* 
   Authentication tests
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
#include "ne_auth.h"
#include "ne_basic.h"

#include "tests.h"
#include "child.h"
#include "utils.h"

char username[BUFSIZ], password[BUFSIZ];
static int auth_failed;

static const char www_wally[] = "WWW-Authenticate: Basic realm=WallyWorld";

static int auth_cb(void *userdata, const char *realm, int tries, 
		   char *un, char *pw)
{
    strcpy(un, username);
    strcpy(pw, password);
    return tries;
}		   

static void auth_hdr(char *value)
{
#define B "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ=="
    NE_DEBUG(NE_DBG_HTTP, "Got auth header: [%s]\n", value);
    NE_DEBUG(NE_DBG_HTTP, "Wanted header:   [%s]\n", B);
    auth_failed = strcmp(value, B);
#undef B
}

/* Sends a response with given response-code. If hdr is not NULL,
 * sends that header string too (appending an EOL).  If eoc is
 * non-zero, request must be last sent down a connection; otherwise,
 * clength 0 is sent to maintain a persistent connection. */
static int send_response(nsocket *sock, const char *hdr, int code, int eoc)
{
    char buffer[BUFSIZ];
    
    sprintf(buffer, "HTTP/1.1 %d Blah Blah" EOL, code);
    
    if (hdr) {
	strcat(buffer, hdr);
	strcat(buffer, EOL);
    }

    if (eoc) {
	strcat(buffer, "Connection: close" EOL EOL);
    } else {
	strcat(buffer, "Content-Length: 0" EOL EOL);
    }
	
    return sock_send_string(sock, buffer);
}

static int auth_serve(nsocket *sock, void *userdata)
{
    auth_failed = 0;

    /* Register globals for discard_request. */
    got_header = auth_hdr;
    want_header = "Authorization";

    discard_request(sock);
    send_response(sock, www_wally, 401, 0);

    discard_request(sock);
    send_response(sock, NULL, auth_failed?500:200, 1);

    return 0;
}

static int basic(void)
{
    int ret;
    ne_session *sess = ne_session_create();

    ne_session_server(sess, "localhost", 7777);

    ne_set_server_auth(sess, auth_cb, NULL);

    strcpy(username, "Aladdin");
    strcpy(password, "open sesame");

    CALL(spawn_server(7777, auth_serve, NULL));
    
    ret = any_request(sess, "/norman");

    ne_session_destroy(sess);

    CALL(await_server());

    ONREQ(ret);
    return OK;
}

static int retry_serve(nsocket *sock, void *ud)
{
    discard_request(sock);
    send_response(sock, www_wally, 401, 0);

    discard_request(sock);
    send_response(sock, www_wally, 401, 0);

    discard_request(sock);
    send_response(sock, NULL, 200, 0);
    
    discard_request(sock);
    send_response(sock, www_wally, 401, 0);

    discard_request(sock);
    send_response(sock, NULL, 200, 0);

    discard_request(sock);
    send_response(sock, NULL, 200, 0);

    discard_request(sock);
    send_response(sock, NULL, 200, 0);

    discard_request(sock);
    send_response(sock, www_wally, 401, 0);

    discard_request(sock);
    send_response(sock, NULL, 200, 0);
    
    return OK;
}

static int retry_cb(void *userdata, const char *realm, int tries, 
		    char *un, char *pw)
{
    int *count = userdata;

    /* dummy creds; server ignores them anyway. */
    strcpy(un, "a");
    strcpy(pw, "b");

    switch (*count) {
    case 0:
    case 1:
	if (tries == *count) {
	    *count += 1;
	    return 0;
	} else {
	    t_context("On request #%d, got attempt #%d", *count, tries);
	    *count = -1;
	    return 1;
	}
	break;
    case 2:
    case 3:
	/* server fails a subsequent request, check that tries has
	 * reset to zero. */
	if (tries == 0) {
	    *count += 1;
	    return 0;
	} else {
	    t_context("On retry after failure #%d, tries was %d", 
		      *count, tries);
	    *count = -1;
	    return 1;
	}
	break;
    default:
	t_context("Count reached %d!?", *count);
	*count = -1;
    }
    return 1;
}

/* Test that auth retries are working correctly. */
static int retries(void)
{
    ne_session *sess = ne_session_create();
    int count = 0;
    
    ne_session_server(sess, "localhost", 7777);
    ne_set_server_auth(sess, retry_cb, &count);

    CALL(spawn_server(7777, retry_serve, NULL));

    /* This request will be 401'ed twice, then succeed. */
    ONREQ(any_request(sess, "/foo"));

    /* auth_cb will have set up context. */
    CALL(count != 2);

    /* this request will be 401'ed once, then succeed. */
    ONREQ(any_request(sess, "/foo"));

    /* auth_cb will have set up context. */
    CALL(count != 3);

    /* some 20x requests. */
    ONREQ(any_request(sess, "/foo"));
    ONREQ(any_request(sess, "/foo"));

    /* this request will be 401'ed once, then succeed. */
    ONREQ(any_request(sess, "/foo"));

    /* auth_cb will have set up context. */
    CALL(count != 4);

    ne_session_destroy(sess);

    CALL(await_server());

    return OK;
}

/* test digest auth 2068-style. */

/* test digest auth 2617-style. */

/* negotiation: within a single header, multiple headers.
 * check digest has precedence */

/* test auth-int, auth-int FAILURE. chunk trailers/non-trailer */

/* test logout */

/* proxy auth, proxy AND origin */

ne_test tests[] = {
    T(basic),
    T(retries),
    T(NULL)
};
