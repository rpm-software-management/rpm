/* 
   nget, neon HTTP GET tester
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

   Id: nget.c,v 1.3 2000/05/10 18:16:56 joe Exp 
*/

#include <sys/types.h>

#include <sys/stat.h>
#include <sys/time.h>

#include <stdio.h>
#include <errno.h>

#include "http_request.h"
#include "http_basic.h"
#include "uri.h"
#include "socket.h"

#include "basename.h"

int in_progress = 0, use_stdout = 0;

static void pretty_progress_bar( void *ud, size_t progress, size_t total );

static const char *use_filename( struct uri *uri )
{
    const char *fname;

    fname = base_name( uri->path );
    if( strcmp( fname, "/" ) == 0 || strlen( fname ) == 0 ) {
	fname = "index.html";
    }
    
    return fname;
}

int main( int argc, char **argv )
{
    http_session *sess;
    struct uri uri = {0}, defaults = {0};
    struct stat st;
    int ret;
    const char *fname;
    FILE *f;

    if( argc < 2 || argc > 3 ) {
	printf( "nget: Usage: \n"
		"  `nget url': download url using appropriate filename\n"
		"  `nget url filename': download url to given filename\n"
		"  `nget url -': download url and display on stdout\n" );
	return -1;
    }

    sock_register_progress( pretty_progress_bar, NULL );

    defaults.port = 80;
    
    if( uri_parse( argv[1], &uri, &defaults ) || 
	!uri.path || !uri.host || uri.port == -1 ) {
	printf( "nget: Invalid URL.\n" );
	return -1;
    }
    
    if( argc == 3 ) {
	fname = argv[2];
    } else {
	fname = use_filename( &uri );
    }
    if( strcmp( fname, "-" ) == 0 ) {
	f = stdout;
	use_stdout = 1;
    } else if( stat( fname, &st ) == 0 ) {
	printf( "nget: File `%s' already exists.\n", fname );
	return -1;
    } else {
	f = fopen( fname, "w" );
	if( f == NULL ) {
	    printf( "nget: Could not open %s: %s\n", fname, strerror(errno) );
	    return -1;
	}
    }
    
    sess = http_session_init();
    
    http_session_server( sess, uri.host, uri.port );
    
    if( !use_stdout ) {
	printf( "nget: Downloading %s to %s\n", argv[1], fname );
    }
    ret = http_get( sess, uri.path, f );

    if( in_progress ) {
	printf( "\n" );
    }

    if( ret == HTTP_OK ) {
	if( !use_stdout ) printf( "nget: Download complete.\n" );
    } else {
	fprintf( stderr, 
		 "nget: Download error: %s\n", http_get_error(sess));
    }

    if( !use_stdout ) 
	fclose( f );

    return ret;
}

/* Smooth progress bar from cadaver.
 * Doesn't update the bar more than once every 100ms, since this 
 * might give flicker, and would be bad if we are displaying on
 * a slow link anyway.
 */
static void pretty_progress_bar( void *ud, size_t progress, size_t total )
{
    int len, n;
    double pc;
    static struct timeval last_call = {0};
    struct timeval this_call;
    if( use_stdout ) return;
    in_progress = 1;
    if( total == -1 ) {
	printf( "\rProgress: %d bytes", progress );
	return;
    }
    if( progress < total && gettimeofday( &this_call, NULL ) == 0 ) {
	struct timeval diff;
	timersub( &this_call, &last_call, &diff );
	if( diff.tv_sec == 0 && diff.tv_usec < 100000 ) {
	    return;
	}
	last_call = this_call;
    }
    if( progress == 0 || total == 0 ) {
	pc = 0;
    } else {
	pc = (double)progress / total;
    }
    len = pc * 30;
    printf( "\rProgress: [" );
    for( n = 0; n<30; n++ ) {
	putchar( (n<len-1)?'=':
		 (n==(len-1)?'>':' ') );
    }
    printf( "] %5.1f%% of %d bytes", pc*100, total );
    fflush( stdout );
}

