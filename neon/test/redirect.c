/* 
   Tests for 3xx redirect interface (ne_redirect.h)
   Copyright (C) 2002, Joe Orton <joe@manyfish.co.uk>

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

#include "ne_redirect.h"

#include "tests.h"
#include "child.h"
#include "utils.h"

struct redir_args {
    int code;
    const char *dest;
};

static int serve_redir(ne_socket *sock, void *ud)
{
    struct redir_args *args = ud;
    char buf[BUFSIZ];

    CALL(discard_request(sock));

    ne_snprintf(buf, BUFSIZ, 
		"HTTP/1.0 %d Get Ye Away\r\n"
		"Content-Length: 0\r\n"
		"Location: %s\r\n\n",
		args->code, args->dest);

    SEND_STRING(sock, buf);

    return OK;
}

static int check_redir(struct redir_args *args, const char *expect)
{
    ne_session *sess;
    int ret;
    const ne_uri *loc;
    
    CALL(make_session(&sess, serve_redir, args));

    ne_redirect_register(sess);
    
    ret = any_request(sess, "/redir/me");

    ONN("did not get NE_REDIRECT", ret != NE_REDIRECT);

    loc = ne_redirect_location(sess);
    
    ONN("redirect location was NULL", loc == NULL);

    ONV(strcmp(ne_uri_unparse(loc), expect),
	("redirected to `%s' not `%s'", ne_uri_unparse(loc), expect));

    ne_session_destroy(sess);

    return OK;
}

#define DEST "http://foo.com/blah/blah/bar"

static int simple(void)
{
    struct redir_args args = {302, DEST};
    return check_redir(&args, DEST);
}

static int redir_303(void)
{
    struct redir_args args = {303, DEST};
    return check_redir(&args, DEST);
}

/* check that a non-absoluteURI is qualified properly */
static int non_absolute(void)
{
    struct redir_args args = {302, "/foo/bar/blah"};
    return check_redir(&args, "http://localhost:7777/foo/bar/blah");
}

#if 0
/* could implement failure on self-referential redirects, but
 * realistically, the application must implement a max-redirs count
 * check, so it's kind of redundant.  Mozilla takes this approach. */
static int fail_loop(void)
{
    ne_session *sess;
    
    CALL(make_session(&sess, serve_redir, "http://localhost:7777/foo/bar"));

    ne_redirect_register(sess);

    ONN("followed looping redirect", 
	any_request(sess, "/foo/bar") != NE_ERROR);

    ne_session_destroy(sess);
    return OK;
}
#endif

ne_test tests[] = {
    T(lookup_localhost),
    T(simple),
    T(redir_303),
    T(non_absolute),
    T(NULL) 
};

