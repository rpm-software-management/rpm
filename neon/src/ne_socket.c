/* 
   socket handling routines
   Copyright (C) 1998-2001, Joe Orton <joe@light.plus.com>, 
   except where otherwise indicated.

   Portions are:
   Copyright (C) 1999-2000 Tommi Komulainen <Tommi.Komulainen@iki.fi>
   Originally under GPL in Mutt, http://www.mutt.org/
   Relicensed under LGPL for neon, http://www.webdav.org/neon/

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

   The sock_readline() function is:

   Copyright (c) 1999 Eric S. Raymond

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without
   restriction, including without limitation the rights to use, copy,
   modify, merge, publish, distribute, sublicense, and/or sell copies
   of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*/

#include "config.h"

#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef WIN32
#include <WinSock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif /* WIN32 */

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <sys/stat.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <errno.h>

#include <fcntl.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif 
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

/* SOCKS support. */
#ifdef HAVE_SOCKS_H
#include <socks.h>
#endif

#include "ne_i18n.h"
#include "ne_utils.h"
#include "ne_string.h"
#include "ne_socket.h"
#include "ne_alloc.h"

#if defined(BEOS_PRE_BONE)
#define NEON_WRITE(a,b,c) send(a,b,c,0)
#define NEON_READ(a,b,c) recv(a,b,c,0)
#define NEON_CLOSE(s) closesocket(s)
#elif defined(WIN32)
#define NEON_WRITE(a,b,c) send(a,b,c,0)
#define NEON_READ(a,b,c) recv(a,b,c,0)
#define NEON_CLOSE(s) closesocket(s)
#else /* really Unix! */
#define NEON_WRITE(a,b,c) write(a,b,c)
#define NEON_READ(a,b,c) read(a,b,c)
#define NEON_CLOSE(s) close(s)
#endif

#ifdef ENABLE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>

/* Whilst the OpenSSL interface *looks* like it is not thread-safe, it
 * appears to do horrible gymnastics to maintain per-thread global
 * variables for error reporting. UGH! */
#define ERROR_SSL_STRING (ERR_reason_error_string(ERR_get_error()))

#endif

/* BeOS doesn't have fd==sockets on anything pre-bone, so see if
 * we need to drop back to our old ways... 
 */
#ifdef __BEOS__
  #ifndef BONE_VERSION /* if we have BONE this isn't an issue... */
    #define BEOS_PRE_BONE
  #endif
#endif

struct nsocket_s {
    int fd;
    const char *error; /* Store error string here */
    sock_progress progress_cb;
    void *progress_ud;
#ifdef ENABLE_SSL
    SSL *ssl;
    SSL_CTX *default_ctx;
#endif
#ifdef BEOS_PRE_BONE
#define MAX_PEEK_BUFFER 1024
    char peeked_bytes[MAX_PEEK_BUFFER];
    char *peeked_bytes_curpos;
    int peeked_bytes_avail;
#endif
};

struct nssl_context_s {
#ifdef ENABLE_SSL
    SSL_CTX *ctx;
#endif
    nssl_accept cert_accept;
    void *accept_ud; /* userdata for callback */

    /* private key prompt callback */
    nssl_key_prompt key_prompt;
    void *key_userdata;
    const char *key_file;
};
    
void sock_register_progress(nsocket *sock, sock_progress cb, void *userdata)
{
    sock->progress_cb = cb;
    sock->progress_ud = userdata;
}

void sock_call_progress(nsocket *sock, off_t progress, off_t total)
{
    if (sock->progress_cb) {
	sock->progress_cb(sock->progress_ud, progress, total);
    }
}

int sock_init(void)
{
#ifdef WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
    
    wVersionRequested = MAKEWORD(2, 2);
    
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0)
	return -1;

#endif

#ifdef ENABLE_SSL
    SSL_load_error_strings();
    SSL_library_init();

    NE_DEBUG(NE_DBG_SOCKET, "Initialized SSL.\n");
#endif

    return 0;
}

void sock_exit(void)
{
#ifdef WIN32
    WSACleanup();
#endif	
}

