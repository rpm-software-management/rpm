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

*/

#ifndef DAV_PROPS_H
#define DAV_PROPS_H

#include "http_request.h"
#include "dav_207.h"

/* PROPFIND results via three callbacks: */

struct dav_propfind_handler_s;
typedef struct dav_propfind_handler_s dav_propfind_handler;

/* Callback types */
typedef void *(*dav_pf_start_resource)( void *userdata, const char *href );
typedef void (*dav_pf_got_property)( void *userdata, void *resource,
				  const dav_propname *name, const hip_xml_char **atts, 
				  const char *value );
typedef void (*dav_pf_end_resource)( void *userdata, void *resource,
				     const char *status_line, const http_status *status );

/* Find properties with names in set *props on resource at URI 
 * in set *props, with given depth. If props == NULL all properties
 * are returned.
 * Returns:
 *   HTTP_OK on success.
 *   else other HTTP_* code
 */
dav_propfind_handler *
dav_propfind_create( http_session *sess, const char *uri, int depth );

void dav_propfind_register( dav_propfind_handler *handler,
			    dav_pf_start_resource start_res,
			    dav_pf_got_property got_prop,
			    dav_pf_end_resource end_res );

void dav_propfind_set_resource_handlers( dav_propfind_handler *handler,
					 dav_pf_start_resource start_res,
					 dav_pf_end_resource end_res );

void dav_propfind_set_element_handler( dav_propfind_handler *handler,
				     struct hip_xml_handler *elm_handler );

int dav_propfind_allprop( dav_propfind_handler *handler, void *userdata );

int dav_propfind_named( dav_propfind_handler *handler, 
			const dav_propname *names, void *userdata );

/* A PROPPATCH request may include any number of operations. Pass an
 * array of these operations to dav_proppatch, with the last item
 * having the name element being NULL.  If the type is propset, the
 * property of the given name is set to the new value.  If the type is
 * propremove, the property of the given name is deleted, and the
 * value is ignored.  */
typedef struct {
    const dav_propname *name;
    enum {
	dav_propset,
	dav_propremove
    } type;
    const char *value;
} dav_proppatch_operation;

/* FIXME: shouldn't need this. */
void *dav_propfind_get_current_resource( dav_propfind_handler *handler );

int dav_proppatch( http_session *sess, const char *uri,
		   const dav_proppatch_operation *items );

#endif /* DAV_PROPS_H */
