/* 
   WebDAV Properties manipulation
   Copyright (C) 2000, Joe Orton <joe@orton.demon.co.uk>

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

   Id: dav_props.c,v 1.12 2000/05/09 22:36:55 joe Exp 
*/

#include "config.h"

#include "xalloc.h"
#include "dav_props.h"
#include "dav_basic.h"
#include "hip_xml.h"

struct dav_propfind_handler_s {
    dav_pf_start_resource start_resource;
    dav_pf_end_resource end_resource;
    struct hip_xml_handler *handler;
    http_session *sess;
    const char *uri;
    int depth;
    char *href;
    http_status status;
    void *userdata, *resource;
};

void *dav_propfind_get_current_resource( dav_propfind_handler *handler )
{ 
    return handler->resource;
}    

static void *start_response( void *userdata, const char *href )
{
    dav_propfind_handler *ctx = userdata;
    ctx->resource = NULL;
    if( ctx->start_resource ) {
	ctx->resource = (*ctx->start_resource)( ctx->userdata, href );
    }
    return ctx->resource;
}

static void end_response( void *userdata, void *response, 
			  const char *status_line, const http_status *status )
{
    dav_propfind_handler *ctx = userdata;
    if( ctx->end_resource ) {
	(*ctx->end_resource)( ctx->userdata, response, status_line, status );
    }
}

dav_propfind_handler *
dav_propfind_create( http_session *sess, const char *uri, int depth )
{
    dav_propfind_handler *ret = xmalloc(sizeof(dav_propfind_handler));
    memset( ret, 0, sizeof(dav_propfind_handler));
    ret->uri = uri;
    ret->depth = depth;
    ret->sess = sess;
    return ret;
}

void dav_propfind_set_resource_handlers( dav_propfind_handler *handler,
					 dav_pf_start_resource start_res,
					 dav_pf_end_resource end_res )
{
    handler->start_resource = start_res;
    handler->end_resource = end_res;
}

void dav_propfind_set_element_handler( dav_propfind_handler *handler,
				       struct hip_xml_handler *elm_handler )
{
    handler->handler = elm_handler;
}

static int propfind( dav_propfind_handler *handler, const dav_propname *names,
		     void *userdata )
{
    int ret, is_allprop = (names==NULL);
    http_req *req = http_request_create( handler->sess, "PROPFIND", handler->uri );
    http_status status;
    dav_207_parser *parser;
    sbuffer body, hdrs;
    
    body = sbuffer_create();

    sbuffer_concat( body, 
		    "<?xml version=\"1.0\" encoding=\"utf-8\"?>" EOL 
		    "<propfind xmlns=\"DAV:\">", NULL );
    
    if( !is_allprop ) {
	int n;
	sbuffer_zappend( body, "<prop>" EOL );
	for( n = 0; names[n].name != NULL; n++ ) {
	    sbuffer_concat( body, "<", names[n].name, " xmlns=\"", 
			    names[n].nspace, "\"/>" EOL, NULL );
	}
	sbuffer_zappend( body, "</prop></propfind>" EOL );
    } else {
	sbuffer_zappend( body, "</allprop></propfind>" EOL );
    }

    parser = dav_207_init_with_handler( handler, handler->handler );
    dav_207_set_response_handlers( parser, start_response, end_response );
    handler->userdata = userdata;

    http_set_request_body_buffer( req, sbuffer_data(body) );

    hdrs = http_get_request_header( req );
    sbuffer_zappend( hdrs, "Content-Type: text/xml" EOL ); /* TODO: UTF-8? */
    dav_add_depth_header( req, handler->depth );
    
    http_add_response_body_reader( req, dav_accept_207, dav_207_parse, parser );

    ret = http_request_dispatch( req, &status );

    http_request_destroy( req );

    return ret;
}

int dav_propfind_allprop( dav_propfind_handler *handler, void *userdata )
{
    return propfind( handler, NULL, userdata );
}

int dav_propfind_named( dav_propfind_handler *handler, 
			const dav_propname *names, void *userdata )
{
    return propfind( handler, names, userdata );
}


/* The easy one... PROPPATCH */
int dav_proppatch( http_session *sess, const char *uri, 
		   const dav_proppatch_operation *items )
{
    http_req *req = http_request_create( sess, "PROPPATCH", uri );
    sbuffer body = sbuffer_create(), hdrs;
    int n, ret;
    
    /* Create the request body */
    sbuffer_zappend( body, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>" EOL
		     "<propertyupdate xmlns=\"DAV:\">" );

    for( n = 0; items[n].name != NULL; n++ ) {
	switch( items[n].type ) {
	case dav_propset:
	    /* <set><prop><prop-name>value</prop-name></prop></set> */
	    sbuffer_concat( body, "<set><prop>"
			    "<", items[n].name->name, " xmlns=\"",
			    items[n].name->nspace, "\">", items[n].value,
			    "</", items[n].name->name, "></prop></set>" EOL, NULL );
	    break;

	case dav_propremove:
	    /* <remove><prop><prop-name/></prop></remove> */
	    sbuffer_concat( body, "<remove><prop><", items[n].name->name, " xmlns=\"",
			    items[n].name->nspace, "\"/></prop></remove>" EOL, NULL );
	    break;
	}
    }	

    sbuffer_zappend( body, "</propertyupdate>" EOL );
    
    http_set_request_body_buffer( req, sbuffer_data(body) );

    hdrs = http_get_request_header( req );
    sbuffer_zappend( hdrs, "Content-Type: text/xml" EOL ); /* TODO: UTF-8? */
    
    ret = dav_simple_request( sess, req );
    
    sbuffer_destroy( body );
    
    return ret;
}

