/* 
   Higher Level Interface to XML Parsers.
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

#ifndef HIP_XML_H
#define HIP_XML_H

/*
  TODO:
  
  There is a whopping great whole in this design. It doesn't 
  correctly handle XML with elements interspersed with CDATA, e.g.
    <foo>this is cdata<bar>still is</bar>still cdata here</foo>
  To handle this, we probably need an extra flag, HIP_XML_MIXED,
  and a cdata callback. Then, on start_element, check if we're in
  mixed mode, and call the cdata callback with everything-up-to-now,
  if we are, then empty the cdata buffer. Note that even with
  this flaw the code is useful, since DAV XML responses don't 
  use this kind of XML.

  To Consider:

  The sitecopy XML parsing code started off as a fairly nasty
  hack. This, then, is the abstraction of that hack.
  
  Need to evaluate whether simply building the entire XML tree
  up is actually better.
  
  My intuition says it's not, but, this is still quite a laborious
  method of handling XML.

*/

#include <config.h>

/* for BUFSIZ */
#include <stdio.h>

/* for sbuffer */
#include "string_utils.h"

#ifdef HAVE_EXPAT
/******** Expat ***********/
# include <xmlparse.h>
typedef XML_Char hip_xml_char;

#else /* not HAVE_EXPAT */
# ifdef HAVE_LIBXML
/******** libxml **********/
#  include <parser.h>
typedef xmlChar hip_xml_char;

# else /* not HAVE_LIBXML */
#  error need an XML parser
# endif /* not HAVE_LIBXML */
#endif /* not HAVE_EXPAT */

/* Generic XML response handling...
   
   Basic principle is that you provide these things:
     1) a list of elements which you wish to handle 
     2) a context checking function which will validate whether a 
     given element is valid in the given parent.
     3) a callback function which is called when a complete element
     has been parsed.
    
   This code then deals with the boring stuff like:
     - Dealing with XML namespaces   
     - Collecting CDATA

   The element list is stored in a 'struct hip_xml_elm' array.
   For each element, you specify the element name, an element id,
   and some flags for that element: whether it can have children,
   and whether it can have cdata.
      
   Lists of element lists can be chained together, so the elements
   from another module don't have to be defined twice.

   Say we have XML docs like:

   <foo attr="yes">
     <bar>norman</bar>
     <bee>yesterday</bee>
     <bar>fishing</bar>
   </foo>

   and we have one module, which handles <foo> elements, and
   another module, which handles bar+bee elements.

   The element lists are:

const static struct hip_xml_elm barbee_elms[] = { 
   {"bar", HIP_ELM_bar, HIP_XML_CDATA },
   {"bee", HIP_ELM_bee, HIP_XML_CDATA },
   { NULL }
};

  Note that foo doesn't take CDATA:

const static struct hip_xml_elm foo_elms[] = { 
   {"foo", HIP_ELM_foo, 0 },
   { NULL }
};

   The context validation functions are:

   int check_barbee_context( parent, child ) {
      return ((child == HIP_ELM_bar ||
	       child == HIP_ELM_bee) && parent == HIP_ELM_foo);
   }

   int check_foo_context( a, parent, child ) {
      return (child == HIP_ELM_foo && parent = HIP_ELM_root);
   }
   
   The list-of-lists are declared:

struct hip_xml_elmlist listfoo = { foo_elms, NULL },
    listbarbee = { barbee_elms, &listfoo };

    Note that listbarbee chains listfoo on the end.
    
*/

/* Reserved element id's */
#define HIP_ELM_unknown -1
#define HIP_ELM_root 0

typedef int hip_xml_elmid;

struct hip_xml_state;
struct hip_xml_elm;
struct hip_xml_handler;

/* An element */
struct hip_xml_elm {
    const char *nspace, *name;
    hip_xml_elmid id;
    unsigned int flags;
};

/* Function to check element context... returns 0 if child is a valid
 * child tag of parent, else non-zero. */
typedef int (*hip_xml_validate_cb)
    ( hip_xml_elmid child, hip_xml_elmid parent );

typedef int (*hip_xml_startelm_cb)
    ( void *userdata, const struct hip_xml_state *s, const hip_xml_char **atts );

/* Called when a complete element is parsed */
typedef int (*hip_xml_endelm_cb)
    ( void *userdata, const struct hip_xml_state *s, const char *cdata );

/* A list of elements */
struct hip_xml_handler {
    const struct hip_xml_elm *elements; /* put it in static memory */
    hip_xml_validate_cb validate_cb; /* validation function */
    hip_xml_startelm_cb startelm_cb; /* on-complete element function */
    hip_xml_endelm_cb endelm_cb; /* on-complete element function */
    void *userdata;
    struct hip_xml_handler *next;
};

struct hip_xml_state {
    /* The element details */
    const struct hip_xml_elm *elm;
    hip_xml_char *name;
    const hip_xml_char *nspace;
    /* Namespaces declared in this element */
    hip_xml_char *default_ns; /* A default namespace */
    struct hip_xml_nspace *nspaces; /* List of other namespace scopes */
    /* Extras */
    struct hip_xml_handler *handler; /* Where the element was declared */
    struct hip_xml_state *parent; /* The parent in the tree */
};

/* We pass around a hip_xml_parser as the userdata in the parsing
 * library.  This maintains the current state of the parse and various
 * other bits and bobs. Within the parse, we store the current branch
 * of the tree, i.e., the current element and all its parents, up to
 * the root, but nothing other than that.  */
struct hip_xml_parser {
    struct hip_xml_state *root; /* the root of the document */
    struct hip_xml_state *current; /* current element in the branch */
    sbuffer buffer; /* the CDATA/collect buffer */
    unsigned int valid:1; /* currently valid? */
    unsigned int want_cdata:1; /* currently collecting CDATA? */
    unsigned int collect; /* currently collecting all children? */
    struct hip_xml_handler *handlers; /* list of handlers */
#ifdef HAVE_EXPAT
    XML_Parser parser;
#else
    xmlParserCtxtPtr parser;
#endif
    char error[BUFSIZ];
};

/* Flags */
/* This element has no children */
#define HIP_XML_CDATA (1<<1)
/* Collect complete contents of this node as cdata */
#define HIP_XML_COLLECT ((1<<2) | HIP_XML_CDATA)
/* Decode UTF-8 data in cdata. */
#define HIP_XML_UTF8DECODE (1<<3)

/* Initialise the parser p, with the list of elements we accept,
 * the context checking function, the element callback, and the userdata
 * for the element callback.
 */
void hip_xml_init( struct hip_xml_parser *p, struct hip_xml_handler *elms );

/* Cleans up the parser's internal structures allocated by hip_xml_init.
 * Returns:
 *   0 if the parse was successful
 *   non-zero if the parse failed.
 */
int hip_xml_finish( struct hip_xml_parser *p );

/* Parse the given block of input of length len. Block does 
 * not need to be NULL-terminated. Note that signed-ness of 
 * the block characters depends upon whether libxml or expat
 * is being used as the underlying parser. */
void hip_xml_parse( struct hip_xml_parser *p, 
		   const hip_xml_char *block, size_t len );

/* As above taking a void * userdata, and a const char * data block,
 * both of which are casted appropriately and passed to
 * hip_xml_parse. */
void hip_xml_parse_v( void *userdata, const char *block, size_t len );

/* Return current parse line for errors */
int hip_xml_currentline( struct hip_xml_parser *p );

/* Returns XML-escaped text, allocated with malloc() */
char *hip_xml_escape( const char *text );

#endif /* HIP_XML_H */
