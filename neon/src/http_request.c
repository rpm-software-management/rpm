/* 
   HTTP request/response handling
   Copyright (C) 1999-2000, Joe Orton <joe@orton.demon.co.uk>

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

   Id: http_request.c,v 1.24 2000/05/10 13:24:08 joe Exp 
*/

/* This is the HTTP client request/response implementation.
 * The goal of this code is to be modular and simple.
 */

/* TODO:
 *  - Implement request hooks
 *  - Move DAV locks into a hook
 *  - Move authentication into a hook
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef __EMX__
#include <sys/select.h>
#endif

#include <netinet/in.h>

#include <ctype.h>
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

#ifndef HAVE_SNPRINTF
#include <snprintf.h>
#endif

#include <netinet/in.h>

#include "neon_i18n.h"

#include "xalloc.h"
#include "http_request.h"
#include "http_auth.h"
#include "socket.h"
#include "string_utils.h" /* for sbuffer */
#include "http_utils.h"
#include "uri.h"
#include "neon.h" /* for NEON_VERSION */

struct host_info {
    const char *hostname;
    int port;
    struct in_addr addr;
    char *hostport; /* URI hostport segment */
    http_auth_session auth;
    http_request_auth auth_callback;
    void *auth_userdata;
};

typedef enum {
    body_buffer,
    body_stream,
    body_none
} request_body;

/* This is called with each of the headers in the response */
struct header_handler {
    const char *name;
    http_header_handler handler;
    void *userdata;
    struct header_handler *next;
};

/* TODO: could unify these all into a generic callback list */

struct body_reader {
    http_block_reader handler;
    http_accept_response accept_response;
    unsigned int use:1;
    void *userdata;
    struct body_reader *next;
};

/* Session support. */
struct http_session_s {
    /* Connection information */
    int socket;

    struct host_info server, proxy;

    unsigned int connected:1;

    /* Settings */
    unsigned int have_proxy:1; /* do we have a proxy server? */
    unsigned int no_persist:1; /* set to disable persistent connections */
    int expect100_works:2; /* known state of 100-continue support */

    char *user_agent; /* full User-Agent string */

    /* The last HTTP-Version returned by the server */
    int version_major;
    int version_minor;

    /* Error string */
    char error[BUFSIZ];
};

struct hook {
    http_request_hooks *handler;
    void *private;
    struct hook *next;
};

struct http_req_s {
    /* Fill in these with http_req_init */
    const char *method;
    char *uri;
    char *abs_path;
    
    /*** Request ***/

    sbuffer headers;
    /* Set these if you want to send a request body */
    request_body body;
    FILE *body_stream;
    const char *body_buffer;
    size_t body_size;

    /**** Response ***/

    /* The transfer encoding types */
    struct http_response {
	enum {
	    te_none,
	    te_chunked,
	    te_unknown
	} te;
	int length;            /* Response entity-body content-length */
	int left;              /* Bytes left to read */
	long int chunk_left;   /* Bytes of chunk left to read */
    } resp;

    struct header_handler *header_handlers;
    struct body_reader *body_readers;

    /*** Miscellaneous ***/
    unsigned int method_is_head:1;
    unsigned int use_proxy:1;
    unsigned int use_expect100:1;
    unsigned int can_persist:1;
    unsigned int forced_close:1;

    http_session *session;
    http_status *status; /* TODO: get rid of this */

#ifdef USE_DAV_LOCKS
    /* TODO: move this to hooks... list of locks to submit */
    struct dav_submit_locks *if_locks;
#endif

};
#define HTTP_PORT 80

#define HTTP_EXPECT_TIMEOUT 15
/* 100-continue only used if size > HTTP_EXPECT_MINSIZ */
#define HTTP_EXPECT_MINSIZE 1024

#define HTTP_VERSION_PRE11(s) \
((s)->version_major<1 || ((s)->version_major==1 && (s)->version_minor<1))

#define HTTP_MAXIMUM_HEADER_LENGTH 8192

#define NEON_USERAGENT "neon/" NEON_VERSION;

static void te_hdr_handler( void *userdata, const char *value );
static void connection_hdr_handler( void *userdata, const char *value );

static void set_hostinfo( struct host_info *info, const char *hostname, int port );
static int lookup_host( struct host_info *info );

static int open_connection( http_req *req );
static int close_connection( http_session *req );

static int set_sockerr( http_req *req, const char *doing, int sockerr );

static char *get_hostport( struct host_info *host);
static void add_fixed_headers( http_req *req );
static int get_request_bodysize( http_req *req );

static int send_request_body( http_req *req );
static void build_request( http_req *req, sbuffer buf );
static int read_message_header( http_req *req, sbuffer buf );
static int read_response_block( http_req *req, struct http_response *resp,
				char *buffer, size_t *buflen );
static int read_response_body( http_req *req, http_status *status );

/* Initializes an HTTP session */
http_session *http_session_init( void ) 
{
    http_session *sess = xmalloc(sizeof(http_session));
    DEBUG( DEBUG_HTTP, "HTTP session begins.\n" );
    strcpy( sess->error, "Unknown error." );
    memset( sess, 0, sizeof(struct http_session_s) );
    sess->version_major = -1;
    sess->version_minor = -1;
    return sess;
}