/* sock_read is read() with a timeout of SOCKET_READ_TIMEOUT. */
int sock_read(nsocket *sock, char *buffer, size_t count) 
{
    int ret;

    if (count == 0) {
	NE_DEBUG(NE_DBG_SOCKET, "Passing 0 to sock_read is probably bad.\n");
	/* But follow normal read() semantics anyway... */
	return 0;
    }

    ret = sock_block(sock, SOCKET_READ_TIMEOUT);
    if (ret == 0) {
	/* Got data */
	do {
#ifdef ENABLE_SSL
	    if (sock->ssl) {
		ret = SSL_read(sock->ssl, buffer, count);
	    } else {
#endif
#ifndef BEOS_PRE_BONE
		ret = NEON_READ(sock->fd, buffer, count);
#else
		if (sock->peeked_bytes_avail > 0) {
		    /* simply return the peeked bytes.... */
		    if (count >= sock->peeked_bytes_avail){
			/* we have room */
			strncpy(buffer, sock->peeked_bytes_curpos, 
				sock->peeked_bytes_avail);
			ret = sock->peeked_bytes_avail;
			sock->peeked_bytes_avail = 0;
		    } else {
			strncpy(buffer, sock->peeked_bytes_curpos, count);
			sock->peeked_bytes_curpos += count;
			sock->peeked_bytes_avail -= count;
			ret = count;
		    }
		} else {
		    ret = recv(sock->fd, buffer, count, 0);
		}
#endif
#ifdef ENABLE_SSL
	    }
#endif
	} while (ret == -1 && errno == EINTR);
	if (ret < 0) {
	    sock->error = strerror(errno);
	    ret = SOCK_ERROR;
	} else if (ret == 0) {
	    /* This might not or might not be an error depending on
               the context. */
	    sock->error = _("Connection was closed by server");
	    NE_DEBUG(NE_DBG_SOCKET, "read returned zero.\n");
	    ret = SOCK_CLOSED;
	}
    }
    return ret;
}

/* sock_peek is recv(...,MSG_PEEK) with a timeout of SOCKET_TIMEOUT.
 * Returns length of data read or SOCK_* on error */
int sock_peek(nsocket *sock, char *buffer, size_t count) 
{
    int ret;
    ret = sock_block(sock, SOCKET_READ_TIMEOUT);
    if (ret < 0) {
	return ret;
    }
    /* Got data */
#ifdef ENABLE_SSL
    if (sock->ssl) {
	ret = SSL_peek(sock->ssl, buffer, count);
	/* TODO: This is the fetchmail fix as in sock_readline.
	 * Do we really need it? */
	if (ret == 0) {
	    if (sock->ssl->shutdown) {
		return SOCK_CLOSED;
	    }
	    if (0 != ERR_get_error()) {
		sock->error = ERROR_SSL_STRING;
		return SOCK_ERROR;
	    }
	}
    } else {
#endif
    do {
#ifndef BEOS_PRE_BONE
	ret = recv(sock->fd, buffer, count, MSG_PEEK);
#else /* we're on BeOS pre-BONE so we need to use the buffer... */
	if (sock->peeked_bytes_avail > 0) {
	    /* we've got some from last time!!! */
	    if (count >= sock->peeked_bytes_avail) {
		strncpy(buffer, sock->peeked_bytes_curpos, sock->peeked_bytes_avail);
		ret = sock->peeked_bytes_avail;
	    } else {
		strncpy(buffer, sock->peeked_bytes_curpos, count);
		ret = count;
	    }
	} else {
	    if (count > MAX_PEEK_BUFFER)
		count = MAX_PEEK_BUFFER;
	    ret = recv(sock->fd, buffer, count, 0);
	    sock->peeked_bytes_avail = ret;
	    strncpy(sock->peeked_bytes, buffer, ret);
	    sock->peeked_bytes_curpos = sock->peeked_bytes;
	}
#endif
    } while (ret == -1 && errno == EINTR);
#ifdef ENABLE_SSL
    }
#endif
    /* According to the Single Unix Spec, recv() will return
     * zero if the socket has been closed the other end. */
    if (ret == 0) {
	ret = SOCK_CLOSED;
    } else if (ret < 0) {
	sock->error = strerror(errno);
	ret = SOCK_ERROR;
    } 
    return ret;
}

/* Blocks waiting for read input on the given socket for the given time.
 * Returns:
 *    0 if data arrived
 *    SOCK_TIMEOUT if data did not arrive before timeout
 *    SOCK_ERROR on error
 */
