/* 
   WebDAV 207 multi-status response handling
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

   Id: dav_207.c,v 1.17 2000/05/10 12:02:23 joe Exp  
*/

/* Generic handling for WebDAV 207 Multi-Status responses. */

#include <config.h>

#include "http_utils.h"
#include "hip_xml.h"
#include "dav_207.h"
#include "uri.h"
#include "xalloc.h"

#include "neon_i18n.h"

struct dav_207_parser_s {
    dav_207_start_response start_response;
    dav_207_end_response end_response;
    dav_207_start_propstat start_propstat;
    dav_207_end_propstat end_propstat;
    struct hip_xml_parser parser;
    struct hip_xml_handler handler;
    void *userdata;
    /* current position */
    void *response, *propstat;
    /* caching */
    http_status status;
    char *description, *href, *status_line;
};

const static struct hip_xml_elm elements[] = {
    { "DAV:", "multistatus", DAV_ELM_multistatus, 0 },
    { "DAV:", "response", DAV_ELM_response, 0 },
    { "DAV:", "responsedescription", DAV_ELM_responsedescription, 0 },
    { "DAV:", "href", DAV_ELM_href, HIP_XML_CDATA },
    { "DAV:", "propstat", DAV_ELM_propstat, 0 },
    { "DAV:", "prop", DAV_ELM_prop, 0 },
    { "DAV:", "status", DAV_ELM_status, HIP_XML_CDATA },
#if 0
    { "", "", HIP_ELM_unknown, HIP_XML_COLLECT },
#endif
    { NULL }
};

/* Set the callbacks for the parser */
void dav_207_set_response_handlers( dav_207_parser *p,
				    dav_207_start_response start,
				    dav_207_end_response end )
{
    p->start_response = start;
    p->end_response = end;
}

void dav_207_set_propstat_handlers( dav_207_parser *p,
				    dav_207_start_propstat start,
				    dav_207_end_propstat end )
{
    p->start_propstat = start;
    p->end_propstat = end;
}

/* Parse an input block, as hip_xml_parse */
void dav_207_parse( void *parser, const char *block, size_t len )
{
    dav_207_parser *p = parser;
    hip_xml_parse( &p->parser, block, len );
}

static int 
start_element( void *userdata, const struct hip_xml_state *s, const hip_xml_char **atts ) 
{
    dav_207_parser *p = userdata;
    
    switch( s->elm->id ) {
    case DAV_ELM_response:
	/* Create new response delayed until we get HREF */
	break;
    case DAV_ELM_propstat:
	if( p->start_propstat ) {
	    p->propstat = (*p->start_propstat)( p->userdata, p->response );
	}
	break;
    }
    return 0;
}

static int 
end_element( void *userdata, const struct hip_xml_state *s, const char *cdata )
{
    dav_207_parser *p = userdata;
    
    switch( s->elm->id ) {
    case DAV_ELM_responsedescription:
	p->description = xstrdup(cdata);
	break;
    case DAV_ELM_href:
	/* Now we have the href, begin the response */
	if( p->start_response && cdata != NULL ) {
	    char *uri = uri_unescape( cdata );
	    p->response = (*p->start_response)( p->userdata, uri );
	    free( uri );
	}
	break;
    case DAV_ELM_status:
	switch( s->parent->elm->id ) {
	case DAV_ELM_propstat:
	case DAV_ELM_response:
	    p->status_line = xstrdup(cdata);
	    if( http_parse_statusline( p->status_line, &p->status ) ) {
		snprintf( p->parser.error, BUFSIZ,
			  _("Invalid HTTP status line in status element at line %d of response"), 
			  hip_xml_currentline(&p->parser) );
		HTTP_FREE( p->status_line );
		return -1;
	    }
	    break;
	}
	break;
    case DAV_ELM_propstat:
	if( p->end_propstat ) {
	    (*p->end_propstat)( p->userdata, p->propstat, p->status_line,
				p->status_line?&p->status:NULL, p->description );
	}
	HTTP_FREE( p->status_line );
	break;
    case DAV_ELM_response:
	if( p->end_response ) {
	    (*p->end_response)( p->userdata, p->response, p->status_line,
				p->status_line?&p->status:NULL );
	}
	HTTP_FREE( p->status_line );
	break;
    }
    return 0;
}

/* This should map directly from the DTD... with the addition of
 * ignoring anything we don't understand, being liberal in what we
 * accept. */
static int check_context( hip_xml_elmid parent, hip_xml_elmid child ) 
{
    switch( parent ) {
    case HIP_ELM_root:
	switch( child ) {
	case DAV_ELM_multistatus:
	case DAV_ELM_response: /* not sure why this is here... */
	    return 0;
	default:
	}
	break;
    case DAV_ELM_multistatus:
	/* <!ELEMENT multistatus (response+, responsedescription?) > */
	switch( child ) {
	case DAV_ELM_response:
	case DAV_ELM_responsedescription:
	case HIP_ELM_unknown:
	    return 0;
	default:
	}
	break;
    case DAV_ELM_response:
	/* <!ELEMENT response (href, ((href*, status)|(propstat+)),
	   responsedescription?) > */
	switch( child ) {
	case DAV_ELM_href:
	case DAV_ELM_status:
	case DAV_ELM_propstat:
	case DAV_ELM_responsedescription:
	case HIP_ELM_unknown:
	    return 0;
	default:
	}
	break;
    case DAV_ELM_propstat:
	/* <!ELEMENT propstat (prop, status, responsedescription?) > */
	switch( child ) {
	case DAV_ELM_prop: 
	case DAV_ELM_status:
	case DAV_ELM_responsedescription:
	case HIP_ELM_unknown:
	    return 0;
	default:
	}
	break;
    case DAV_ELM_prop:
	/* <!ELEMENT prop ANY > */
	switch( child ) {
	case HIP_ELM_unknown:
	    return 0;
	default:
	    break;
	}
	break;
    case HIP_ELM_unknown:
	return 0;
    default:
	break;
    }
    return -1;
}

static dav_207_parser *init_parser( void *userdata, struct hip_xml_handler *extra )
{
    dav_207_parser *p = xmalloc( sizeof(dav_207_parser) );
    memset( p, 0, sizeof(dav_207_parser) );
    p->handler.validate_cb = check_context;
    p->handler.startelm_cb = start_element;
    p->handler.endelm_cb = end_element;
    p->handler.userdata = p;
    p->handler.elements = elements;
    p->handler.next = extra;
    p->userdata = userdata;
    hip_xml_init( &p->parser, &p->handler );
    return p;
}   

dav_207_parser *dav_207_init_with_handler( void *userdata, 
					   struct hip_xml_handler *extra )
{
    return init_parser( userdata, extra );
}

dav_207_parser *dav_207_init( void *userdata )
{
    return init_parser( userdata, NULL );
}

int dav_207_finish( dav_207_parser *p ) {
    int ret = hip_xml_finish( &p->parser );
    HTTP_FREE( p->response );
    free( p );
    return ret;
}

const char *dav_207_error( dav_207_parser *p )
{
    return p->parser.error;
}

int dav_accept_207( void *userdata, http_req *req, http_status *status )
{
    return (status->code == 207);
}