static void
set_hostinfo( struct host_info *info, const char *hostname, int port )
{
    HTTP_FREE( info->hostport );
    info->hostname = hostname;
    info->port = port;
    info->hostport = get_hostport( info );
    http_auth_init( &info->auth );
}

static int lookup_host( struct host_info *info )
{
    if( host_lookup( info->hostname, &info->addr ) ) {
	return HTTP_LOOKUP;
    } else {
	return HTTP_OK;
    }
}

int http_version_pre_http11( http_session *sess )
{
    return HTTP_VERSION_PRE11(sess);
}

int http_session_server( http_session *sess, const char *hostname, int port )
{
    set_hostinfo( &sess->server, hostname, port );
    if( !sess->have_proxy ) {
	return lookup_host( &sess->server );
    } else {
	return HTTP_OK;
    }
}

int http_session_proxy( http_session *sess, const char *hostname, int port )
{
    sess->have_proxy = 1;
    set_hostinfo( &sess->proxy, hostname, port );
    return lookup_host( &sess->proxy );
}

void http_set_server_auth( http_session *sess, 
			   http_request_auth callback, void *userdata )
{
    sess->server.auth_callback = callback;
    sess->server.auth_userdata = userdata;
}

/* Set callback to handle proxy authentication */
void http_set_proxy_auth( http_session *sess, 
			  http_request_auth callback, void *userdata )
{
    sess->proxy.auth_callback = callback;
    sess->proxy.auth_userdata = userdata;
}

void http_set_error( http_session *sess, const char *errstring )
{
    strncpy( sess->error, errstring, BUFSIZ );
    sess->error[BUFSIZ-1] = '\0';
    STRIP_EOL( sess->error );
}

const char *http_get_error( http_session *sess ) {
    return sess->error;
}

/* Give authentication credentials */
static int give_creds( void *udata, const char *realm,
		       char **username, char **password ) 
{ 
    http_req *req = udata;
    http_session *sess = req->session;
    if( req->status->code == 407 && req->use_proxy && sess->proxy.auth_callback ) {
	return (*sess->proxy.auth_callback)( 
	    sess->proxy.auth_userdata, realm, sess->proxy.hostname,
	    username, password );
    } else if( req->status->code == 401 && sess->server.auth_callback ) {
	return (*sess->server.auth_callback)( 
	    sess->server.auth_userdata, realm, sess->server.hostname,
	    username, password );
    }
    return -1;
}

void http_duplicate_header( void *userdata, const char *value )
{
    char **location = userdata;
    *location = xstrdup( value );
}

void http_handle_numeric_header( void *userdata, const char *value )
{
    int *location = userdata;
    *location = atoi( value );
}

/* The body reader callback */
static void auth_body_reader( void *userdata, const char *block, size_t length )
{
    http_auth_session *sess = userdata;
    http_auth_response_body( sess, block, length );
}

int http_session_finish( http_session *sess ) {
    DEBUG( DEBUG_HTTP, "http_session_finish called.\n" );
    http_auth_finish( &sess->server.auth );
    if( sess->have_proxy ) {
	http_auth_finish( &sess->proxy.auth );
    }
    HTTP_FREE( sess->server.hostport );
    HTTP_FREE( sess->proxy.hostport );
    HTTP_FREE( sess->user_agent );
    if( sess->connected ) close_connection(sess);
    free( sess );
    return HTTP_OK;
}

/* Sends the body down the socket.
 * Returns HTTP_* code */
int send_request_body( http_req *req ) {
    int ret;
    switch( req->body ) {
    case body_stream:
	ret = sock_transfer( fileno(req->body_stream), req->session->socket, 
			req->body_size );
	DEBUG( DEBUG_HTTP, "Sent %d bytes.\n", ret );
	rewind( req->body_stream ); /* since we may have to send it again */
	break;
    case body_buffer:
	DEBUG( DEBUG_HTTP, "Sending body:\n%s\n", req->body_buffer );
	ret = sock_send_string( req->session->socket, req->body_buffer );
	DEBUG( DEBUG_HTTP, "sock_send_string returns: %d\n", ret );
	break;
    default:
	ret = 0;
	break;
    }
    if( ret < 0 ) { 
	/* transfer failed */
	req->forced_close = 1;
	return set_sockerr( req, _("Could not send request body"), ret );
    } else {
	return HTTP_OK;
    }
}

/* Deal with the body size.
 * Returns 0 on success or non-zero on error. */
static int get_request_bodysize( http_req *req ) 
{
    struct stat bodyst;
    /* Do extra stuff if we have a body */
    switch( req->body ) {
    case body_stream:
	/* Get file length */
	if( fstat( fileno(req->body_stream), &bodyst ) < 0 ) {
	    /* Stat failed */
	    DEBUG( DEBUG_HTTP, "Stat failed: %s\n", strerror( errno ) );
	    return -1;
	}
	req->body_size = bodyst.st_size;
	break;
    case body_buffer:
	req->body_size = strlen( req->body_buffer );
	break;
    default:
	/* No body, so no size. */
	break;
    }
    if( req->body != body_none) {
	char tmp[BUFSIZ];
	/* Add the body length header */
	snprintf( tmp, BUFSIZ, "Content-Length: %d" EOL, req->body_size );
	sbuffer_zappend( req->headers, tmp );
    } else {
	sbuffer_zappend( req->headers, "Content-Length: 0" EOL );
    }
    return 0;
}

