/* 
   socket handling routines
   Copyright (C) 1998, 1999, 2000, Joe Orton <joe@orton.demon.co.uk>, 
   except where otherwise indicated.

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

   Id: socket.c,v 1.7 2000/05/10 13:24:42 joe Exp  
*/

#include <config.h>

#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/socket.h>
#include <sys/stat.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>

#include <fcntl.h>
#include <stdio.h>
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

#include "string_utils.h"
#include "http_utils.h"
#include "socket.h"

static sock_progress progress_cb = NULL;
static sock_notify notify_cb = NULL;
static void *progress_ud, *notify_ud;

void sock_register_progress( sock_progress cb, void *userdata )
{
    progress_cb = cb;
    progress_ud = userdata;
}

void sock_register_notify( sock_notify cb, void *userdata )
{
    notify_cb = cb;
    notify_ud = userdata;
}

void sock_call_progress( size_t progress, size_t total )
{
    if( progress_cb ) {
	(*progress_cb)( progress_ud, progress, total );
    }
}

/* sock_read is read() with a timeout of SOCKET_READ_TIMEOUT. */
int sock_read( int sock, void *buffer, size_t count ) {
    int ret;
    ret = sock_block( sock, SOCKET_READ_TIMEOUT );
    if( ret == 0 ) {
	/* Got data */
	do {
	    ret = read( sock, buffer, count );
	} while( ret == -1 && errno == EINTR );
	if( ret < 0 ) {
	    ret = SOCK_ERROR;
	}
    }
    return ret;
}

/* sock_recv is recv() with a timeout of SOCKET_TIMEOUT.
 * Returns length of data read or SOCK_* on error */
