/* 
   Test for cookies interface (ne_cookies.h)
   Copyright (C) 2002-2003, Joe Orton <joe@manyfish.co.uk>

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
#include "ne_cookies.h"

#include "tests.h"
#include "child.h"
#include "utils.h"

static int serve_cookie(ne_socket *sock, void *ud)
{
    const char *hdr = ud;
    char buf[BUFSIZ];

    CALL(discard_request(sock));
    
    ne_snprintf(buf, BUFSIZ, "HTTP/1.0 200 Okey Dokey\r\n"
		"Connection: close\r\n" "%s\r\n\r\n", hdr);
    
    SEND_STRING(sock, buf);

    return OK;    
}

static int fetch_cookie(const char *hdr, const char *path,
			ne_cookie_cache *jar)
{
    ne_session *sess;
    
    CALL(make_session(&sess, serve_cookie, (void *)hdr));

    ne_cookie_register(sess, jar);
    
    CALL(any_request(sess, path));

    ne_session_destroy(sess);
    CALL(await_server());

    return OK;
}

static int parsing(void)
{
    static const struct {
	const char *hdr, *name, *value;
    } cookies[] = {
	{ "Set-Cookie: alpha=bar", "alpha", "bar" },
#if 0
	{ "Set-Cookie2: alpha=bar", "alpha", "bar" },
#endif
	{ "Set-Cookie: beta = bar", "beta", "bar" },
	{ "Set-Cookie: delta = bar; norman=fish", "delta", "bar" },
        /* parsing bug in <0.24.1 */
        { "Set-Cookie: alpha=beta; path", "alpha", "beta" },
	{ NULL, NULL, NULL }
    };
    int n;

    for (n = 0; cookies[n].hdr != NULL; n++) {
	ne_cookie_cache jar = {0};
	ne_cookie *ck;

	CALL(fetch_cookie(cookies[n].hdr, "/foo", &jar));

	ck = jar.cookies;
	ONV(ck == NULL, ("%d: cookie jar was empty!", n));
	
	ONV(strcmp(ck->name, cookies[n].name) || 
	    strcmp(ck->value, cookies[n].value),
	    ("%d: was [%s]=[%s]!", n, ck->name, ck->value));
    }

    return OK;
}

static int rejects(void)
{
    static const struct {
	const char *hdr, *path;
    } resps[] = {
	/* names prefixed with $ are illegal */
	{ "Set-Cookie2: $foo=bar, Version=1", "/foo" },
#define FOOBAR "Set-Cookie2: foo=bar, Version=1"
	/* Path is not prefix of Request-URI */
	{ FOOBAR ", Path=/bar", "/foo" },
	/* Domain must have embedded dots. */
	{ FOOBAR ", Domain=fish", "/foo" },
	/* Domain must domain-match request-host */ 
	{ FOOBAR ", Domain=other.host.com", "/foo" },
	/* Port not named in Port list */
	{ FOOBAR ", Port=\"12\"", "/foo" },
	{ NULL, NULL }
    };
    int n;
    
    for (n = 0; resps[n].hdr != NULL; n++) {
	ne_cookie_cache jar = {0};

	CALL(fetch_cookie(resps[n].hdr, resps[n].path, &jar));
	
	ONV(jar.cookies != NULL, 
	    ("cookie was returned for `%s'", resps[n].hdr));
    }

    return OK;
}

ne_test tests[] = {
    T(lookup_localhost),
    T(parsing),
    T(rejects),
    T(NULL) 
};