static char *get_hostport( struct host_info *host) 
{
    size_t len = strlen( host->hostname );
    char *ret = xmalloc( len + 10 );
    strcpy( ret, host->hostname );
    if( host->port != HTTP_PORT ) {
	snprintf( ret + len, 9, ":%d", host->port );
    }
    return ret;
}

const char *http_get_server_hostport( http_session *sess ) {
    return sess->server.hostport;
}


/* Lob the User-Agent, connection and host headers in to the request
 * headers */
static void add_fixed_headers( http_req *req ) {
    if( req->session->user_agent ) {
	sbuffer_concat( req->headers, 
			"User-Agent: ", req->session->user_agent, EOL, NULL );
    }	
    /* Send Connection: Keep-Alive for pre-1.1 origin servers, so we
     * might get a persistent connection. 2068 sec 19.7.1 says we 
     * MUST NOT do this for proxies, though. So we don't. */
    if( HTTP_VERSION_PRE11(req->session) && !req->use_proxy ) {
	sbuffer_zappend( req->headers, "Connection: Keep-Alive, TE" EOL );
	sbuffer_zappend( req->headers, "Keep-Alive: " EOL );
    } else {
	sbuffer_zappend( req->headers, "Connection: TE" EOL );
    }
    /* We send TE: trailers since we understand trailers in the chunked
     * response. */
    sbuffer_concat( req->headers, "TE: trailers" EOL 
		    "Host: ", req->session->server.hostport, EOL, NULL );
}

static int always_accept_response( void *userdata, http_req *req, http_status *st )
{
    return 1;
}				   

int http_accept_2xx( void *userdata, http_req *req, http_status *st )
{
    return (st->class == 2);
}

/* Initializes the request with given method and URI.
 * URI must be abs_path - i.e., NO scheme+hostname. It will BREAK 
 * otherwise. */
http_req *http_request_create( http_session *sess,
			       const char *method, const char *uri ) {
    sbuffer real_uri;
    http_req *req = xmalloc( sizeof(http_req) );

    /* Clear it out */
    memset( req, 0, sizeof( http_req ) );

    DEBUG( DEBUG_HTTP, "Creating request...\n" );

    req->session = sess;
    req->headers = sbuffer_create();
    
    /* Add in the fixed headers */
    add_fixed_headers( req );

    /* Set the standard stuff */
    req->method = method;
    req->method_is_head = (strcmp( req->method, "HEAD" ) == 0);
    req->body = body_none;
    

    /* TODO: Can do clever "should we use the proxy" discrimination
     * here, e.g. by server hostname used.  TODO: when we do that,
     * then we might need to do a name lookup on the server here,
     * since we omit that if we are using a proxy, normally. */
    req->use_proxy = sess->have_proxy;

    /* Add in standard callbacks */

    http_auth_set_creds_cb( &sess->server.auth, give_creds, req );
    http_add_response_body_reader( req, always_accept_response, 
				   auth_body_reader, &req->session->server.auth );
    if( req->use_proxy ) {
	http_auth_set_creds_cb( &sess->proxy.auth, give_creds, req );
	http_add_response_body_reader( req, always_accept_response, 
				       auth_body_reader, &req->session->proxy.auth );
    }
    
    /* Add in handlers for all the standard HTTP headers. */

    http_add_response_header_handler( req, "Content-Length", 
				      http_handle_numeric_header, &req->resp.length );
    http_add_response_header_handler( req, "Transfer-Encoding", 
				      te_hdr_handler, &req->resp );
    http_add_response_header_handler( req, "Connection", 
				      connection_hdr_handler, req );

    req->abs_path = uri_abspath_escape( uri );

    real_uri = sbuffer_create();
    if( req->use_proxy )
	sbuffer_concat( real_uri, "http://", 
			req->session->server.hostport, NULL );
    sbuffer_zappend( real_uri, req->abs_path );
    req->uri = sbuffer_finish(real_uri);

    DEBUG( DEBUG_HTTP, "Request created.\n" );

    return req;
}

void http_set_request_body_buffer( http_req *req, const char *buffer )
{
    req->body = body_buffer;
    req->body_buffer = buffer;
    req->body_stream = NULL;
}

void http_set_request_body_stream( http_req *req, FILE *stream )
{
    req->body = body_stream;
    req->body_stream = stream;
    req->body_buffer = NULL;
}

void http_set_expect100( http_session *sess, int use_expect100 )
{
    if( use_expect100 ) {
	sess->expect100_works = 1;
    } else {
	sess->expect100_works = -1;
    }
}

void http_set_persist( http_session *sess, int persist )
{
    sess->no_persist = !persist;
}

void http_set_useragent( http_session *sess, const char *token )
{
    static const char *fixed = " " NEON_USERAGENT;
    HTTP_FREE( sess->user_agent );
    CONCAT2( sess->user_agent, token, fixed );
}

sbuffer http_get_request_header( http_req *req )
{
    return req->headers;
}

void
http_add_response_header_handler( http_req *req, const char *name, 
				  http_header_handler hdl, void *userdata )
{
    struct header_handler *new = xmalloc( sizeof(struct header_handler) );
    new->name = name;
    new->handler = hdl;
    new->userdata = userdata;
    new->next = req->header_handlers;
    req->header_handlers = new;
}

