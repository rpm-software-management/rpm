/* 
   WebDAV 207 multi-status response handling
   Copyright (C) 1999-2001, Joe Orton <joe@light.plus.com>

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

#include "ne_xml.h"
#include "ne_request.h" /* for http_req */

BEGIN_NEON_DECLS

#define NE_ELM_207_first (NE_ELM_UNUSED)

#define NE_ELM_multistatus (NE_ELM_207_first)
#define NE_ELM_response (NE_ELM_207_first + 1)
#define NE_ELM_responsedescription (NE_ELM_207_first + 2)
#define NE_ELM_href (NE_ELM_207_first + 3)
#define NE_ELM_propstat (NE_ELM_207_first + 4)
#define NE_ELM_prop (NE_ELM_207_first + 5)
#define NE_ELM_status (NE_ELM_207_first + 6)

#define NE_ELM_207_UNUSED (NE_ELM_UNUSED + 100)

struct ne_207_parser_s;
typedef struct ne_207_parser_s ne_207_parser;

/* The name of a WebDAV property. */
typedef struct {
    const char *nspace, *name;
} ne_propname;

/* The handler structure: you provide a set of callbacks.
 * They are called in the order they are listed... start/end_prop
 * multiple times before end_prop, start/end_propstat multiple times
 * before an end_response, start/end_response multiple times.
 */

/* TODO: do we need to pass userdata to ALL of these? We could get away with
 * only passing the userdata to the start_'s and relying on the caller
 * to send it through as the _start return value if they need it. */

typedef void *(*ne_207_start_response)(void *userdata, const char *href);
typedef void (*ne_207_end_response)(
    void *userdata, void *response, const char *status_line,
    const ne_status *status, const char *description);

typedef void *(*ne_207_start_propstat)(void *userdata, void *response);
typedef void (*ne_207_end_propstat)(
    void *userdata, void *propstat, const char *status_line, 
    const ne_status *status, const char *description);

/* Create a 207 parser */

ne_207_parser *ne_207_create(ne_xml_parser *parser, void *userdata);

/* Set the callbacks for the parser */

void ne_207_set_response_handlers(
    ne_207_parser *p, ne_207_start_response start, ne_207_end_response end);

void ne_207_set_propstat_handlers(
    ne_207_parser *p, ne_207_start_propstat start, ne_207_end_propstat end);

void ne_207_destroy(ne_207_parser *p);

/* An acceptance function which only accepts 207 responses */
int ne_accept_207(void *userdata, ne_request *req, ne_status *status);

void *ne_207_get_current_propstat(ne_207_parser *p);
void *ne_207_get_current_response(ne_207_parser *p);

/* Call this as the LAST thing before beginning parsing, to install a
 * catch-all handler which means all unknown XML returned in the 207
 * response is ignored gracefully.  */
void ne_207_ignore_unknown(ne_207_parser *p);

/* Dispatch a DAV request and handle a 207 error response appropriately */
int ne_simple_request(ne_session *sess, ne_request *req);

END_NEON_DECLS

#endif /* DAV207_H */
