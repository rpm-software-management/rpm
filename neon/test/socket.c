/* 
   Socket handling tests
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "ne_socket.h"

#include "child.h"
#include "tests.h"

static struct in_addr localhost;
static char buffer[BUFSIZ];

static int resolve(void)
{
    ONN("could not resolve `localhost'",
	ne_name_lookup("localhost", &localhost));
    /* and again for child.c */
    CALL(lookup_localhost());
    return OK;
}

static int serve_close(ne_socket *sock, void *ud)
{
    return 0;
}

static int begin(ne_socket **sock, server_fn fn, void *ud)
{
    CALL(spawn_server(7777, fn, ud));
    *sock = ne_sock_connect(localhost, 7777);
    ONN("could not connect to localhost:7777", sock == NULL);
    return OK;
}

static int just_connect(void)
{
    ne_socket *sock;
    
    CALL(begin(&sock, serve_close, NULL));
    ne_sock_close(sock);
    CALL(await_server());

    return OK;
}

/* Exect a read() to return EOF */
static int expect_close(ne_socket *sock)
{
    ssize_t n = ne_sock_read(sock, buffer, 1);
    ONV(n != NE_SOCK_CLOSED, ("read gave %d not closure", n));
    return OK;
}

/* Exect a ne_sock_peek() to return EOF */
static int expect_peek_close(ne_socket *sock)
{
    ssize_t n = ne_sock_read(sock, buffer, 1);
    ONV(n != NE_SOCK_CLOSED, ("peek gave %d not closure", n));
    return OK;
}

/* Test that just does a connect then a close. */
static int read_close(void)
{
    ne_socket *sock;

    CALL(begin(&sock, serve_close, NULL));
    CALL(expect_close(sock));    
    ONN("close failed", ne_sock_close(sock));
    CALL(await_server());
    return OK;
}

/* Test that just does a connect then a close (but gets the close via
 * ne_sock_peek). */
static int peek_close(void)
{
    ne_socket *sock;

    CALL(begin(&sock, serve_close, NULL));
    CALL(expect_peek_close(sock));    
    ONN("close failed", ne_sock_close(sock));
    CALL(await_server());
    return OK;
}


struct string {
    char *data;
    size_t len;
};

static int serve_string(ne_socket *sock, void *ud)
{
    struct string *str = ud;

    ONN("write failed", ne_sock_fullwrite(sock, str->data, str->len));
    
    return 0;
}

/* Don't change this string. */
#define STR "Hello, World."

/* do a sock_peek() on sock for 'len' bytes, and expect 'str'. */
static int peek_expect(ne_socket *sock, const char *str, size_t len)
{
    ssize_t ret = ne_sock_peek(sock, buffer, len);
    ONV((ssize_t)len != ret, ("peek got %d bytes not %d", ret, len));
    ONV(memcmp(str, buffer, len),
	("peek mismatch: `%.*s' not `%.*s'", len, buffer, len, str));    
    return OK;
}

/* do a sock_read() on sock for 'len' bytes, and expect 'str'. */
static int read_expect(ne_socket *sock, const char *str, size_t len)
{
    ssize_t ret = ne_sock_read(sock, buffer, len);
    ONV((ssize_t)len != ret, ("peek got %d bytes not %d", ret, len));
    ONV(memcmp(str, buffer, len),
	("read mismatch: `%.*s' not `%.*s'", len, buffer, len, str));    
    return OK;
}

/* Test a simple read. */
static int single_read(void)
{
    ne_socket *sock;
    struct string hello = { STR, strlen(STR) };

    CALL(begin(&sock, serve_string, &hello));
    CALL(read_expect(sock, STR, strlen(STR)));
    CALL(expect_close(sock));

    CALL(await_server());
    return OK;
}

/* Test a simple peek. */
static int single_peek(void)
{
    ne_socket *sock;
    struct string hello = { STR, strlen(STR) };

    CALL(begin(&sock, serve_string, &hello));
    CALL(peek_expect(sock, STR, strlen(STR)));
    ONN("close failed", ne_sock_close(sock));

    CALL(await_server());
    return OK;
}

/* Test lots of 1-byte reads. */
static int small_reads(void)
{
    ne_socket *sock;
    struct string hello = { STR, strlen(STR) };
    char *pnt;

    CALL(begin(&sock, serve_string, &hello));

    /* read the string byte-by-byte. */
    for (pnt = hello.data; *pnt; pnt++) {
	CALL(read_expect(sock, pnt, 1));
    }

    ONN("close failed", ne_sock_close(sock));
    CALL(await_server());
    return OK;
}

/* Stress out the read buffer handling a little. */
static int read_and_peek(void)
{
    ne_socket *sock;
    struct string hello = { STR, strlen(STR) };

#define READ(str) CALL(read_expect(sock, str, strlen(str)))
#define PEEK(str) CALL(peek_expect(sock, str, strlen(str)))

    CALL(begin(&sock, serve_string, &hello));

    PEEK("Hello");
    PEEK("Hell");
    PEEK(STR);
    READ("He");
    PEEK("llo, ");
    READ("l");
    PEEK("lo, World.");
    READ("lo, Worl");
    PEEK("d."); PEEK("d");
    READ("d.");

    CALL(expect_close(sock));

    ONN("close failed", ne_sock_close(sock));
    CALL(await_server());
    return OK;
}

/* Read more bytes than were written. */
static int larger_read(void)
{
    ne_socket *sock;
    struct string hello = { STR, strlen(STR) };
    ssize_t nb;

    CALL(begin(&sock, serve_string, &hello));
    
    nb = ne_sock_read(sock, buffer, hello.len + 10);
    ONV(nb != (ssize_t)hello.len, ("read gave too many bytes (%d)", nb));
    ONN("read gave wrong data", memcmp(buffer, hello.data, hello.len));
    
    CALL(expect_close(sock));

    ONN("close failed", ne_sock_close(sock));
    CALL(await_server());
    return OK;
}

ne_test tests[] = {
    T(resolve),
    T(just_connect),
    T(read_close),
    T(peek_close),
    T(single_read),
    T(single_peek),
    T(small_reads),
    T(read_and_peek),
    T(larger_read),
    T(NULL)
};