void
http_add_response_body_reader( http_req *req, http_accept_response acpt,
			       http_block_reader rdr, void *userdata )
{
    struct body_reader *new = xmalloc( sizeof(struct body_reader) );
    new->accept_response = acpt;
    new->handler = rdr;
    new->userdata = userdata;
    new->next = req->body_readers;
    req->body_readers = new;
}

void http_request_destroy( http_req *req ) 
{

    HTTP_FREE( req->uri );
    HTTP_FREE( req->abs_path );

    sbuffer_destroy( req->headers );

    DEBUG( DEBUG_HTTP, "Request ends.\n" );
    free( req );
}


/* Reads a block of the response into buffer, which is of size buflen.
 * Returns number of bytes read, 0 on end-of-response, or HTTP_* on error.
 * TODO?: only make one actual read() call in here... 
 */
static int read_response_block( http_req *req, struct http_response *resp, 
				char *buffer, size_t *buflen ) 
{
    int willread, readlen, socket = req->session->socket;
    if( resp->te==te_chunked ) {
	/* We are doing a chunked transfer-encoding.
	 * It goes:  `SIZE CRLF CHUNK CRLF SIZE CRLF CHUNK CRLF ...'
	 * ended by a `CHUNK CRLF 0 CRLF', a 0-sized chunk.
	 * The slight complication is that we have to cope with
	 * partial reads of chunks.
	 * For this reason, resp.chunk_left contains the number of
	 * bytes left to read in the current chunk.
	 */
	if( resp->chunk_left == 0 ) {
	    long int chunk_len;
	    /* We are at the start of a new chunk. */
	    DEBUG( DEBUG_HTTP, "New chunk.\n" );
	    readlen = sock_readline( socket, buffer, *buflen );
	    if( readlen <= 0 ) {
		return set_sockerr( req, _("Could not read chunk size"), readlen );
	    }
	    DEBUG( DEBUG_HTTP, "[Chunk Size] < %s", buffer );
	    chunk_len = strtol( buffer, NULL, 16 );
	    if( chunk_len == LONG_MIN || chunk_len == LONG_MAX ) {
		DEBUG( DEBUG_HTTP, "Couldn't read chunk size.\n" );
		return -1;
	    }
	    DEBUG( DEBUG_HTTP, "Got chunk size: %ld\n", chunk_len );
	    if( chunk_len == 0 ) {
		/* Zero-size chunk */
		DEBUG( DEBUG_HTTP, "Zero-size chunk.\n" );
		*buflen = 0;
		return HTTP_OK;
	    }
	    resp->chunk_left = chunk_len;
	}
	willread = min( *buflen - 1, resp->chunk_left );
    } else if( resp->length > 0 ) {
	/* Have we finished reading the body? */
	if( resp->left == 0 ) {
	    *buflen = 0;
	    return HTTP_OK;
	}
	willread = min( *buflen - 1, resp->left );
    } else {
	/* Read until socket-close */
	willread = *buflen - 1;
    }
    DEBUG( DEBUG_HTTP, "Reading %d bytes of response body.\n", willread );
    readlen = sock_read( socket, buffer, willread );
    DEBUG( DEBUG_HTTP, "Got %d bytes.\n", readlen );
    if( readlen < 0 ||
	(readlen == 0 && (resp->length > 0 || resp->te==te_chunked)) ) {
	return set_sockerr( req, _("Could not read response body"), readlen );
    }
    buffer[readlen] = '\0';
    *buflen = readlen;
    DEBUG( DEBUG_HTTPBODY, "Read block:\n%s\n", buffer );
    if( resp->te==te_chunked ) {
	resp->chunk_left -= readlen;
	if( resp->chunk_left == 0 ) {
	    char crlfbuf[2];
	    /* If we've read a whole chunk, read a CRLF */
	    readlen = sock_fullread( socket, crlfbuf, 2 );
	    if( readlen < 0 || strncmp( crlfbuf, EOL, 2 ) != 0 ) {
		return set_sockerr( req, _("Error reading chunked response body"),
				    readlen );
	    }
	}
    } else if( resp->length > 0 ) {
	resp->left -= readlen;
    }
    return HTTP_OK;
}

/* Build a request string into the buffer.
 * If we sent the data as we generated it, it's possible that multiple
 * packets could go out on the wire, which is less efficient. */
static void build_request( http_req *req, sbuffer buf ) 
{
    const char *uri;
    char *tmp;

    /* If we are talking to a proxy, we send them the absoluteURI
     * as the Request-URI. If we are talking to a server, we just 
     * send abs_path. */
    if( req->use_proxy )
	uri = req->uri;
    else
	uri = req->abs_path;
    
    sbuffer_clear( buf );

    /* Add in the request and the user-supplied headers */
    sbuffer_concat( buf, req->method, " ", uri, " HTTP/1.1" EOL,
		    sbuffer_data(req->headers), NULL );
    
    /* Add the authorization headers in */
    tmp = http_auth_request_header( &req->session->server.auth );
    if( tmp != NULL ) {
	sbuffer_concat( buf, "Authorization: ", tmp, NULL );
	free( tmp );
    }

    if( req->use_proxy ) {
	tmp = http_auth_request_header( &req->session->proxy.auth );
	if( tmp != NULL ) {
	    sbuffer_concat( buf, "Proxy-Authorization: ", tmp, NULL );
	    free( tmp );
	}
    }
    
    /* Now handle the body. */
    req->use_expect100 = 0;
    if( req->body!=body_none && 
	(req->session->expect100_works > -1) &&
	(req->body_size > HTTP_EXPECT_MINSIZE) && 
	!HTTP_VERSION_PRE11(req->session) ) {
	/* Add Expect: 100-continue. */
	sbuffer_zappend( buf, "Expect: 100-continue" EOL );
	req->use_expect100 = 1;
    }

}

