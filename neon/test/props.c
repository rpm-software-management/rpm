/* 
   Tests for property handling
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

#include "ne_props.h"

#include "tests.h"
#include "child.h"
#include "utils.h"

static const ne_propname p_alpha = {"DAV:", "alpha"},
    p_beta = {"http://webdav.org/random/namespace", "beta"},
    p_delta = {NULL, "delta"};

/* Tests little except that ne_proppatch() doesn't segfault. */
static int patch_simple(void)
{
    ne_session *sess;
    ne_proppatch_operation ops[] = {
	{ &p_alpha, ne_propset, "fish" },
	{ &p_beta, ne_propremove, NULL },
	{ NULL, ne_propset, NULL }
    };
    
    CALL(make_session(&sess, single_serve_string, 
		      "HTTP/1.1 200 Goferit\r\n"
		      "Connection: close\r\n\r\n"));
    ONREQ(ne_proppatch(sess, "/fish", ops));
    ne_session_destroy(sess);
    return await_server();
}

ne_test tests[] = {
    T(patch_simple),
    T(NULL) 
};