int sock_block(nsocket *sock, int timeout) 
{
    struct timeval tv;
    fd_set fds;
    int ret;

#ifdef ENABLE_SSL
    if (sock->ssl) {
	/* There may be data already available in the 
	 * SSL buffers */
	if (SSL_pending(sock->ssl)) {
	    return 0;
	}
	/* Otherwise, we should be able to select on
	 * the socket as per normal. Probably? */
    }
#endif
#ifdef BEOS_PRE_BONE
    if (sock->peeked_bytes_avail > 0) {
        return 0;
    }
#endif

    /* Init the fd set */
    FD_ZERO(&fds);
    FD_SET(sock->fd, &fds);
    /* Set the timeout */
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    do {
	ret = select(sock->fd+1, &fds, NULL, NULL, &tv);
    } while (ret == -1 && errno == EINTR);

    switch(ret) {
    case 0:
	return SOCK_TIMEOUT;
    case -1:
	sock->error = strerror(errno);
	return SOCK_ERROR;
    default:
	return 0;
    }
}

/* Send the given line down the socket with CRLF appended. 
 * Returns 0 on success or SOCK_* on failure. */
int sock_sendline(nsocket *sock, const char *line) 
{
    char *buffer;
    int ret;
    CONCAT2(buffer, line, "\r\n");
    ret = sock_send_string(sock, buffer);
    free(buffer);
    return ret;
}

int sock_readfile_blocked(nsocket *sock, off_t length,
			  sock_block_reader reader, void *userdata) 
{
    char buffer[BUFSIZ];
    int ret;
    off_t done = 0;
    do {
	ret = sock_read(sock, buffer, BUFSIZ);
	if (ret < 0) {
	    if (length == -1 && ret == SOCK_CLOSED) {
		/* Not an error condition. */
		return 0;
	    }
	    return ret;
	} 
	done += ret;
	sock_call_progress(sock, done, length);
	(*reader)(userdata, buffer, ret);
    } while ((length == -1) || (done < length));
    return 0;
}


/* Send a block of data down the given fd.
 * Returns 0 on success or SOCK_* on failure */
int sock_fullwrite(nsocket *sock, const char *data, size_t length) 
{
    ssize_t wrote;

#ifdef ENABLE_SSL
    if (sock->ssl) {
	/* joe: ssl.h says SSL_MODE_ENABLE_PARTIAL_WRITE must 
	 * be enabled to have SSL_write return < length... 
	 * so, SSL_write should never return < length. */
	wrote = SSL_write(sock->ssl, data, length);
	if (wrote >= 0 && (size_t)wrote < length) {
	    NE_DEBUG(NE_DBG_SOCKET, "SSL_write returned less than length!\n");
	    sock->error = ERROR_SSL_STRING;
	    return SOCK_ERROR;
	}
    } else {
#endif
	const char *pnt = data;
	size_t sent = 0;

	while (sent < length) {
	    wrote = NEON_WRITE(sock->fd, pnt, length-sent);
	    if (wrote < 0) {
		if (errno == EINTR) {
		    continue;
		} else if (errno == EPIPE) {
		    return SOCK_CLOSED;
		} else {
		    sock->error = strerror(errno);
		    return SOCK_ERROR;
		}
	    }
	    sent += wrote;
	    pnt += wrote;
#ifdef ENABLE_SSL
	}
#endif
    }
    return 0;
}

/* Sends the given string down the given socket.
 * Returns 0 on success or -1 on failure. */
int sock_send_string(nsocket *sock, const char *data) 
{
    return sock_fullwrite(sock, data, strlen(data));
}

/* This is from from Eric Raymond's fetchmail (SockRead() in socket.c)
 * since I wouldn't have a clue how to do it properly.
 * This function is Copyright 1999 (C) Eric Raymond.
 * Modifications Copyright 2000 (C) Joe Orton
 */