static int set_sockerr( http_req *req, const char *doing, int code )
{
    switch( code ) {
    case 0:
	if( req->use_proxy ) {
	    snprintf( req->session->error, BUFSIZ,
		      _("%s: connection was closed by proxy server."), doing );
	} else {
	    snprintf( req->session->error, BUFSIZ,
		      _("%s: connection was closed by server."), doing );
	}
	return HTTP_ERROR;
    case SOCK_TIMEOUT:
	snprintf( req->session->error, BUFSIZ, 
		  _("%s: connection timed out."), doing );
	return HTTP_TIMEOUT;
    default:
	snprintf( req->session->error, BUFSIZ,
		  "%s: %s", doing, strerror(errno) );
	return HTTP_ERROR;
    }
}

/* TODO: The retry loop may be overkill. It may only be necessary to
 * try sending the request >1 time; making the loop shorter. */
static int send_request( http_req *req, const char *request, 
			 sbuffer buf, http_status *status )
{
    http_session *sess = req->session;
    int ret, try_again;
    
    do {

	try_again = 0;

	/* Open the connection if necessary */
	if( !sess->connected ) {
	    ret = open_connection( req );
	    if( ret != HTTP_OK ) {
		return ret;
	    }
	}

#ifdef DEBUGGING
	if( (DEBUG_HTTPPLAIN&debug_mask) == DEBUG_HTTPPLAIN ) { 
	    /* Display everything mode */
	    DEBUG( DEBUG_HTTP, "Sending request headers:\n%s", request );
	} else {
	    /* Blank out the Authorization paramaters */
	    char *reqdebug = xstrdup(request), *pnt = reqdebug;
	    while( (pnt = strstr( pnt, "Authorization: ")) != NULL ) {
		for( pnt += 15; *pnt != '\r' && *pnt != '\0'; pnt++ ) {
		    *pnt = 'x';
		}
	    }
	    DEBUG( DEBUG_HTTP, "Sending request headers:\n%s", reqdebug );
	    free( reqdebug );
	}
#endif /* DEBUGGING */
	
	DEBUG( DEBUG_HTTP, "Request size: %d\n", strlen(request) );
	/* Send the Request-Line and headers */
	ret = sock_send_string( req->session->socket, request );
	if( ret < 0 ) {
	    return set_sockerr( req, _("Could not send request"), ret );
	}
	
	DEBUG( DEBUG_HTTP, "Request sent\n" );
	
	/* Now, if we are doing a Expect: 100, hang around for a short
	 * amount of time, to see if the server actually cares about the 
	 * Expect and sends us a 100 Continue response if the request
	 * is valid, else an error code if it's not. This saves sending
	 * big files to the server when they will be rejected.
	 */
	
	if( req->use_expect100 ) {
	    DEBUG( DEBUG_HTTP, "Waiting for response...\n" );
	    ret = sock_block( sess->socket, HTTP_EXPECT_TIMEOUT );
	    switch( ret ) {
	    case SOCK_TIMEOUT: 
		/* Timed out - i.e. Expect: ignored. There is a danger
		 * here that the server DOES respect the Expect: header,
		 * but was going SO slowly that it didn't get time to
		 * respond within HTTP_EXPECT_TIMEOUT.
		 * TODO: while sending the body, check to see if the
		 * server has sent anything back - if it HAS, then
		 * stop sending - this is a spec compliance SHOULD */
		DEBUG( DEBUG_HTTP, "Wait timed out.\n" );
		sess->expect100_works = -1; /* don't try that again */
		/* Try sending the request again without using 100-continue */
		try_again = 1;
		break;
	    case SOCK_ERROR: /* error */
		return set_sockerr( req, _("Error waiting for response"), ret );
	    default:
		DEBUG( DEBUG_HTTP, "Wait got data.\n" );
		sess->expect100_works = 1; /* it works - use it again */
		break;
	    }
	} else if( req->body != body_none ) {
	    /* Just chuck the file down the socket */
	    DEBUG( DEBUG_HTTP, "Sending body...\n" );
	    ret = send_request_body( req );
	    if( ret != HTTP_OK ) return ret;
	    DEBUG( DEBUG_HTTP, "Body sent.\n" );
	}
	
	/* Now, we have either:
	 *   - Sent the header and body, or
	 *   - Sent the header incl. Expect: line, and got some response.
	 * In any case, we get the status line of the response.
	 */
	
	/* HTTP/1.1 says that the server MAY emit any number of
	 * interim 100 (Continue) responses prior to the normal
	 * response.  So loop while we get them.  */
	
	do {
	    if( sock_readline( sess->socket, sbuffer_data(buf), BUFSIZ ) <= 0 ) {
		if( try_again ) {
		    return set_sockerr( req, _("Could not read status line"), ret );
		}
		try_again = 1;
		break;
	    }

	    DEBUG( DEBUG_HTTP, "[Status Line] < %s", sbuffer_data(buf) );
	    
	    /* Got the status line - parse it */
	    if( http_parse_statusline( sbuffer_data(buf), status ) ) {
		http_set_error( sess, "Could not parse response status line." );
		return -1;
	    }

	    sess->version_major = status->major_version;
	    sess->version_minor = status->minor_version;
	    snprintf( sess->error, BUFSIZ, "%d %s", 
		      status->code, status->reason_phrase );
	    STRIP_EOL( sess->error );

	    if( status->class == 1 ) {
		DEBUG( DEBUG_HTTP, "Got 1xx-class.\n" );
		/* Skip any headers, we don't need them */
		do {
		    ret = sock_readline( sess->socket, sbuffer_data(buf), BUFSIZ );
		    if( ret <= 0 ) {
			return set_sockerr( 
			    req, _("Error reading response headers"), ret );
		    }
		    DEBUG( DEBUG_HTTP, "[Ignored header] < %s", 
			   sbuffer_data(buf) );
		} while( strcmp( sbuffer_data(buf), EOL ) != 0 );
	
		if( req->use_expect100 && (status->code == 100) ) {
		    /* We are using Expect: 100, and we got a 100-continue 
		     * return code... send the request body */
		    DEBUG( DEBUG_HTTP, "Got continue... sending body now.\n" );
		    if( send_request_body( req ) != HTTP_OK )
			return HTTP_ERROR;
			
		    DEBUG( DEBUG_HTTP, "Body sent.\n" );
		}
	    }
	} while( status->class == 1 );

	if( try_again ) {
	    /* If we're trying again, close the conn first */
	    DEBUG( DEBUG_HTTP, "Retrying request, closing connection first.\n" );
	    close_connection( sess );
	}

    } while( try_again );

    return HTTP_OK;
}

