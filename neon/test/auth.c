/* 
   Authentication tests
   Copyright (C) 2001-2004, Joe Orton <joe@manyfish.co.uk>

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

static const char username[] = "Aladdin", password[] = "open sesame";
static int auth_failed;

#define BASIC_WALLY "Basic realm=WallyWorld"
#define CHAL_WALLY "WWW-Authenticate: " BASIC_WALLY

static int auth_cb(void *userdata, const char *realm, int tries, 
		   char *un, char *pw)
{
    if (strcmp(realm, "WallyWorld")) {
        NE_DEBUG(NE_DBG_HTTP, "Got wrong realm '%s'!\n", realm);
        return -1;
    }    
    strcpy(un, username);
    strcpy(pw, password);
    return tries;
}		   

static void auth_hdr(char *value)
{
#define B "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ=="
    auth_failed = strcmp(value, B);
    NE_DEBUG(NE_DBG_HTTP, "Got auth header: [%s]\nWanted header:   [%s]\n"
	     "Result: %d\n", value, B, auth_failed);
#undef B
}

/* Sends a response with given response-code. If hdr is not NULL,
 * sends that header string too (appending an EOL).  If eoc is
 * non-zero, request must be last sent down a connection; otherwise,
 * clength 0 is sent to maintain a persistent connection. */
static int send_response(ne_socket *sock, const char *hdr, int code, int eoc)
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
	
    return SEND_STRING(sock, buffer);
}

/* Server function which sends two responses: first requires auth,
 * second doesn't. */
static int auth_serve(ne_socket *sock, void *userdata)
{
    char *hdr = userdata;

    auth_failed = 1;

    /* Register globals for discard_request. */
    got_header = auth_hdr;
    want_header = "Authorization";

    discard_request(sock);
    send_response(sock, hdr, 401, 0);

    discard_request(sock);
    send_response(sock, NULL, auth_failed?500:200, 1);

    return 0;
}

/* Test that various Basic auth challenges are correctly handled. */
static int basic(void)
{
    const char *hdrs[] = {
        /* simplest case */
        CHAL_WALLY,

        /* several challenges, one header */
        "WWW-Authenticate: BarFooScheme, " BASIC_WALLY,

        /* several challenges, one header */
        CHAL_WALLY ", BarFooScheme realm=\"PenguinWorld\"",

        /* whitespace tests. */
        "WWW-Authenticate:   Basic realm=WallyWorld   ",

        /* nego test. */
        "WWW-Authenticate: Negotiate fish, Basic realm=WallyWorld",

        /* nego test. */
        "WWW-Authenticate: Negotiate fish, bar=boo, Basic realm=WallyWorld",

        /* multi-header case 1 */
        "WWW-Authenticate: BarFooScheme\r\n"
        CHAL_WALLY,
        
        /* multi-header cases 1 */
        CHAL_WALLY "\r\n"
        "WWW-Authenticate: BarFooScheme bar=\"foo\"",

        /* multi-header case 3 */
        "WWW-Authenticate: FooBarChall foo=\"bar\"\r\n"
        CHAL_WALLY "\r\n"
        "WWW-Authenticate: BarFooScheme bar=\"foo\""
    };
    size_t n;
    
    for (n = 0; n < sizeof(hdrs)/sizeof(hdrs[0]); n++) {
        ne_session *sess;
        
        CALL(make_session(&sess, auth_serve, (void *)hdrs[n]));
        ne_set_server_auth(sess, auth_cb, NULL);
        
        CALL(any_2xx_request(sess, "/norman"));
        
        ne_session_destroy(sess);
        CALL(await_server());
    }

    return OK;
}

static int retry_serve(ne_socket *sock, void *ud)
{
    discard_request(sock);
    send_response(sock, CHAL_WALLY, 401, 0);

    discard_request(sock);
    send_response(sock, CHAL_WALLY, 401, 0);

    discard_request(sock);
    send_response(sock, NULL, 200, 0);
    
    discard_request(sock);
    send_response(sock, CHAL_WALLY, 401, 0);

    discard_request(sock);
    send_response(sock, NULL, 200, 0);

    discard_request(sock);
    send_response(sock, NULL, 200, 0);

    discard_request(sock);
    send_response(sock, NULL, 200, 0);

    discard_request(sock);
    send_response(sock, CHAL_WALLY, 401, 0);

    discard_request(sock);
    send_response(sock, NULL, 200, 0);

    discard_request(sock);
    send_response(sock, CHAL_WALLY, 401, 0);

    discard_request(sock);
    send_response(sock, CHAL_WALLY, 401, 0);

    discard_request(sock);
    send_response(sock, CHAL_WALLY, 401, 0);

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
    case 4:
    case 5:
	if (tries > 1) {
	    t_context("Attempt counter reached #%d", tries);
	    *count = -1;
	    return 1;
	}
	return tries;
    default:
	t_context("Count reached %d!?", *count);
	*count = -1;
    }
    return 1;
}

/* Test that auth retries are working correctly. */
static int retries(void)
{
    ne_session *sess;
    int count = 0;
    
    CALL(make_session(&sess, retry_serve, NULL));

    ne_set_server_auth(sess, retry_cb, &count);

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

    /* First request is 401'ed by the server at both attempts. */
    ONV(any_request(sess, "/foo") != NE_AUTH,
	("auth succeeded, should have failed: %s", ne_get_error(sess)));

    count++;

    /* Second request is 401'ed first time, then will succeed if
     * retried.  0.18.0 didn't reset the attempt counter though so 
     * this didn't work. */
    ONV(any_request(sess, "/foo") == NE_AUTH,
	("auth failed on second try, should have succeeded: %s", ne_get_error(sess)));

    ne_session_destroy(sess);

    CALL(await_server());

    return OK;
}

/* crashes with neon <0.22 */
static int forget_regress(void)
{
    ne_session *sess = ne_session_create("http", "localhost", 7777);
    ne_forget_auth(sess);
    ne_session_destroy(sess);
    return OK;    
}

static int fail_auth_cb(void *ud, const char *realm, int attempt, 
			char *un, char *pw)
{
    return 1;
}

/* this may trigger a segfault in neon 0.21.x and earlier. */
static int tunnel_regress(void)
{
    ne_session *sess = ne_session_create("https", "localhost", 443);
    ne_session_proxy(sess, "localhost", 7777);
    ne_set_server_auth(sess, fail_auth_cb, NULL);
    CALL(spawn_server(7777, single_serve_string,
		      "HTTP/1.1 401 Auth failed.\r\n"
		      "WWW-Authenticate: Basic realm=asda\r\n"
		      "Content-Length: 0\r\n\r\n"));
    any_request(sess, "/foo");
    ne_session_destroy(sess);
    CALL(await_server());
    return OK;
}

/* test digest auth 2068-style. */

/* test digest auth 2617-style. */

/* test that digest has precedence over Basic for multi-scheme
 * challenges */

/* test auth-int, auth-int FAILURE. chunk trailers/non-trailer */

/* test logout */

/* proxy auth, proxy AND origin */

ne_test tests[] = {
    T(lookup_localhost),
    T(basic),
    T(retries),
    T(forget_regress),
    T(tunnel_regress),
    T(NULL)
};