int sock_recv( int sock, void *buffer, size_t count, unsigned int flags ) {
    int ret;
    ret = sock_block( sock, SOCKET_READ_TIMEOUT );
    if( ret < 0 ) {
	return ret;
    }
    /* Got data */
    do {
	ret = recv( sock, buffer, count, flags );
    } while( ret == -1 && errno == EINTR );
    if( ret < 0 ) {
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
int sock_block( int sock, int timeout ) {
    struct timeval tv;
    fd_set fds;
    int ret;
    
    /* Init the fd set */
    FD_ZERO( &fds );
    FD_SET( sock, &fds );
    /* Set the timeout */
    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    do {
	ret = select( sock+1, &fds, NULL, NULL, &tv );
    } while( ret == -1 && errno == EINTR );

    switch( ret ) {
    case 0:
	return SOCK_TIMEOUT;
    case -1:
	return SOCK_ERROR;
    default:
	return 0;
    }
}

/* Send the given line down the socket with CRLF appended. 
 * Returns 0 on success or SOCK_* on failure. */
int sock_sendline( int sock, const char *line ) {
    char *buffer;
    int ret;
    CONCAT2( buffer, line, "\r\n" );
    ret = sock_send_string( sock, buffer );
    free( buffer );
    return ret;
}

/* Reads from fd, passing blocks to reader, also calling
 * fe_t_p. 
 * Returns 0 on success or SOCK_* on error. */
int sock_readfile_blocked( int fd, size_t length,
			   sock_block_reader reader, void *userdata ) {
    char buffer[BUFSIZ];
    int ret;
    size_t done = 0;
    do {
	ret = sock_read( fd, buffer, BUFSIZ );
	if( ret < 0 ) {
	    return ret;
	} 
	done += ret;
	sock_call_progress( done, length );
	(*reader)( userdata, buffer, ret );
    } while( (done < length) && ret );
    return 0;
}


/* Send a block of data down the given fd.
 * Returns 0 on success or SOCK_* on failure */
int sock_fullwrite( int fd, const char *data, size_t length ) {
    ssize_t sent, wrote;
    const char *pnt;

    sent = 0;
    pnt = data;
    while( sent < length ) {
	wrote = write( fd, pnt, length-sent );
	if( wrote < 0 ) {
	    if( errno == EINTR ) {
		continue;
	    } else if( errno == EPIPE ) {
		return SOCK_CLOSED;
	    } else {
		return SOCK_ERROR;
	    }
	}
	sent += wrote;
    }
    return 0;
}

/* Sends the given string down the given socket.
 * Returns 0 on success or -1 on failure. */
int sock_send_string( int sock, const char *data ) {
    return sock_fullwrite( sock, data, strlen( data ) );
}

/* This is from from Eric Raymond's fetchmail (SockRead() in socket.c)
 * since I wouldn't have a clue how to do it properly.
 * This function is Copyright 1999 (C) Eric Raymond.
 * Modifications Copyright 2000 (C) Joe Orton
 */
int sock_readline(int sock, char *buf, int len)
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
#ifdef	SSL_ENABLE

	/* joe: of course, we don't actually have SSL support here yet,
	 * but this will be handy when that day comes. */

	SSL *ssl;

	if( NULL != ( ssl = SSLGetContext( sock ) ) ) {
		/* Hack alert! */
		/* OK...  SSL_peek works a little different from MSG_PEEK
			Problem is that SSL_peek can return 0 if there
			is no data currently available.  If, on the other
			hand, we loose the socket, we also get a zero, but
			the SSL_read then SEGFAULTS!  To deal with this,
			we'll check the error code any time we get a return
			of zero from SSL_peek.  If we have an error, we bail.
			If we don't, we read one character in SSL_read and
			loop.  This should continue to work even if they
			later change the behavior of SSL_peek
			to "fix" this problem...  :-(	*/
		if ((n = SSL_peek(ssl, bp, len)) < 0) {
			return(-1);
		}
		if( 0 == n ) {
			/* SSL_peek says no data...  Does he mean no data
			or did the connection blow up?  If we got an error
			then bail! */
			if( 0 != ( n = ERR_get_error() ) ) {
				return -1;
			}
			/* We didn't get an error so read at least one
				character at this point and loop */
			n = 1;
			/* Make sure newline start out NULL!
			 * We don't have a string to pass through
			 * the strchr at this point yet */
			newline = NULL;
		} else if ((newline = memchr(bp, '\n', n)) != NULL)
			n = newline - bp + 1;
		if ((n = SSL_read(ssl, bp, n)) == -1) {
			return(-1);
		}
		/* Check for case where our single character turned out to
		 * be a newline...  (It wasn't going to get caught by
		 * the strchr above if it came from the hack...  ). */
		if( NULL == newline && 1 == n && '\n' == *bp ) {
			/* Got our newline - this will break
				out of the loop now */
			newline = bp;
		}
	} else {
		if ((n = recv(sock, bp, len, MSG_PEEK)) <= 0)
			return(-1);
		if ((newline = memchr(bp, '\n', n)) != NULL)
			n = newline - bp + 1;
		if ((n = read(sock, bp, n)) == -1)
			return(-1);
	}
#else
	if ((n = sock_recv(sock, bp, len, MSG_PEEK)) <= 0)
	    return n;
	if ((newline = memchr(bp, '\n', n)) != NULL)
	    n = newline - bp + 1;
	if ((n = sock_read(sock, bp, n)) < 0)
	    return n;
#endif
	bp += n;
	len -= n;
	if( len < 1 ) return SOCK_FULL;
    } while (!newline && len);
    *bp = '\0';
    return bp - buf;
}

/*** End of ESR-copyrighted section ***/

/* Reads readlen bytes from srcfd and writes to destfd.
 * (Not all in one go, obviously).
 * If readlen == -1, then it reads from srcfd until EOF.
 * Returns number of bytes written to destfd, or -1 on error.
 */