/* Read a message header from sock into buf.
 * Returns an HTTP_* code.
 */
static int read_message_header( http_req *req, sbuffer buf )
{
    char extra[BUFSIZ], *pnt;
    int ret, sock = req->session->socket;
    
    sbuffer_clear(buf);

    ret = sock_readline( sock, sbuffer_data(buf), BUFSIZ );
    if( ret <= 0 )
	return set_sockerr( req, _("Error reading response headers"), ret );
    DEBUG( DEBUG_HTTP, "[Header:%d] < %s", 
	   strlen(sbuffer_data(buf)), sbuffer_data(buf) );
    if( strcmp( sbuffer_data(buf), EOL ) == 0 ) {
	DEBUG( DEBUG_HTTP, "CRLF: End of headers.\n" );
	return HTTP_OK;
    }
    while(sbuffer_size(buf) < HTTP_MAXIMUM_HEADER_LENGTH ) {
	/* Collect any extra lines into buffer */
	ret = sock_recv( sock, extra, 1, MSG_PEEK);
	if( ret <= 0 ) {
	    return set_sockerr( req, _("Error reading response headers"), ret );
	}
	if( extra[0] != ' ' && extra[0] != '\t' ) {
	    /* No more headers */
	    return HTTP_OK;
	}
	ret = sock_readline( sock, extra, BUFSIZ );
	if( ret <= 0 ) {
	    return set_sockerr( req, _("Error reading response headers"), ret);
	}
	DEBUG( DEBUG_HTTP, "[Cont:%d] < %s", strlen(extra), extra);
	/* Append a space to the end of the last header, in
	 * place of the CRLF. */
	pnt = strchr( sbuffer_data(buf), '\r' );
	pnt[0] = ' '; pnt[1] = '\0';
	/* Skip leading whitespace off next line */
	for( pnt = extra; *pnt!='\0' && 
		 ( *pnt == ' ' || *pnt =='\t' ); pnt++ ) /*oneliner*/;
	DEBUG( DEBUG_HTTP, "[Continued] < %s", pnt );
	sbuffer_altered( buf );
	sbuffer_zappend( buf, pnt );
    }
    return HTTP_ERROR;
}

static void normalize_response_length( http_req *req, http_status *status )
{
    /* Response entity-body length calculation, bit icky.
     * Here, we set:
     * length==-1 if we DO NOT know the exact body length
     * length>=0 if we DO know the body length.
     *
     * RFC2616, section 4.4: 
     * NO body is returned if the method is HEAD, or the resp status
     * is 204 or 304
     */
    if( req->method_is_head || status->code==204 || status->code==304 ) {
	req->resp.length = 0;
    } else {
	/* RFC2616, section 4.4: if we have a transfer encoding
	 * and a content-length, then ignore the content-length. */
	if( (req->resp.length>-1) && 
	    (req->resp.te!=te_unknown) ) {
	    req->resp.length = -1;
	}
    }
}

/* Read response headers, using buffer buffer.
 * Returns HTTP_* code. */