int sock_readline(nsocket *sock, char *buf, int len)
{
    char *newline, *bp = buf;
    int n;

    do {
	/* 
	 * The reason for these gymnastics is that we want two things:
	 * (1) to read \n-terminated lines,
	 * (2) to return the true length of data read, even if the
	 *     data coming in has embedded NULS.
	 */
#ifdef	ENABLE_SSL

	if (sock->ssl) {
	    /* Hack alert! */
	    /* OK...  SSL_peek works a little different from MSG_PEEK
	       Problem is that SSL_peek can return 0 if there is no
	       data currently available.  If, on the other hand, we
	       loose the socket, we also get a zero, but the SSL_read
	       then SEGFAULTS!  To deal with this, we'll check the
	       error code any time we get a return of zero from
	       SSL_peek.  If we have an error, we bail.  If we don't,
	       we read one character in SSL_read and loop.  This
	       should continue to work even if they later change the
	       behavior of SSL_peek to "fix" this problem...  :-(*/
	    NE_DEBUG(NE_DBG_SOCKET, "SSL readline... \n");
	    if ((n = SSL_peek(sock->ssl, bp, len)) < 0) {
		sock->error = ERROR_SSL_STRING;
		return(-1);
	    }
	    if (0 == n) {
		/* SSL_peek says no data...  Does he mean no data
		   or did the connection blow up?  If we got an error
		   then bail! */
		NE_DEBUG(NE_DBG_SOCKET, "SSL_Peek says no data!\n");
		/* Check properly to see if the connection has closed */
		if (sock->ssl->shutdown) {
		    NE_DEBUG(NE_DBG_SOCKET, "SSL says shutdown.");
		    return SOCK_CLOSED;
		} else if (0 != (n = ERR_get_error())) {
		    NE_DEBUG(NE_DBG_SOCKET, "SSL error occured.\n");
		    sock->error = ERROR_SSL_STRING;
		    return -1;
		}
		    
		/* We didn't get an error so read at least one
		   character at this point and loop */
		n = 1;
		/* Make sure newline start out NULL!  We don't have a
		 * string to pass through the strchr at this point yet
		 * */
		newline = NULL;
	    } else if ((newline = memchr(bp, '\n', n)) != NULL)
		n = newline - bp + 1;
	    n = SSL_read(sock->ssl, bp, n);
	    NE_DEBUG(NE_DBG_SOCKET, "SSL_read returned %d\n", n);
	    if (n == -1) {
		sock->error = ERROR_SSL_STRING;
		return(-1);
	    }
	    /* Check for case where our single character turned out to
	     * be a newline...  (It wasn't going to get caught by
	     * the strchr above if it came from the hack...). */
	    if (NULL == newline && 1 == n && '\n' == *bp) {
		/* Got our newline - this will break
				out of the loop now */
		newline = bp;
	    }
	} else {
#endif
	    if ((n = sock_peek(sock, bp, len)) <= 0)
		return n;
	    if ((newline = memchr(bp, '\n', n)) != NULL)
		n = newline - bp + 1;
	    if ((n = sock_read(sock, bp, n)) < 0)
		return n;
#ifdef ENABLE_SSL
	}
#endif
	bp += n;
	len -= n;
	if (len < 1) {
	    sock->error = _("Line too long");
	    return SOCK_FULL;
	}
    } while (!newline && len);
    *bp = '\0';
    return bp - buf;
}

/*** End of ESR-copyrighted section ***/

/* Reads readlen bytes from fd and write to sock.
 * If readlen == -1, then it reads from srcfd until EOF.
 * Returns number of bytes written to destfd, or -1 on error.
 */
int sock_transfer(int fd, nsocket *sock, off_t readlen) 
{
    char buffer[BUFSIZ];
    size_t curlen; /* total bytes yet to read from srcfd */
    off_t sumwrlen; /* total bytes written to destfd */

    if (readlen == -1) {
	curlen = BUFSIZ; /* so the buffer size test works */
    } else {
	curlen = readlen; /* everything to do */
    }
    sumwrlen = 0; /* nowt done yet */

    while (curlen > 0) {
	int rdlen, wrlen;

	/* Get a chunk... if the number of bytes that are left to read
	 * is less than the buffer size, only read that many bytes. */
	rdlen = read(fd, buffer, (readlen==-1)?BUFSIZ:(min(BUFSIZ, curlen)));
	sock_call_progress(sock, sumwrlen, readlen);
	if (rdlen < 0) { 
	    if (errno == EPIPE) {
		return SOCK_CLOSED;
	    } else {
		sock->error = strerror(errno);
		return SOCK_ERROR;
	    }
	} else if (rdlen == 0) { 
	    /* End of file... get out of here */
	    break;
	}
	if (readlen != -1)
	    curlen -= rdlen;

	/* Otherwise, we have bytes!  Write them to destfd */
	
	wrlen = sock_fullwrite(sock, buffer, rdlen);
	if (wrlen < 0) { 
	    return wrlen;
	}

	sumwrlen += rdlen;
    }
    sock_call_progress(sock, sumwrlen, readlen);
    return sumwrlen;
}

/* Reads buflen bytes into buffer until it's full.
 * Returns 0 on success, -1 on error */
int sock_fullread(nsocket *sock, char *buffer, int buflen) 
{
    char *pnt; /* current position within buffer */
    int len;
    pnt = buffer;
    while (buflen > 0) {
	len = sock_read(sock, pnt, buflen);
	if (len < 0) return len;
	buflen -= len;
	pnt += len;
    }
    return 0;
}

