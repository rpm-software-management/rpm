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

*/

#ifndef DAV207_H
#define DAV207_H

#include "hip_xml.h"
#include "http_utils.h" /* for struct http_status */
#include "http_request.h" /* for http_req */

#define DAV_ELM_multistatus (101)
#define DAV_ELM_response (102)
#define DAV_ELM_responsedescription (103)
#define DAV_ELM_href (104)
#define DAV_ELM_propstat (105)
#define DAV_ELM_prop (106)
#define DAV_ELM_status (107)

struct dav_207_parser_s;
typedef struct dav_207_parser_s dav_207_parser;

/* The name of a WebDAV property. */
typedef struct {
    const char *nspace, *name;
} dav_propname;

/* The handler structure: you provide a set of callbacks.
 * They are called in the order they are listed... start/end_prop
 * multiple times before end_prop, start/end_propstat multiple times
 * before an end_response, start/end_response multiple times.
 */

/* TODO: do we need to pass userdata to ALL of these? We could get away with
 * only passing the userdata to the start_'s and relying on the caller
 * to send it through as the _start return value if they need it. */

typedef void *(*dav_207_start_response)( void *userdata, const char *href );
typedef void (*dav_207_end_response)( 
    void *userdata, void *response, const char *status_line, const http_status *status );

typedef void *(*dav_207_start_propstat)( void *userdata, void *response );
typedef void (*dav_207_end_propstat)( 
    void *userdata, void *propstat, const char *status_line, const http_status *status, 
    const char *description );

/* Create a 207 parser */

dav_207_parser *dav_207_init( void *userdata );

dav_207_parser *dav_207_init_with_handler( void *userdata, 
					   struct hip_xml_handler *extra );

/* Set the callbacks for the parser */

void dav_207_set_response_handlers( 
    dav_207_parser *p, dav_207_start_response start, dav_207_end_response end );

void dav_207_set_propstat_handlers( 
    dav_207_parser *p, dav_207_start_propstat start, dav_207_end_propstat end );

/* Parse an input block, as hip_xml_parse */
void dav_207_parse( void *parser, const char *block, size_t len );

/* Finish parsing... returns 0 if the parse was successful, else
 * non-zero. */
int dav_207_finish( dav_207_parser *p );

const char *dav_207_error( dav_207_parser *p );

/* An acceptance function which only accepts 207 responses */
int dav_accept_207( void *userdata, http_req *req, http_status *status );

#endif /* DAV207_H */