static int read_response_headers( http_req *req, sbuffer buf ) 
{
    http_session *sess = req->session;
    struct header_handler *hdl;
    char *name, *value, *pnt, *hdr;
    int ret;
    
    /* Read response headers */
    while(1) {
	ret = read_message_header( req, buf );
	if( ret != HTTP_OK ) return ret;
	
	hdr = sbuffer_data(buf);

	if( strcmp( hdr, EOL ) == 0 ) {
	    return HTTP_OK;
	}

	/* Now parse the header line. This is all a bit noddy. */
	pnt = strchr( hdr, ':' );
	if( pnt == NULL ) {
	    http_set_error( sess, "Malformed header line." );	    
	    return HTTP_ERROR;
	}
	
	/* Null-term name at the : */
	*pnt = '\0';
	name = hdr;
	/* Strip leading whitespace from the value */
	for( value = pnt+1; *value!='\0' && *value==' '; value++ )
	    /* nullop */;
	STRIP_EOL( value );
	DEBUG( DEBUG_HTTP, "Header Name: [%s], Value: [%s]\n",
	       name, value );
	/* Iterate through the header handlers */
	for( hdl = req->header_handlers; hdl != NULL; hdl = hdl->next ) {
	    if( strcasecmp( name, hdl->name ) == 0 ) {
		(*hdl->handler)( hdl->userdata, value );
	    }
	}
    }

    return HTTP_ERROR;
}

/* Read the response message body */
static int read_response_body( http_req *req, http_status *status )
{
    char buffer[BUFSIZ];
    int readlen, ret = HTTP_OK;
    struct body_reader *rdr;
	    
    /* If there is nothing to do... */
    if( req->resp.length == 0 ) {
	/* Do nothing */
	return HTTP_OK;
    }
    
    /* First off, tell all of the response body handlers that they are
     * going to get a body, and let them work out whether they want to 
     * handle it or not */
    for( rdr = req->body_readers; rdr != NULL; rdr=rdr->next ) {
	rdr->use = (*rdr->accept_response)( rdr->userdata, req, status );
    }    
    
    req->resp.left = req->resp.length;
    req->resp.chunk_left = 0;

    /* Now actually read the thing */
    
    do {
	/* Read a block */
	readlen = BUFSIZ;
	ret = read_response_block( req, &req->resp, buffer, &readlen );

	/* TODO: Do we need to call them if readlen==0, or if
	 * readlen == -1, to tell them something has gone wrong? */
	   
	if( ret == HTTP_OK ) {
	    for( rdr = req->body_readers; rdr!=NULL; rdr=rdr->next ) {
		if( rdr->use )
		    (*rdr->handler)( rdr->userdata, buffer, readlen );
	    }
	}
	
    } while( ret == HTTP_OK && readlen > 0 );

    if( ret != HTTP_OK )
	req->forced_close = 1;

    return ret;
}

/* Handler for the "Transfer-Encoding" response header */
static void te_hdr_handler( void *userdata, const char *value ) 
{
    struct http_response *resp = userdata;
    if( strcasecmp( value, "chunked" ) == 0 ) {
	resp->te = te_chunked;
    } else {
	resp->te = te_unknown;
    }
}

/* Handler for the "Connection" response header */
static void connection_hdr_handler( void *userdata, const char *value )
{
    http_req *req = userdata;
    if( strcasecmp( value, "close" ) == 0 ) {
	req->forced_close = 1;
    } else if( strcasecmp( value, "Keep-Alive" ) == 0 ) {
	req->can_persist = 1;
    }
}


/* HTTP/1.x request/response mechanism 
 *
 * Returns an HTTP_* return code. 
 *   
 * The status information is placed in status. The error string is
 * placed in req->session->error
 *
 */
