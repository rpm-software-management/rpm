/* 
   tests for compressed response handling.
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <fcntl.h>

#include "tests.h"

#ifndef NEON_ZLIB

static int skip_compress(void)
{
    return SKIP;
}

test_func tests[] = {
    skip_compress,
    NULL
};

#else

#include "ne_compress.h"

#include "child.h"
#include "utils.h"

static int failed = 0;

static void reader(void *ud, const char *block, size_t len)
{
    const char **pnt = ud;
    
    if (memcmp(*pnt, block, len) != 0)
	failed = 1;

    *pnt += len;
}

static int file2buf(int fd, ne_buffer *buf)
{
    char buffer[BUFSIZ];
    ssize_t n;
    
    while ((n = read(fd, buffer, fd)) > 0) {
	ne_buffer_append(buf, buffer, n);
    }
    
    return 0;
}

static int fetch(const char *realfn, const char *gzipfn, int chunked)
{
    ne_session *sess = ne_session_create();
    ne_request *req;
    int fd;
    ne_buffer *buf = ne_buffer_create();
    const char *pnt;
    struct serve_file_args sfargs;
    ne_decompress *dc;
    
    fd = open(realfn, O_RDONLY);
    ONN("failed to open file", fd < 0);
    file2buf(fd, buf);
    (void) close(fd);
    pnt = buf->data;
    
    ne_session_server(sess, "localhost", 7777);
    
    if (gzipfn) {
	sfargs.fname = gzipfn;
	sfargs.headers = "Content-Encoding: gzip\r\n";
    } else {
	sfargs.fname = realfn;
	sfargs.headers = NULL;
    }
    sfargs.chunks = chunked;
    
    CALL(spawn_server(7777, serve_file, &sfargs));
    
    req = ne_request_create(sess, "GET", "/");
    dc = ne_decompress_reader(req, ne_accept_2xx, reader, &pnt);

    ONREQ(ne_request_dispatch(req));

    ONN("error during decompress", ne_decompress_destroy(dc));

    ne_request_destroy(req);
    ne_session_destroy(sess);
    ne_buffer_destroy(buf);
    
    CALL(await_server());

    ONN("inflated response compare", failed);

    return OK;
}

/* Test the no-compression case. */
static int compress_0(void)
{
    return fetch("../NEWS", NULL, 0);
}

static int compress_1(void)
{
    return fetch("../NEWS", "file1.gz", 0);
}

/* file1.gz has an embedded filename. */
static int compress_2(void)
{
    return fetch("../NEWS", "file2.gz", 0);
}

/* deliver various different sizes of chunks: tests the various
 * decoding cases. */
static int compress_3(void)
{
    return fetch("../NEWS", "file2.gz", 1);
}

static int compress_4(void)
{
    return fetch("../NEWS", "file1.gz", 1);
}

static int compress_5(void)
{
    return fetch("../NEWS", "file2.gz", 12);
}

static int compress_6(void)
{
    return fetch("../NEWS", "file2.gz", 20);
}

static int compress_7(void)
{
    return fetch("../NEWS", "file1.gz", 10);
}

static int compress_8(void)
{
    return fetch("../NEWS", "file2.gz", 10);
}

test_func tests[] = {
    compress_0,
    compress_1,
    compress_2,
    compress_3,
    compress_4, 
    compress_5, 
    compress_6,
    compress_7,
    compress_8,
    NULL,
};

#endif /* NEON_ZLIB */
