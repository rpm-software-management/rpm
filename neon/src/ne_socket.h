/* 
   socket handling interface
   Copyright (C) 1999-2001, Joe Orton <joe@light.plus.com>

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

*/

#ifndef NE_SOCKET_H
#define NE_SOCKET_H

#ifdef WIN32
#include <WinSock2.h>
#include <stddef.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "ne_defs.h"

BEGIN_NEON_DECLS

#define SOCK_ERROR -1
/* Read/Write timed out */
#define SOCK_TIMEOUT -2
/* Passed buffer was full */
#define SOCK_FULL -3
/* Socket was closed */
#define SOCK_CLOSED -4

/* Socket read timeout */
#define SOCKET_READ_TIMEOUT 120

struct nsocket_s;
typedef struct nsocket_s nsocket;

typedef void (*sock_block_reader) (
    void *userdata, const char *buf, size_t len);

typedef void (*sock_progress)(void *userdata, off_t progress, off_t total);

void sock_register_progress(nsocket *sock, sock_progress cb, void *userdata);

void sock_call_progress(nsocket *sock, off_t progress, off_t total);

/* Initialize the socket library. If you don't do this, SSL WILL NOT WORK.
 * Returns 0 on success, or non-zero on screwed up SSL library. */
int sock_init(void);

/* Shutdown the socket library. */
void sock_exit(void);

/* sock_read is read() with a timeout of SOCKET_TIMEOUT.
 * Returns:
 *   SOCK_* on error,
 *    0 on no data to read (due to EOF),
 *   >0 length of data read into buffer.
 */
int sock_read(nsocket *sock, char *buffer, size_t count);

/* sock_peek is recv() with a timeout of SOCKET_TIMEOUT.
 * Returns:
 *   SOCK_* on error,
 *    0 on no data to read (due to EOF),
 *   >0 length of data read into buffer.
 */
int sock_peek(nsocket *sock, char *buffer, size_t count);

/* Blocks waiting for data on the given socket for the given time.
 * Returns:
 *  SOCK_* on error,
 *  SOCK_TIMEOUT on no data within timeout,
 *  0 if data arrived on the socket.
 */
int sock_block(nsocket *sock, int timeout);

/* Reads readlen bytes from fd and writes to socket.
 * (Not all in one go, obviously).
 * If readlen == -1, then it reads from srcfd until EOF.
 * Returns number of bytes written to destfd, or SOCK_* on error.
 */
int sock_transfer(int fd, nsocket *sock, off_t readlen);

/* Sends the given line to given socket, CRLF appended */
int sock_sendline(nsocket *sock, const char *line); 
/* Sends the given block of data down the nsocket */
int sock_fullwrite(nsocket *sock, const char *data, size_t length); 
/* Sends the null-terminated string down the given nsocket */
int sock_send_string(nsocket *sock, const char *string); 

/* Reads a line from given nsocket */
int sock_readline(nsocket *sock, char *line, int len); 
/* Reads a chunk of data. */
int sock_fullread(nsocket *sock, char *buffer, int buflen);

/* Creates and connects a nsocket */
nsocket *sock_connect(const struct in_addr host, 
		      unsigned short int portnum);

/* Not as good as accept(2), missing parms 2+3.
 * Addings parms 2+3 would probably mean passing socklen_t as an
 * int then casting internally, since we don't really want to
 * autogenerate the header file to be correct for the build platform.
 */
nsocket *sock_accept(int listener);

/* Returns the file descriptor used for the socket */
int sock_get_fd(nsocket *sock);

/* Closes the socket and frees the nsocket object. */
int sock_close(nsocket *sock);

const char *sock_get_version(nsocket *sock);

const char *sock_get_error(nsocket *sock);

/* Do a name lookup on given hostname, writes the address into
 * given address buffer. Return -1 on failure. */
int sock_name_lookup(const char *hostname, struct in_addr *addr);

/* Returns the standard TCP port for the given service */
int sock_service_lookup(const char *name);

/* Read from socket, passing each block read to reader callback.
 * Pass userdata as first argument to reader callback.
 *
 * If length is -1, keep going till EOF is returned. SOCK_CLOSED
 * is never returned in this case.
 *
 * Otherwise, read exactly 'length' bytes. If EOF is encountered
 * before length bytes have been read, and SOCK_CLOSED will be
 * returned.
 *
 * Returns:
 *   0 on success,
 *   SOCK_* on error (SOCK_CLOSED is a special case, as above)
 */
int sock_readfile_blocked(nsocket *sock, off_t length,
			  sock_block_reader reader, void *userdata);

/* Auxiliary for use with SSL. */
struct nssl_context_s;
typedef struct nssl_context_s nssl_context;

/* Netscape's prompts on getting a certificate which it doesn't
 * recognize the CA for:
 *   1. Hey, I don't recognize the CA for this cert.
 *   2. Here is the certificate: for foo signed by BLAH,
 *      using encryption level BLEE
 *   3. Allow: accept for this session only, 
 *             don't accept
 *             accept forever
 */
nssl_context *sock_create_ssl_context(void);

void sock_destroy_ssl_context(nssl_context *ctx);

/* Callback to decide whether the user will accept the
 * given certificate or not */
typedef struct {
    char *owner; /* Multi-line string describing owner of
		  * certificate */
    char *issuer; /* As above for issuer of certificate */
    /* Strings the certificate is valid between */
    char *valid_from, *valid_till;
    /* Certificate fingerprint */
    char *fingerprint;
} nssl_certificate;

/* Returns:
 *   0 -> User accepts the certificate
 *   non-zero -> user does NOT accept the certificate.
 */
typedef int (*nssl_accept)(void *userdata, const nssl_certificate *info);

void sock_set_cert_accept(nssl_context *c, 
			  nssl_accept accepter, void *userdata);

/* Callback for retrieving the private key password.
 * Filename will be the filename of the private key file.
 * Must return:
 *    0 on success.  buf must be filled in with the password.
 *  non-zero if the user cancelled the prompt.
 *
 * FIXME: this is inconsistent with the HTTP authentication callbacks.  */
typedef int (*nssl_key_prompt)(void *userdata, const char *filename,
			       char *buf, int buflen);

void sock_set_key_prompt(nssl_context *c, 
			 nssl_key_prompt prompt, void *userdata);

/* For PEM-encoded client certificates: use the given client
 * certificate and private key file. 
 * Returns: 0 if certificate is read okay,
 * non-zero otherwise.

 * For decoding the private key file, the callback above will be used
 * to prompt for the password.  If no callback has been set, then the
 * OpenSSL default will be used: the prompt appears on the terminal.
 * */
int sock_set_client_cert(nssl_context *ctx, const char *certfile,
			 const char *keyfile);

void sock_disable_tlsv1(nssl_context *c);
void sock_disable_sslv2(nssl_context *c);
void sock_disable_sslv3(nssl_context *c);

/* Ctx is OPTIONAL. If it is NULL, defaults are used. */
int sock_make_secure(nsocket *sock, nssl_context *ctx);

END_NEON_DECLS

#endif /* NE_SOCKET_H */
