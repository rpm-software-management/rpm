/* 
   lock tests
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

#include "ne_request.h"
#include "ne_locks.h"
#include "ne_socket.h"

#include "tests.h"
#include "child.h"
#include "utils.h"

/* return body of LOCK response for given lock. */
static char *lock_response(enum ne_lock_scope scope,
			   const char *depth,
			   const char *owner,
			   const char *timeout,
			   const char *token_href)
{
    static char buf[BUFSIZ];
    sprintf(buf, 
	    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
	    "<D:prop xmlns:D=\"DAV:\"><D:lockdiscovery><D:activelock>\n"
	    "<D:locktype><D:write/></D:locktype>\n"
	    "<D:lockscope><D:%s/></D:lockscope>\n"
	    "<D:depth>%s</D:depth>\n"
	    "<D:owner>%s</D:owner>\n"
	    "<D:timeout>%s</D:timeout>\n"
	    "<D:locktoken><D:href>%s</D:href></D:locktoken>\n"
	    "</D:activelock></D:lockdiscovery></D:prop>\n",
	    scope==ne_lockscope_exclusive?"exclusive":"shared",
	    depth, owner, timeout, token_href);
    return buf;
}	    

/* regression test for <= 0.18.2, where timeout field was not parsed correctly. */
static int lock_timeout(void)
{
    ne_session *sess = ne_session_create();
    char *resp, *rbody = lock_response(ne_lockscope_exclusive, "0", "me",
				       "Second-6500", "opaquelocktoken:foo");
    struct ne_lock lock = {0};

    ON(ne_session_server(sess, "localhost", 7777));

    CONCAT2(resp, 
	    "HTTP/1.1 200 OK\r\n" "Server: neon-test-server\r\n"
	    "Connection: close\r\n\r\n", rbody);
	
    CALL(spawn_server(7777, single_serve_string, resp));

    lock.uri = "/foo";
    lock.depth = 0;
    lock.scope = ne_lockscope_exclusive;
    lock.type = ne_locktype_write;
    lock.timeout = 5;

    ONREQ(ne_lock(sess, &lock));

    ONN("lock timeout ignored in response",
	lock.timeout != 6500);

    ne_session_destroy(sess);
    
    CALL(await_server());

    return OK;
}

ne_test tests[] = {
    T(lock_timeout),
    T(NULL)
};