int sock_transfer( int srcfd, int destfd, ssize_t readlen ) {
    char buffer[BUFSIZ], *pnt;
    size_t curlen; /* total bytes yet to read from srcfd */
    size_t sumwrlen; /* total bytes written to destfd */

    if( readlen == -1 ) {
	curlen = BUFSIZ; /* so the buffer size test works */
    } else {
	curlen = readlen; /* everything to do */
    }
    sumwrlen = 0; /* nowt done yet */

    while( curlen > 0 ) {
	int rdlen, len2;

	/* Get a chunk... if the number of bytes that are left to read
	 * is less than the buffer size, only read that many bytes. */
	rdlen = sock_read( srcfd, buffer, 
			   (readlen==-1)?BUFSIZ:(min( BUFSIZ, curlen )) );
	sock_call_progress( sumwrlen, readlen );
	if( rdlen < 0 ) { 
	    return -1;
	} else if( rdlen == 0 ) { 
	    /* End of file... get out of here */
	    break;
	}
	if( readlen != -1 )
	    curlen -= rdlen;
	/* Otherwise, we have bytes!  Write them to destfd... might
	 * only manage to write a few of them at a time, so we have
	 * to deal with that too. */
	/* Replace this with a call to send_data? */
	
	pnt = buffer;
	len2 = rdlen;
	while( len2 > 0 ) {
	    ssize_t wrlen;
	    wrlen = write( destfd, pnt, len2 );
	    if( wrlen < 0 ) { 
		return SOCK_ERROR;
	    }
	    len2 -= wrlen;
	    pnt += wrlen;
	    sumwrlen += wrlen;
	}
    }
    sock_call_progress( sumwrlen, readlen );
    return sumwrlen;
}

/* Reads buflen bytes into buffer until it's full.
 * Returns 0 on success, -1 on error */
int sock_fullread( int sock, char *buffer, int buflen ) {
    char *pnt; /* current position within buffer */
    int len;
    pnt = buffer;
    while( buflen > 0 ) {
	len = sock_read( sock, pnt, buflen );
	if( len < 0 ) return len;
	buflen -= len;
	pnt += len;
    }
    return 0;
}

/* Dump the given filename down the given socket.
 * Returns 0 on success or non-zero on error. */
