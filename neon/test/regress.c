/* 
   Some miscellenaneous regression tests
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
#include "ne_props.h"

#include "tests.h"
#include "child.h"

/* This caused a segfault in 0.15.3 and earlier. */
static int serve_dodgy_xml(nsocket *sock, void *ud)
{
    char buffer[BUFSIZ];

    CALL(discard_request(sock));
    
    sock_read(sock, buffer, clength);

    sock_send_string(sock,
		     "HTTP/1.0 207 OK\r\n"
		     "Server: foo\r\n"
		     "Connection: close\r\n"
		     "\r\n"
		     "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		     "<multistatus xmlns=\"DAV:\">"
		     "<response><propstat><prop><href>"
		     "</href></prop></propstat></response>"
		     "</multistatus>");

    return 0;
}

static void dummy_results(void *ud, const char *href,
			  const ne_prop_result_set *rset)
{

}

static int propfind_segv(void)
{
    ne_session *sess = ne_session_create();

    ne_session_server(sess, "localhost", 7777);

    CALL(spawn_server(7777, serve_dodgy_xml, NULL));
    
    ne_simple_propfind(sess, "/", 0, NULL, dummy_results, NULL);

    ne_session_destroy(sess);

    await_server();

    return OK;
}

test_func tests[] = {
    propfind_segv,
    NULL
};