/* Do a name lookup on given hostname, writes the address into
 * given address buffer. Return -1 on failure.
 */
int sock_name_lookup(const char *hostname, struct in_addr *addr) 
{
    struct hostent *hp;
    unsigned long laddr;
    
    /* TODO?: a possible problem here, is that if we are passed an
     * invalid IP address e.g. "500.500.500.500", then this gets
     * passed to gethostbyname and returned as "Host not found".
     * Arguably wrong, but maybe difficult to detect correctly what is
     * an invalid IP address and what is a hostname... can hostnames
     * begin with a numeric character? */
    laddr = (unsigned long)inet_addr(hostname);
    if ((int)laddr == -1) {
	/* inet_addr failed. */
	hp = gethostbyname(hostname);
	if (hp == NULL) {
#if 0
	    /* Need to get this back somehow, but we don't have 
	     * an nsocket * yet... */
	    switch(h_errno) {
	    case HOST_NOT_FOUND:
		sock->error = _("Host not found");
		break;
	    case TRY_AGAIN:
		sock->error = _("Host not found (try again later?)");
		break;
	    case NO_ADDRESS:
		sock->error = _("Host exists but has no address.");
		break;
	    case NO_RECOVERY:
	    default:
		sock->error = _("Non-recoverable error in resolver library.");
		break;
	    }
#endif
	    return SOCK_ERROR;
	}
	memcpy(addr, hp->h_addr, hp->h_length);
    } else {
	addr->s_addr = laddr;
    }
    return 0;
}

static nsocket *create_sock(int fd)
{
    nsocket *sock = ne_calloc(sizeof *sock);
#ifdef ENABLE_SSL
    sock->default_ctx = SSL_CTX_new(SSLv23_client_method());
#endif
    sock->fd = fd;
    return sock;
}

/* Opens a socket to the given port at the given address.
 * Returns -1 on failure, or the socket on success. 
 * portnum must be in HOST byte order */
nsocket *sock_connect(const struct in_addr addr, 
		      unsigned short int portnum)
{
    struct sockaddr_in sa;
    int fd;

    /* Create the socket */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
	return NULL;
    /* Connect the nsocket */
    sa.sin_family = AF_INET;
    sa.sin_port = htons(portnum); /* host -> net byte orders */
    sa.sin_addr = addr;
    if (connect(fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) < 0) {
	(void) NEON_CLOSE(fd);
	return NULL;
    }
    /* Success - return the nsocket */
    return create_sock(fd);
}

nsocket *sock_accept(int listener) 
{
    int fd = accept(listener, NULL, NULL);
    if (fd > 0) {
	return create_sock(fd);
    } else {
	return NULL;
    }
}

int sock_get_fd(nsocket *sock)
{
    return sock->fd;
}

nssl_context *sock_create_ssl_context(void)
{
    nssl_context *ctx = ne_calloc(sizeof *ctx);
#ifdef ENABLE_SSL
    ctx->ctx = SSL_CTX_new(SSLv23_client_method());
#endif
    return ctx;
}

void sock_destroy_ssl_context(nssl_context *ctx)
{
#ifdef ENABLE_SSL
    SSL_CTX_free(ctx->ctx);
#endif
    free(ctx);
}

#ifdef ENABLE_SSL
void sock_disable_tlsv1(nssl_context *c)
{
    SSL_CTX_set_options(c->ctx, SSL_OP_NO_TLSv1);
}
void sock_disable_sslv2(nssl_context *c)
{
    SSL_CTX_set_options(c->ctx, SSL_OP_NO_SSLv2);

}
void sock_disable_sslv3(nssl_context *c)
{
    SSL_CTX_set_options(c->ctx, SSL_OP_NO_SSLv3);
}

/* The callback neon installs with OpenSSL for giving the private key
 * prompt.  FIXME: WTH is 'rwflag'? */
static int key_prompt_cb(char *buf, int len, int rwflag, void *userdata)
{
    nssl_context *ctx = userdata;
    int ret;
    ret = ctx->key_prompt(ctx->key_userdata, ctx->key_file, buf, len);
    if (ret)
	return -1;
    /* Obscurely OpenSSL requires the callback to return the length of
     * the password, this seems a bit weird so we don't expose this in
     * the neon API. */
    return strlen(buf);
}