int send_file_binary( int sock, const char *filename ) {
    int fd, wrote;
    struct stat fs;

#if defined (__EMX__) || defined(__CYGWIN__)
    if( (fd = open( filename, O_RDONLY | O_BINARY )) < 0 ) {
#else
    if( (fd = open( filename, O_RDONLY )) < 0 ) {
#endif     
	return -1;
    }
    if( fstat( fd, &fs ) < 0 ) {
	close( fd );
	return -2;
    }
    /* What's the Right Thing to do? Choices:
     *  a) Let transfer send everything from fd until EOF
     *    + If the EOF pos changes, we'll know and can signal an error
     *    - Unsafe - the transfer might not end if someone does 
     *        yes > file
     *  b) Tell transfer to send only the number of bytes from the stat()
     *    + Safe - the transfer WILL end.
     *    - If the EOF pos changes, we'll have a partial (corrupt) file.
     * I'm not sure. I think (a) gets my vote but it doesn't allow
     * nice transfer progress bars in the FE under the current API
     * so we go with (b).
     */
    wrote = sock_transfer( fd, sock, fs.st_size );
    close( fd ); /* any point in checking that one? */
    if( wrote == fs.st_size ) {
	return 0;
    } else {
	return -1;
    }
}

/* Dump the given filename down the given socket, in ASCII translation
 * mode.  Performs LF -> CRLF conversion.
 * Returns zero on success or non-zero on error. */
int send_file_ascii( int sock, const char *filename ) {
    int ret;
    char buffer[BUFSIZ], *pnt;
    FILE *f;
    ssize_t total = 0;
    
    f = fopen( filename, "r" );
    if( f == NULL ) {
	return -1;
    }

    /* Init to success */
    ret = 0;

    while(1) {
	if( fgets( buffer, BUFSIZ - 1, f ) == NULL ) {
	    if( ferror( f ) ) {
		ret = -1;
		break;
	    }
	    /* Finished upload */
	    ret = 0;
	    break;
	}
	/* To send in ASCII mode, we need to send CRLF as the EOL.
	 * We might or might not already have CRLF-delimited lines.
	 * So we mess about a bit to ensure that we do.
	 */
	pnt = strchr( buffer, '\r' );
	if( pnt == NULL ) {
	    /* We need to add the CR in */
	    pnt = strchr( buffer, '\n' );
	    if( pnt == NULL ) {
		/* No CRLF found at all */
		pnt = strchr( buffer, '\0' );
		if( pnt == NULL ) /* crud in buffer */
		    pnt = buffer;
	    }
	    /* Now, pnt points to the first character after the 
	     * end of the line, i.e., where we want to put the CR.
	     */
	    *pnt++ = '\r';
	    /* And lob in an LF afterwards */
	    *pnt-- = '\n';
	}
	/* At this point, pnt points to the CR.
	 * We send everything between pnt and the beginning of the buffer,
	 * +2 for the CRLF
	 */
	if( sock_fullwrite( sock, buffer, (pnt - buffer) +2 ) != 0 ) {
	    ret = -1;
	    break;
	}
	total += (pnt - buffer) + 2;
	sock_call_progress( total, -1 );
    }
    fclose( f ); /* any point in checking that one? */
    /* Return true */
    return ret;
}

/* Dump from given socket into given file. Reads only filesize bytes,
 * or until EOF if filesize == -1.
 * Returns number of bytes written on success, or -1 on error */
int recv_file( int sock, const char *filename, ssize_t filesize ) {
    int fd, wrote;
#if defined (__EMX__) || defined(__CYGWIN__)
    /* We have to set O_BINARY, thus need open(). Otherwise it should be
       equivalent to creat(). */
    if( (fd = open( filename, O_WRONLY|O_TRUNC|O_CREAT|O_BINARY, 0644 )) < 0 ) {
	return -1;
    }
#else
    if( (fd = creat( filename, 0644 )) < 0 ) {
	return -1;
    }
#endif
    wrote = sock_transfer( sock, fd, filesize );
    if( close( fd ) == -1 ) {
	/* Close failed - file was not written correctly */
	return -1;
    }
    if( filesize == -1 ) {
	return wrote;
    } else {
	return (wrote==filesize);
    }
}

/* Do a name lookup on given hostname, writes the address into
 * given address buffer. Return -1 on failure.
 */
int host_lookup( const char *hostname, struct in_addr *addr ) {
    struct hostent *hp;
    unsigned long laddr;
    
    if( notify_cb )
	(*notify_cb)( notify_ud, sock_namelookup, hostname );
    
    laddr = (unsigned long)inet_addr(hostname);
    if ((int)laddr == -1) {
	/* inet_addr failed. */
	hp = gethostbyname(hostname);
	if( hp == NULL ) {
	    return -1;
	}
	memcpy( addr, hp->h_addr, hp->h_length );
    } else {
	addr->s_addr = laddr;
    }
    return 0;
}

/* Opens a socket to the given port at the given address.
 * Returns -1 on failure, or the socket on success. 
 * portnum must be in HOST byte order */
int sock_connect_u( const struct in_addr addr, int portnum, int call_fe ) {
    struct sockaddr_in sa;
    int sock;
    /* Create the socket */
    if( ( sock = socket(AF_INET, SOCK_STREAM, 0) ) < 0)
	return -1;
    /* Connect the socket */
    sa.sin_family = AF_INET;
    sa.sin_port = htons(portnum); /* host -> net byte orders */
    sa.sin_addr = addr;
    if( call_fe && notify_cb ) (*notify_cb)( notify_ud, sock_connecting, NULL );
    if( connect(sock, (struct sockaddr *) &sa, sizeof(struct sockaddr_in)) < 0 )
	return -1;
    if( call_fe && notify_cb ) (*notify_cb)( notify_ud, sock_connected, NULL );
    /* Success - return the socket */
    return sock;
}

int sock_connect( const struct in_addr addr, int portnum ) {
    return sock_connect_u( addr, portnum, 1 );
}

/* Closes given socket */
int sock_close( int sock ) {
    return close( sock );
}

/* Returns HOST byte order port of given name */
int get_tcp_port( const char *name ) {
    struct servent *ent;
    ent = getservbyname( name, "tcp" );
    if( ent == NULL ) {
	return 0;
    } else {
	return ntohs( ent->s_port );
    }
}
