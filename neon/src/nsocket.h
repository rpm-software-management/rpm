/* 
   socket handling interface
   Copyright (C) 1998-2000, Joe Orton <joe@orton.demon.co.uk>

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

#ifndef NSOCKET_H
#define NSOCKET_H

#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>

#include <neon_defs.h>

BEGIN_NEON_DECLS

#define SOCK_ERROR -1
/* Read/Write timed out */
#define SOCK_TIMEOUT -2
/* Passed buffer was full */
#define SOCK_FULL -3
/* Socket was closed */
#define SOCK_CLOSED -4

/* Socket read timeout */
#define SOCKET_READ_TIMEOUT 60

typedef enum {
    sock_namelookup, /* Looking up hostname given by info */
    sock_connecting, /* Connecting to server */
    sock_connected, /* Connection established */
    sock_secure_details /* Secure connection details */
} sock_status;

struct nsocket_s;
typedef struct nsocket_s nsocket;

typedef void (*sock_block_reader) (
    void *userdata, const char *buf, size_t len);

typedef void (*sock_progress)(void *userdata, size_t progress, size_t total);
typedef void (*sock_notify)(void *userdata, 
			    sock_status status, const char *info);

void sock_register_progress(sock_progress cb, void *userdata);
void sock_register_notify(sock_notify cb, void *userdata);

void sock_call_progress(size_t progress, size_t total);

/* Initialize the socket library. If you don't do this, SSL WILL NOT WORK.
 * Returns 0 on success, or non-zero on screwed up SSL library. */
int sock_init(void);

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
int sock_transfer(int fd, nsocket *sock, ssize_t readlen);

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
nsocket *sock_connect(const struct in_addr host, int portnum);

nsocket *sock_connect_u(const struct in_addr addr, int portnum, int call_fe);

/* Not as good as accept(2), missing parms 2+3.
 * Addings parms 2+3 would probably mean passing socklen_t as an
 * int then casting internally, since we don't really want to
 * autogenerate the header file to be correct for the build platform.
 */
nsocket *sock_accept(int listener);

/* Returns the file descriptor used for the socket */
int sock_get_fd(nsocket *sock);

/* Closes the socket and frees the nsocket object. */
int sock_close(nsocket *socket);

const char *sock_get_error(nsocket *sock);

/* Do a name lookup on given hostname, writes the address into
 * given address buffer. Return -1 on failure. */
int sock_name_lookup(const char *hostname, struct in_addr *addr);

/* Returns the standard TCP port for the given service */
int sock_service_lookup(const char *name);

int sock_readfile_blocked(nsocket *socket, size_t length,
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
void sock_set_certificate_file(nssl_context *c, const char *fname);

void sock_disable_tlsv1(nssl_context *c);
void sock_disable_sslv2(nssl_context *c);
void sock_disable_sslv3(nssl_context *c);

/* Ctx is OPTIONAL. If it is NULL, defaults are used. */
int sock_make_secure(nsocket *sock, nssl_context *ctx);

END_NEON_DECLS

#endif /* NSOCKET_H */