void sock_set_key_prompt(nssl_context *ctx, 
			 nssl_key_prompt prompt, void *userdata)
{
    SSL_CTX_set_default_passwd_cb(ctx->ctx, key_prompt_cb);
    SSL_CTX_set_default_passwd_cb_userdata(ctx->ctx, ctx);
    ctx->key_prompt = prompt;
    ctx->key_userdata = userdata;
}

#else
void sock_disable_tlsv1(nssl_context *c) {}
void sock_disable_sslv2(nssl_context *c) {}
void sock_disable_sslv3(nssl_context *c) {}
void sock_set_key_prompt(nssl_context *c, nssl_key_prompt p, void *u) {}
#endif

int sock_make_secure(nsocket *sock, nssl_context *ctx)
{
#ifdef ENABLE_SSL
    int ret;
    SSL_CTX *ssl_ctx;

    if (ctx) {
	ssl_ctx = ctx->ctx;
    } else {
	ssl_ctx = sock->default_ctx;
    }

    sock->ssl = SSL_new(ssl_ctx);
    if (!sock->ssl) {
	sock->error = ERROR_SSL_STRING;
	/* Usually goes wrong because: */
	fprintf(stderr, "Have you called sock_init()!?\n");
	return SOCK_ERROR;
    }
    
#ifdef SSL_MODE_AUTO_RETRY
    /* OpenSSL 0.9.6 and later... */
    (void) SSL_set_mode(sock->ssl, SSL_MODE_AUTO_RETRY);
#endif

    SSL_set_fd(sock->ssl, sock->fd);
    
    ret = SSL_connect(sock->ssl);
    if (ret == -1) {
	sock->error = ERROR_SSL_STRING;
	SSL_free(sock->ssl);
	sock->ssl = NULL;
	return SOCK_ERROR;
    }

#if 0
    /* Tommi Komulainen <Tommi.Komulainen@iki.fi> has donated his SSL
     * cert verification from the mutt IMAP/SSL code under the
     * LGPL... it will plug in here */
    ret = sock_check_certicate(sock);
    if (ret) {
	SSL_shutdown(sock->ssl);
	SSL_free(sock->ssl);
	sock->ssl = NULL;
	return ret;
    }
#endif

    NE_DEBUG(NE_DBG_SOCKET, "SSL connected: version %s\n", 
	   SSL_get_version(sock->ssl));
    return 0;
#else
    sock->error = _("This application does not have SSL support.");
    return SOCK_ERROR;
#endif
}

const char *sock_get_version(nsocket *sock)
{
#ifdef ENABLE_SSL
    return SSL_get_version(sock->ssl);
#else
    return NULL;
#endif
}

const char *sock_get_error(nsocket *sock)
{
    return sock->error;
}

/* Closes given nsocket */
int sock_close(nsocket *sock) {
    int ret;
#ifdef ENABLE_SSL
    if (sock->ssl) {
	SSL_shutdown(sock->ssl);
	SSL_free(sock->ssl);
    }
    SSL_CTX_free(sock->default_ctx);
#endif
    ret = NEON_CLOSE(sock->fd);
    free(sock);
    return ret;
}

/* FIXME: get error messages back to the caller. */   
int sock_set_client_cert(nssl_context *ctx, const char *cert, const char *key)
{
#ifdef ENABLE_SSL
    if (SSL_CTX_use_certificate_file(ctx->ctx, cert, SSL_FILETYPE_PEM) <= 0) {
	NE_DEBUG(NE_DBG_SOCKET, "Could not load cert file.\n");
	return -1;
    }
    
    /* The cert file can contain the private key too, apparently. Not
     * sure under what circumstances this is sensible, but hey. */
    if (key == NULL)
	key = cert;
    
    /* Set this so the callback can tell the user what's going on. */
    ctx->key_file = key;
    
    if (SSL_CTX_use_PrivateKey_file(ctx->ctx, key, SSL_FILETYPE_PEM) <= 0) {
	NE_DEBUG(NE_DBG_SOCKET, "Could not load private key file.\n");
	return -1;
    }

    /* Sanity check. */
    if (!SSL_CTX_check_private_key(ctx->ctx)) {
	NE_DEBUG(NE_DBG_SOCKET, "Private key does not match certificate.\n");
	return -1;
    }

    return 0;
#else
    return -1;
#endif
}

/* Returns HOST byte order port of given name */
int sock_service_lookup(const char *name) {
    struct servent *ent;
    ent = getservbyname(name, "tcp");
    if (ent == NULL) {
	return 0;
    } else {
	return ntohs(ent->s_port);
    }
}