int http_request_dispatch( http_req *req, http_status *status ) 
{
    http_session *sess = req->session;
    sbuffer buf, request;
    int ret, attempt, proxy_attempt, con_attempt, can_retry;
    /* Response header storage */
    char *www_auth, *proxy_auth, *authinfo, *proxy_authinfo;

    /* Initialization... */
    DEBUG( DEBUG_HTTP, "Request started...\n" );
    http_set_error( sess, "Unknown error." );
    ret = HTTP_OK;

    if( get_request_bodysize( req ) )
	return HTTP_ERROR;

    buf = sbuffer_create_sized( BUFSIZ );

    http_add_response_header_handler( req, "WWW-Authenticate",
				      http_duplicate_header, &www_auth );
    http_add_response_header_handler( req, "Proxy-Authenticate",
				      http_duplicate_header, &proxy_auth );
    http_add_response_header_handler( req, "Authentication-Info",
				      http_duplicate_header, &authinfo );
    http_add_response_header_handler( req, "Proxy-Authentication-Info",
				      http_duplicate_header, &proxy_authinfo );
    req->status = status;

    request = sbuffer_create();
    proxy_attempt = con_attempt = attempt = 1;
    www_auth = proxy_auth = authinfo = proxy_authinfo = NULL;
    
    /* Loop sending the request:
     * Retry whilst authentication fails and we supply it. */
    
    do {
	
	can_retry = 0;
	req->can_persist = 0;
	req->forced_close = 0;

	/* Note that we pass the abs_path here... */
	http_auth_new_request( &sess->server.auth, req->method, req->uri,
			       req->body_buffer, req->body_stream );
	if( req->use_proxy ) {
	    /* ...and absoluteURI here. */
	    http_auth_new_request( &sess->proxy.auth, req->method, req->uri,
				   req->body_buffer, req->body_stream );
	}

	build_request( req, request );

#ifdef USE_DAV_LOCKS
	/* TODO: Move this into a hook structure */
	{
	    char *tmp = dav_lock_ifheader( req );
	    if( tmp != NULL ) {
		sbuffer_zappend( request, tmp );
		free( tmp );
		if( HTTP_VERSION_PRE11(sess) ) {
		    /* HTTP/1.0 */
		    sbuffer_zappend( request, "Pragma: no-cache" EOL );
		} else {
		    /* HTTP/1.1 and above */
		    sbuffer_zappend( request, "Cache-Control: no-cache" EOL );
		}
	    }
	}
#endif /* USE_DAV_LOCKS */

	/* Final CRLF */
	sbuffer_zappend( request, EOL );
	
	/* Now send the request, and read the Status-Line */
	ret = send_request( req, sbuffer_data(request), buf, status );
	if( ret != HTTP_OK ) goto dispatch_error;

	req->resp.length = -1;
	req->resp.te = te_unknown;

	/* Read the headers */
	if( read_response_headers( req, buf ) != HTTP_OK ) {
	    ret = HTTP_ERROR;
	    goto dispatch_error;
	}

	normalize_response_length( req, status );

	ret = read_response_body( req, status );
	if( ret != HTTP_OK ) goto dispatch_error;

	/* Read headers in chunked trailers */
	if( req->resp.te == te_chunked ) {
	    ret = read_response_headers( req, buf );
	    if( ret != HTTP_OK ) goto dispatch_error;
	}

	if( proxy_authinfo != NULL && 
	    http_auth_verify_response( &sess->proxy.auth, proxy_authinfo ) ) {
	    DEBUG( DEBUG_HTTP, "Proxy response authentication invalid.\n" );
	    ret = HTTP_SERVERAUTH;
	    http_set_error( sess, _("Proxy server was not authenticated correctly.") );
	} else if( authinfo != NULL &&
		   http_auth_verify_response( &sess->server.auth, authinfo ) ) {
	    DEBUG( DEBUG_HTTP, "Response authenticated as invalid.\n" );
	    ret = HTTP_PROXYAUTH;
	    http_set_error( sess, _("Server was not authenticated correctly.") );
	} else if( status->code == 401 && www_auth != NULL && attempt++ == 1) {
	    if( !http_auth_challenge( &sess->server.auth, www_auth ) ) {
		can_retry = 1;
	    }		
	} else if( status->code == 407 && proxy_auth != NULL && proxy_attempt++ == 1 ) {
	    if( !http_auth_challenge( &sess->proxy.auth, proxy_auth ) ) {
		can_retry = 1;
	    }
	}

	HTTP_FREE( www_auth );
	HTTP_FREE( proxy_auth );
	HTTP_FREE( authinfo );
	HTTP_FREE( proxy_authinfo );
	
	/* Close the connection if one of the following is true:
	 *  - We have a forced close (e.g. "Connection: close" header)
	 *  - We are not using persistent connections for this session
	 *  - this is HTTP/1.0, and they haven't said they can do
	 *    persistent connections 
	 */
	if( req->forced_close || sess->no_persist ||
	    (HTTP_VERSION_PRE11(sess) && !req->can_persist ) ) {
	    close_connection(sess);
	}
    
	/* Retry it if we had an auth challenge */

    } while( can_retry );

    DEBUG( DEBUG_HTTP | DEBUG_FLUSH, 
	   "Request ends, status %d class %dxx, error line:\n%s\n", 
	   status->code, status->class, sess->error );
    DEBUG( DEBUG_HTTPBASIC, "Response: %d %s", status->code, sess->error );

    switch( status->code ) {
    case 401:
	ret = HTTP_AUTH;
	break;
    case 407:
	ret = HTTP_AUTHPROXY;
	break;
    default:
	break;
    }

dispatch_error:
    
    sbuffer_destroy(request);
    
    HTTP_FREE( www_auth );
    HTTP_FREE( proxy_auth );
    HTTP_FREE( authinfo );
    HTTP_FREE( proxy_authinfo );

    return ret;
}

static int open_connection( http_req *req ) {
    http_session *sess = req->session;
    if( req->use_proxy ) {
	DEBUG( DEBUG_SOCKET, "Connecting to proxy at %s:%d...\n", 
	       sess->proxy.hostname, sess->proxy.port );
	sess->socket = sock_connect( sess->proxy.addr, sess->proxy.port );
	if( sess->socket < 0 ) {
	    (void) set_sockerr( req, _("Could not connect to proxy server"), -1 );
	    return HTTP_CONNECT;
	}
    } else {
	DEBUG( DEBUG_SOCKET, "Connecting to server at %s:%d...\n", 
	       sess->server.hostname, sess->server.port );
	sess->socket = sock_connect( sess->server.addr, sess->server.port );
	if( sess->socket < 0 ) {
	    (void) set_sockerr( req, _("Could not connect to server"), -1 );
	    return HTTP_CONNECT;
	}
    }
    DEBUG( DEBUG_SOCKET, "Connected.\n" );
    sess->connected = 1;
    return HTTP_OK;
}

static int close_connection( http_session *sess ) {
    DEBUG( DEBUG_SOCKET, "Closing socket.\n" );
    sock_close( sess->socket );
    sess->connected = 0;
    DEBUG( DEBUG_SOCKET, "Socket closed.\n" );
    return 0;
}

