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

#include "tests.h"
#include "child.h"
#include "utils.h"

#ifndef NEON_SSL
/* this file shouldn't be built if SSL is not enabled. */
#error SSL not supported
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>

#define ERROR_SSL_STRING (ERR_reason_error_string(ERR_get_error()))

SSL_CTX *server_ctx;

static int s_strwrite(SSL *s, const char *buf)
{
    size_t len = strlen(buf);
    
    ONV(SSL_write(s, buf, len) != (int)len,
	("SSL_write failed: %s", ERROR_SSL_STRING));

    return OK;
}

static int serve_ssl(nsocket *sock, void *ud)
{
    int fd = sock_get_fd(sock);
    /* we don't want OpenSSL to close this socket for us. */
    BIO *bio = BIO_new_socket(fd, BIO_NOCLOSE);
    SSL *ssl = SSL_new(server_ctx);
    char buf[BUFSIZ];

    ONN("SSL_new failed", ssl == NULL);

    SSL_set_bio(ssl, bio, bio);

    ONV(SSL_accept(ssl) != 1,
	("SSL_accept failed: %s", ERROR_SSL_STRING));

    SSL_read(ssl, buf, BUFSIZ);

    CALL(s_strwrite(ssl, "HTTP/1.0 200 OK\r\n"
		    "Content-Length: 0\r\n"
		    "Connection: close\r\n\r\n"));

    /* Erk, shutdown is messy! See Eric Rescorla's article:
     * http://www.linuxjournal.com/article.php?sid=4822 ; we'll just
     * hide our heads in the sand here. */
    SSL_shutdown(ssl);
    SSL_free(ssl);

    return OK;
}

static int init(void)
{
    ONN("sock_init failed.\n", sock_init());
    server_ctx = SSL_CTX_new(SSLv23_server_method());
    ONN("SSL_CTX_new failed", server_ctx == NULL);
    ONN("failed to load private key",
	!SSL_CTX_use_PrivateKey_file(server_ctx, 
				    "server.key", SSL_FILETYPE_PEM));
    ONN("failed to load certificate",
	!SSL_CTX_use_certificate_file(server_ctx, 
				      "server.pem", SSL_FILETYPE_PEM));
    return OK;
}

static int simple(void)
{
    ne_session *sess;
    int ret;

    CALL(make_session(&sess, serve_ssl, NULL));

    ne_set_secure(sess, 1);

    ret = any_request(sess, "/foo");

    CALL(await_server());

    ONREQ(ret);

    ne_session_destroy(sess);
    return OK;
}

ne_test tests[] = {
    T(init),

    T(simple),

    T(NULL) 
};
