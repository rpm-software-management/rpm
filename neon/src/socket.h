/* 
   socket handling routines
   Copyright (C) 1998, Joe Orton <joe@orton.demon.co.uk>, except where
   otherwise indicated.

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

#ifndef SOCKET_H
#define SOCKET_H

#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>

#define SOCK_ERROR -1
/* Read/Write timed out */
#define SOCK_TIMEOUT -2
/* Passed buffer was full */
#define SOCK_FULL -3
/* Socket was closed */
#define SOCK_CLOSED -4

/* Socket read timeout */
#define SOCKET_READ_TIMEOUT 30

typedef enum {
    sock_namelookup, /* Looking up hostname given by info */
    sock_connecting, /* Connecting to server */
    sock_connected /* Connection established */
} sock_status;

typedef void (*sock_block_reader) ( 
    void *userdata, const char *buf, size_t len );

typedef void (*sock_progress)( void *userdata, size_t progress, size_t total );
typedef void (*sock_notify)( void *userdata, sock_status status, const char *info );

void sock_register_progress( sock_progress cb, void *userdata );
void sock_register_notify( sock_notify cb, void *userdata );

void sock_call_progress( size_t progress, size_t total );

/* sock_read is read() with a timeout of SOCKET_TIMEOUT.
 * Returns:
 *   SOCK_ERROR on error,
 *   SOCK_TIMEOUT on timeout,
 *    0 on no data to read (due to timeout or EOF),
 *   >0 length of data read into buffer.
 */
int sock_read( int sock, void *buffer, size_t count );

/* sock_recv is recv() with a timeout of SOCKET_TIMEOUT.
 * Returns:
 *   -1 on error,
 *    0 on no data to read (due to timeout or EOF),
 *   >0 length of data read into buffer.
 */
int sock_recv( int sock, void *buffer, size_t count, unsigned int flags);

/* Blocks waiting for data on the given socket for the given time.
 * Returns:
 *  -1 on error,
 *   0 on no data within timeout,
 *   1 if data arrived on the socket.
 */
int sock_block( int sock, int timeout );

/* Dump the given filename down the given socket.
 * Returns 0 on success, non-zero on error */
int send_file_binary( int sock, const char *filename ); 

/* Dump the given filename down the given socket, performing
 * CR -> CRLF conversion.
 * Returns 0 on success, non-zero on error */
int send_file_ascii( int sock, const char *filename ); 

/* Dump from given socket into given file. Reads only filesize bytes,
 * or until EOF if filesize == -1.
 * Returns number of bytes written on success, or -1 on error */
int recv_file( int sock, const char *filename, ssize_t filesize ); 

/* Reads readlen bytes from srcfd and writes to destfd.
 * (Not all in one go, obviously).
 * If readlen == -1, then it reads from srcfd until EOF.
 * Returns number of bytes written to destfd, or -1 on error.
 * Calls fe_transfer_progress( a, b ) during transfers, where
 *  a = bytes transferred so far, and b = readlen
 */
int sock_transfer( int srcfd, int destfd, ssize_t readlen );

/* Sends the given line to given socket, CRLF appended */
int sock_sendline( int sock, const char *line ); 
/* Sends the given block of data down the socket */
int sock_fullwrite( int sock, const char *data, size_t length ); 
/* Sends the null-terminated string down the given socket */
int sock_send_string( int sock, const char *string ); 

/* Reads a line from given socket */
int sock_readline( int sock, char *line, int len ); 
/* Reads a chunk of data. */
int sock_fullread( int sock, char *buffer, int buflen );

/* Creates and connects a socket */
int sock_connect( const struct in_addr host, int portnum );

int sock_connect_u( const struct in_addr addr, int portnum, int call_fe );

int sock_close( int socket );

/* Do a name lookup on given hostname, writes the address into
 * given address buffer. Return -1 on failure.
 */
int host_lookup( const char *hostname, struct in_addr *addr );

/* Returns the standard TCP port for the given service */
int get_tcp_port( const char *name );

int sock_readfile_blocked( int fd, size_t length,
			   sock_block_reader reader, void *userdata );

#endif /* SOCKET_H */
