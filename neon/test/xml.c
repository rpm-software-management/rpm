/* 
   neon test suite
   Copyright (C) 2002, Joe Orton <joe@manyfish.co.uk>

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

*/

#include "config.h"

#include <sys/types.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "ne_xml.h"

#include "tests.h"
#include "child.h"
#include "utils.h"

/* validelm, startelm, endelm re-create an XML-like representation of
 * the original document, to verify namespace handling etc works
 * correctly. */
static int validelm(void *userdata, ne_xml_elmid parent, ne_xml_elmid child)
{
    return NE_XML_VALID;
}

static int startelm(void *userdata, const struct ne_xml_elm *s,
		    const char **atts)
{
    ne_buffer *buf = userdata;
    int n;

    ne_buffer_concat(buf, "<", "{", s->nspace, "}", s->name, NULL);
    for (n = 0; atts && atts[n] != NULL; n+=2) {
	ne_buffer_concat(buf, " ", atts[n], "='", atts[n+1], "'", NULL);
    }
    ne_buffer_zappend(buf, ">");
    return 0;
}

static int endelm(void *userdata, const struct ne_xml_elm *s,
		  const char *cdata)
{
    ne_buffer *buf = userdata;
    ne_buffer_concat(buf, cdata?cdata:"NOCDATA", "</{", s->nspace, "}",
		     s->name, ">", NULL);
    return 0;
}

static int parse_match(struct ne_xml_elm *elms,
		       const char *doc, const char *result)
{
    ne_xml_parser *p = ne_xml_create();
    ne_buffer *buf = ne_buffer_create();

    ne_xml_push_handler(p, elms, validelm, startelm, endelm, buf);

    ne_xml_parse(p, doc, strlen(doc));
    
    ONN("parse failed", !ne_xml_valid(p));
    
    ONV(strcmp(result, buf->data),
	("result mismatch: %s not %s", buf->data, result));

    ne_xml_destroy(p);
    ne_buffer_destroy(buf);

    return OK;
}

static int matches(void)
{
#define PFX "<?xml version='1.0'?>\r\n"
#define E(ns, n) "<{" ns "}" n "></{" ns "}" n ">"
    static const struct {
	const char *in, *out;
	int flags;
    } ms[] = {
	{ PFX "<hello/>", "<{}hello></{}hello>", 0 },
	{ PFX "<hello foo='bar'/>",
	  "<{}hello foo='bar'></{}hello>", 0 },
	/*** CDATA handling. ***/
	{ PFX "<hello> world</hello>", "<{}hello> world</{}hello>", 0 },
	{ PFX "<hello> world</hello>", "<{}hello>world</{}hello>",
	  NE_XML_STRIPWS },
	/* test that the XML interface is truly borked. */
	{ PFX "<hello>\r\n<wide>  world</wide></hello>",
	  "<{}hello><{}wide>  world</{}wide></{}hello>", 0 },
	/*** namespace handling. */
#define NSA "xmlns:foo='bar'"
	{ PFX "<foo:widget " NSA "/>", 
	  "<{bar}widget " NSA ">"
	  "</{bar}widget>" },
	/* inherited namespace expansion. */
	{ PFX "<widget " NSA "><foo:norman/></widget>",
	  "<{}widget " NSA ">" E("bar", "norman") "</{}widget>", 0 },
	{ NULL, NULL }
    };
    struct ne_xml_elm elms[] = {
	{ "", "", NE_ELM_unknown, 0 },
	{ NULL }
    };
    int n;

    for (n = 0; ms[n].in != NULL; n++) {
	elms[0].flags = NE_XML_CDATA | ms[n].flags;
	CALL(parse_match(elms, ms[n].in, ms[n].out));
    }

    return OK;
}
	
ne_test tests[] = {
    T(matches),

    T(NULL)
};

