/* 
   nserver, neon Server checker.
   Copyright (C) 2000, Joe Orton <joe@orton.demon.co.uk>
                                                                     
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

   Id: nserver.c,v 1.2 2000/07/16 16:15:44 joe Exp 
*/

#include <sys/types.h>

#include <sys/stat.h>
#include <sys/time.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <neon_config.h>
#include <http_request.h>
#include <http_basic.h>
#include <uri.h>
#include <socket.h>

#include <getopt.h>

#include "basename.h"

static void conn_notify( void *userdata, sock_status status, const char *info )
{
    switch( status ) {
    case sock_namelookup:
    case sock_connecting:
    case sock_connected:
	break;
    case sock_secure_details:
	fprintf( stderr, "nserver: Using a secure connection (%s)\n", info );
	break;
    }
}

int main( int argc, char **argv )
{
    struct uri uri = {0}, defaults = {0};
    http_session *sess;
    http_req *req;
    http_status status;
    char *server = NULL, *pnt;
    int ret;

    neon_debug_init( stderr, 0 );

    sess = http_session_create();

    defaults.path = "/";
    defaults.port = 0;
    defaults.host = NULL;
    defaults.scheme = "http";

    if( argc < 2 ) {
	printf( "nserver: Usage: %s http[s]://server.host.name[:port]\n",
		argv[0] );
	return -1;
    }
    
    pnt = strchr( argv[1], '/' );
    if( pnt == NULL ) {
	uri.host = argv[1];
	pnt = strchr( uri.host, ':' );
	if( pnt == NULL ) {
	    uri.port = 80;
	} else {
	    uri.port = atoi(pnt+1);
	    *pnt = '\0';
	}
	uri.path = "/";
	uri.scheme = "http";
    } else {
	if( uri_parse( argv[1], &uri, &defaults ) || !uri.host ) {
	    printf( "nserver: Could not parse URL `%s'\n", argv[1] );
	    return -1;
	}
	if( strcasecmp( uri.scheme, "https" ) == 0 ) {
	    if( uri.port == 0 ) {
		uri.port = 443;
	    }
	    if( http_set_secure( sess, 1 ) ) {
		fprintf( stderr, "nserver: SSL not supported.\n" );
		exit( -1 );
	    }
	}
    }

    sock_init();
    sock_register_notify( conn_notify, NULL );

    if( uri.port == 0 ) {
	uri.port = 80;
    }
    
    printf( "nserver: Retrieving server string for server at %s (port %d):\n",
	    uri.host, uri.port );
    
    req = http_request_create(sess, "HEAD", uri.path );
    if( http_session_server( sess, uri.host, uri.port ) != HTTP_OK ) {
	printf( "nserver: Hostname `%s' not found.\n", uri.host );
	return -1;
    }

    /* Use a standard strdup-er handler */
    http_add_response_header_handler( req, "Server", 
				      http_duplicate_header, &server );
    
    switch( http_request_dispatch( req, &status ) ) {
    case HTTP_OK:
	if( server == NULL ) {
	    printf( "nserver: No server string was given.\n" );
	    ret = 1;
	} else {
	    printf( "Server string: %s\n", server );
	    ret = 0;
	}
	break;
    default:
	printf( "nserver: Failed: %s\n", http_get_error(sess) );
	ret = -1;
	break;
    }
    
    http_request_destroy( req );
    http_session_destroy( sess );

    return ret;
}
