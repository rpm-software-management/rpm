/* 
   WebDAV Class 1 namespace operations and 207 error handling
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

   Id: dav_basic.c,v 1.12 2000/05/09 18:28:00 joe Exp 
*/

#include "config.h"

#include "http_request.h"

#include "dav_basic.h"
#include "uri.h" /* for uri_has_trailing_slash */
#include "http_basic.h" /* for http_content_type */
#include "string_utils.h" /* for sbuffer */
#include "dav_207.h"

/* Handling of 207 errors: we keep a string buffer, and append
 * messages to it as they come down.
 *
 * Note, 424 means it would have worked but something else went wrong.
 * We will have had the error for "something else", so we display
 * that, and skip 424 errors.  TODO: should display 'description' too,
 * but find out whether servers actually use this first. */

/* This is passed as userdata to the 207 code. */
struct context {
    char *href;
    sbuffer buf;
    unsigned int is_error;
};

static void *start_response( void *userdata, const char *href )
{
    struct context *ctx = userdata;
    HTTP_FREE( ctx->href );
    ctx->href = xstrdup(href);
    return NULL;
}

static void handle_error( struct context *ctx,
			 const char *status_line, const http_status *status )
{
    if( status && status->class != 2 ) {
	ctx->is_error = 1;
	if( status->code != 424 ) {
	    sbuffer_concat( ctx->buf, ctx->href, ": ", status_line, "\n", NULL );
	}
    }
}

static void end_response( void *userdata, void *response, const char *status_line,
			  const http_status *status )
{
    struct context *ctx = userdata;
    handle_error( ctx, status_line, status );
}

static void 
end_propstat( void *userdata, void *propstat, const char *status_line,
	      const http_status *status, const char *description )
{
    struct context *ctx = userdata;
    handle_error( ctx, status_line, status );
}

void dav_add_depth_header( http_req *req, int depth )
{
    sbuffer hdrs = http_get_request_header( req );
    switch( depth ) {
    case DAV_DEPTH_ZERO:
	sbuffer_zappend( hdrs, "Depth: 0" EOL );
	break;
    case DAV_DEPTH_ONE:
	sbuffer_zappend( hdrs, "Depth: 1" EOL );
	break;
    default:
	sbuffer_zappend( hdrs, "Depth: infinity" EOL );
	break;
    }
}

/* Dispatch a DAV request and handle a 207 error response appropriately */
int dav_simple_request( http_session *sess, http_req *req )
{
    int ret, invalid;
    http_status status;
    http_content_type ctype = {0};
    struct context ctx = {0};
    dav_207_parser *p;

    p = dav_207_init( &ctx );
    ctx.buf = sbuffer_create();

    dav_207_set_response_handlers( p, start_response, end_response );
    dav_207_set_propstat_handlers( p, NULL, end_propstat );
    
    http_add_response_body_reader( req, dav_accept_207, dav_207_parse, p );
    http_add_response_header_handler( req, "Content-Type", 
				      http_content_type_handler, &ctype );

    ret = http_request_dispatch( req, &status );

    invalid = dav_207_finish( p );

    if( ret == HTTP_OK ) {
	if( status.code == 207 ) {
	    if( invalid ) { 
		http_set_error( sess, dav_207_error(p) );
		ret = HTTP_ERROR;
	    } else if( ctx.is_error ) {
		http_set_error( sess, sbuffer_data(ctx.buf) );
		ret = HTTP_ERROR;
	    }
	} else if( status.class != 2 ) {
	    ret = HTTP_ERROR;
	}
    }

    sbuffer_destroy(ctx.buf);
    HTTP_FREE(ctx.href);

    http_request_destroy( req );

    return ret;
}
    
static int copy_or_move( http_session *sess, int is_move, int no_overwrite,
			 const char *src, const char *dest ) {
    http_req *req = http_request_create( sess, is_move?"MOVE":"COPY", src );
    const char *str;
    sbuffer hdrs;

#ifdef USE_DAV_LOCKS
    /* Copy needs to read/write the source, and write to the destination */
    dav_lock_using_resource( req, src, 
			     is_move?dav_lockusage_write:dav_lockusage_read, 
			     DAV_DEPTH_INFINITE );
    dav_lock_using_resource( req, dest, dav_lockusage_write, 
			     DAV_DEPTH_INFINITE );
    /* And we need to be able to add members to the destination's parent */
    dav_lock_using_parent( req, dest );
#endif

    hdrs = http_get_request_header( req );
    str = http_get_server_hostport( sess );
    sbuffer_concat( hdrs, "Destination: http://", str, dest, EOL, NULL );

    if( no_overwrite )
	sbuffer_zappend( hdrs, "Overwrite: F" EOL );

#if 0
    /* FIXME */
    if( ! http_webdav_server ) {
	/* For non-WebDAV servers */
	strcat( req.headers, "New-URI: " );
	strcat( req.headers, to );
	strcat( req.headers, EOL );
    }
#endif

    return dav_simple_request( sess, req );
}

int dav_copy( http_session *sess, const char *src, const char *dest ) {
    return copy_or_move( sess, 0, 1, src, dest );
}

int dav_move( http_session *sess, const char *src, const char *dest ) {
    return copy_or_move( sess, 1, 1, src, dest );
}

/* Deletes the specified resource. (and in only two lines of code!) */
int dav_delete( http_session *sess, const char *uri ) {
    http_req *req = http_request_create( sess, "DELETE", uri );

#ifdef USE_DAV_LOCKS
    dav_lock_using_resource( req, uri, dav_lockusage_write, DAV_DEPTH_INFINITE );
    dav_lock_using_parent( req, uri );
#endif
    
    /* joe: I asked on the DAV WG list about whether we might get a
     * 207 error back from a DELETE... conclusion, you shouldn't if
     * you don't send the Depth header, since we might be an HTTP/1.1
     * client and a 2xx response indicates success to them.  But
     * it's all a bit unclear. In any case, DAV servers today do
     * return 207 to DELETE even if we don't send the Depth header.
     * So we handle 207 errors appropriately. */

    return dav_simple_request( sess, req );
}

int dav_mkcol( http_session *sess, const char *uri ) 
{
    http_req *req;
    char *real_uri;
    int ret;

    if( uri_has_trailing_slash( uri ) ) {
	real_uri = xstrdup( uri );
    } else {
	CONCAT2( real_uri, uri, "/" );
    }

    req = http_request_create( sess, "MKCOL", real_uri );

#ifdef USE_DAV_LOCKS
    dav_lock_using_resource( req, real_uri, dav_lockusage_write, 0 );
    dav_lock_using_parent( req, real_uri );
#endif
    
    ret = dav_simple_request( sess, req );

    free( real_uri );

    return ret;
}
