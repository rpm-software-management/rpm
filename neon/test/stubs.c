/* 
   neon test suite
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

/** These tests show that the stub functions produce appropriate
 * results to provide ABI-compatibility when a particular feature is
 * not supported by the library.
 **/

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
#include "ne_compress.h"

#include "tests.h"
#include "child.h"
#include "utils.h"

#if defined(NEON_ZLIB) && defined(NEON_SSL)
#define NO_TESTS 1
#endif

#ifndef NEON_ZLIB
static int sd_result = OK;

static void sd_reader(void *ud, const char *block, size_t len)
{
    const char *expect = ud;
    if (strncmp(expect, block, len) != 0) {
	sd_result = FAIL;
	t_context("decompress reader got bad data");
    }    
}

static int stub_decompress(void)
{
    ne_session *sess;
    ne_decompress *dc;
    ne_request *req;
    int ret;

    CALL(make_session(&sess, single_serve_string, 
		      "HTTP/1.1 200 OK" EOL
		      "Connection: close" EOL EOL
		      "abcde"));
    
    req = ne_request_create(sess, "GET", "/foo");

    dc = ne_decompress_reader(req, ne_accept_2xx, sd_reader, "abcde");
    
    ret = ne_request_dispatch(req);
    
    CALL(await_server());
    
    ONREQ(ret);

    ONN("decompress_destroy failed", ne_decompress_destroy(dc));
    
    ne_request_destroy(req);
    ne_session_destroy(sess);

    /* This is a skeleton test suite file. */
    return sd_result;
}
#endif

#ifndef NEON_SSL
static int stub_ssl(void)
{
    ne_session *sess = ne_session_create("https", "localhost", 7777);
    
    /* these should all fail when SSL is not supported. */
    ONN("load default CA succeeded", ne_ssl_load_default_ca(sess) == 0);
    ONN("load CA succeeded", ne_ssl_load_ca(sess, "Makefile") == 0);
    ONN("load PKCS12 succeeded", ne_ssl_load_pkcs12(sess, "Makefile") == 0);
    ONN("load PEM succeeded", ne_ssl_load_pem(sess, "Makefile", NULL) == 0);
    
    ne_session_destroy(sess);
    return OK;
}
#endif

#ifdef NO_TESTS
static int null_test(void) { return OK; }
#endif

ne_test tests[] = {
#ifndef NEON_ZLIB
    T(stub_decompress),
#endif
#ifndef NEON_SSL
    T(stub_ssl),
#endif
/* to prevent failure when SSL and zlib are supported. */
#ifdef NO_TESTS
    T(null_test),
#endif
    T(NULL) 
};

