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

   Id: hip_xml.c,v 1.14 2000/05/09 22:33:54 joe Exp 
*/

#include <config.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "neon_i18n.h"

#include "xalloc.h"
#include "http_utils.h"
#include "string_utils.h"
#include "hip_xml.h"

static const char *friendly_name( const struct hip_xml_elm *elm )
{
    switch( elm->id ) {
    case HIP_ELM_root:
	return _("document root");
    case HIP_ELM_unknown:
	return _("unknown element");
    default:
	if( elm->name ) {
	    return elm->name;
	} else {
	    return _("unspecified");
	}
    }
}
  
/* TODO: 
 * could move 'valid' into state, maybe allow optional
 * continuation past an invalid branch.
 */

const static struct hip_xml_elm root_element = 
{ "@<root>@", HIP_ELM_root, 0 };

/* The callback handlers */
static void start_element( void *userdata, const hip_xml_char *name, const hip_xml_char **atts );
static void end_element( void *userdata, const hip_xml_char *name );
static void char_data( void *userdata, const hip_xml_char *cdata, int len );

#ifdef HAVE_EXPAT
/* libxml doesn't seem to pass stuff back in UTF-8.
 * maybe libxml-2.0 will do. */
#define HIP_XML_DECODE_UTF8
#endif

#ifdef HIP_XML_DECODE_UTF8

/* UTF-8 decoding */

/* Single byte range 0x00 -> 0x7F */
#define SINGLEBYTE_UTF8( ch ) ( ((unsigned char) (ch)) < 0x80 )

/* Decode a double byte UTF8 string.
 * Returns 0 on success or non-zero on error. */
static inline int decode_utf8_double( char *dest, const char *src );

#endif

/* Linked list of namespace scopes */
struct hip_xml_nspace {
    hip_xml_char *name;
    hip_xml_char *uri;
    struct hip_xml_nspace *next;
};

/* And an auxiliary */
static int parse_element( struct hip_xml_parser *p, struct hip_xml_state *state,
			  const hip_xml_char *name, const hip_xml_char **atts );


#ifdef HAVE_LIBXML

/* Could be const as far as we care, but libxml doesn't want that */
static xmlSAXHandler sax_handler = {
    NULL, /* internalSubset */
    NULL, /* isStandalone */
    NULL, /* hasInternalSubset */
    NULL, /* hasExternalSubset */
    NULL, /* resolveEntity */
    NULL, /* getEntity */
    NULL, /* entityDecl */
    NULL, /* notationDecl */
    NULL, /* attributeDecl */
    NULL, /* elementDecl */
    NULL, /* unparsedEntityDecl */
    NULL, /* setDocumentLocator */
    NULL, /* startDocument */
    NULL, /* endDocument */
    start_element, /* startElement */
    end_element, /* endElement */
    NULL, /* reference */
    char_data, /* characters */
    NULL, /* ignorableWhitespace */
    NULL, /* processingInstruction */
    NULL, /* comment */
    NULL, /* xmlParserWarning */
    NULL, /* xmlParserError */
    NULL, /* xmlParserError */
    NULL, /* getParameterEntity */
};

#endif /* HAVE_LIBXML */

#ifdef HIP_XML_DECODE_UTF8

static inline int 
decode_utf8_double( char *dest, const char *src ) 
{
    /* From utf-8 man page; two-byte encoding is:
     *    0x00000080 - 0x000007FF:
     *       110xxxxx 10xxxxxx
     * If more than 8-bits of those x's are set, we fail.
     * So, we check that the first 6 bits of the first byte are:
     *       110000.
     * Then decode like:
     *       110000xx 10yyyyyy  -> xxyyyyyy
     * Do this with a mask and a compare:
     *       zzzzzzzz
     *     & 11111100  <=> 0xFC
     *    == 11000000  <=> 0XC0    
     * 
     * joe: A real C hacker would probably do some funky bit
     * inversion, and turn this into an is-not-zero test, 
     * but I'm a fake, so...
     */
    if( (src[0] & 0xFC) == 0xC0 ) {
	dest[0] = ((src[0] & 0x03) << 6) | (src[1] & 0x3F);
	return 0;
    } else {
	return -1;
    }
}

#endif

int hip_xml_currentline( struct hip_xml_parser *p ) {
#ifdef HAVE_EXPAT
    return XML_GetCurrentLineNumber(p->parser);
#else
    return p->parser->input->line;
#endif
}

static int find_handler( struct hip_xml_handler *list, struct hip_xml_state *state ) {
    struct hip_xml_handler *cur, *unk_handler = NULL;
    const struct hip_xml_elm *unk_elm = NULL;
    int n;
    for( cur = list; cur != NULL; cur = cur->next ) {
	for( n = 0; cur->elements[n].nspace != NULL; n++ ) {
	    if( strcasecmp( cur->elements[n].name, state->name ) == 0 && 
		strcasecmp( cur->elements[n].nspace, state->nspace ) == 0 ) {
		state->handler = cur;
		state->elm = &cur->elements[n];
		return 0;
	    }
	    if( !unk_elm && cur->elements[n].id == HIP_ELM_unknown ) {
		unk_elm = &cur->elements[n];
		unk_handler = cur;
	    }
	}
    }
    if( !cur && unk_elm ) {
	/* Give them the unknown handler */
	state->handler = unk_handler;
	state->elm = unk_elm;
	return 0;
    } else {
	return -1;
    }
}

char *hip_xml_escape( const char *text )
{
#if 0
    sbuffer buf = sbuffer_create();
    const char *pnt;
    if( !buf ) return NULL;
    /* FIXME: implement */
#endif
    return NULL;
}

/* Called with the start of a new element. */
static void 
start_element( void *userdata, const hip_xml_char *name, const hip_xml_char **atts ) 
{
    struct hip_xml_parser *p = (struct hip_xml_parser *)userdata;
    struct hip_xml_state *s;

    if( !p->valid ) {
	/* We've stopped parsing */
	DEBUG( DEBUG_XML, "Parse died. Ignoring start of element: %s\n", name );
	return;
    }
    if( p->collect ) {
	/* In Collect Mode. */
	sbuffer_concat( p->buffer, "<", name, NULL );
	if( atts != NULL ) {
	    int n;
	    for( n = 0; atts[n] != NULL; n+=2 ) {
		sbuffer_concat( p->buffer, " ", atts[n], "=", atts[n+1],
				NULL );
	    }
	}
	sbuffer_zappend( p->buffer, ">" );
	/* One deeper */
	p->collect++;
	return;
    }
    /* Set the new state */
    s = xmalloc( sizeof(struct hip_xml_state) );
    memset( s, 0, sizeof(struct hip_xml_state) );
    s->parent = p->current;
    p->current = s;
    /* We need to handle namespaces ourselves */
    if( parse_element( p, s, name, atts ) ) {
	/* it bombed. */
	p->valid = 0;
	return;
    }
    /* Map the element name to an id */
    DEBUG( DEBUG_XMLPARSE, "Mapping element name %s@@%s... ", s->nspace, s->name );
    if( find_handler( p->handlers, s ) ) {
	DEBUG( DEBUG_XMLPARSE, "Unexpected element\n" );
	snprintf( p->error, BUFSIZ, "Unknown XML element `%s'", s->name );
	p->valid = 0;
	return;
    }
    
    DEBUG( DEBUG_XMLPARSE, "mapped to id %d\n", s->elm->id );

    /* Do we want cdata? */
    p->want_cdata = ((p->current->elm->flags & HIP_XML_CDATA) 
		     == HIP_XML_CDATA);
    p->collect = ((p->current->elm->flags & HIP_XML_COLLECT) 
		  == HIP_XML_COLLECT);

    /* expat is not a validating parser - check the new element
     * is valid in the current context.
     */
    DEBUG( DEBUG_XML, "Checking context of %s@@%s: element %s (parent: %s)\n", 
	   s->nspace, s->name, friendly_name(s->elm), friendly_name(s->parent->elm) );
    if( (*s->handler->validate_cb)( s->parent->elm->id, s->elm->id ) ) {
	DEBUG( DEBUG_XML, "Invalid context.\n" );
	snprintf( p->error, BUFSIZ, 
		  _("XML is not valid (%s found in parent %s)"),
		  friendly_name(s->elm), friendly_name(s->parent->elm) );
	p->valid = 0;
    } else {
	DEBUG( DEBUG_XML, "Valid context.\n" );
	if( s->handler->startelm_cb ) {
	    if( (*s->handler->startelm_cb)( s->handler->userdata, s, atts ) ) {
		DEBUG( DEBUG_XML, "Startelm callback failed.\n" );
		p->valid = 0;
	    }
	} else {
	    DEBUG( DEBUG_XML, "No startelm handler.\n" );
	}
    }

}

/* Destroys given state */
static void destroy_state( struct hip_xml_state *s ) {
    struct hip_xml_nspace *this_ns, *next_ns;
    DEBUG( DEBUG_XMLPARSE, "Freeing namespaces...\n" );
    HTTP_FREE( s->default_ns );
    HTTP_FREE( s->name );
    /* Free the namespaces */
    this_ns = s->nspaces;
    while( this_ns != NULL ) {
	next_ns = this_ns->next;
	free( this_ns->name );
	free( this_ns->uri );
	free( this_ns );
	this_ns = next_ns;
    };
    DEBUG( DEBUG_XMLPARSE, "Finished freeing namespaces.\n" );
    free( s );
}

static void char_data( void *userdata, const hip_xml_char *data, int len ) {
    struct hip_xml_parser *p = userdata;
    
    if( !p->want_cdata || !p->valid ) return;
    /* First, if this is the beginning of the CDATA, skip all
     * leading whitespace, we don't want it. */
    DEBUG( DEBUG_XMLPARSE, "Given %d bytes of cdata.\n", len );
    if( sbuffer_size(p->buffer) == 0 ) {
	size_t wslen = 0;
	/* Ignore any leading whitespace */
	while( wslen < len && 
	       ( data[wslen] == ' ' || data[wslen] == '\r' ||
		 data[wslen] == '\n' || data[wslen] == '\t' ) ) {
	    wslen++;
	}
	data += wslen;
	len -= wslen;
	DEBUG( DEBUG_XMLPARSE, "Skipped %d bytes of leading whitespace.\n", 
	       wslen );
	if( len == 0 ) {
	    DEBUG( DEBUG_XMLPARSE, "Zero bytes of content.\n" );
	    return;
	}
    }

#ifdef HIP_XML_DECODE_UTF8

    if( (p->current->elm->flags & HIP_XML_UTF8DECODE) == HIP_XML_UTF8DECODE ) {
	int n, m, clen;
	char *dest;

	clen = sbuffer_size(p->buffer);
	sbuffer_grow( p->buffer, clen + len + 1 );
	dest = sbuffer_data( p->buffer ) + clen;

	for( n = 0, m = 0; n < len; n++, m++ ) {
	    if( SINGLEBYTE_UTF8( data[n] ) ) {
		dest[m] = data[n];
	    } else {
		/* An optimisation here: we only deal with 8-bit 
		 * data, which will be encoded as two bytes of UTF-8 */
		if( (len - n < 2) ||
		    decode_utf8_double( &dest[m], &data[n] ) ) {
		    /* Failed to decode! */
		    DEBUG( DEBUG_XML, "Could not decode UTF-8 data.\n" );
		    strcpy( p->error, "XML parser received non-8-bit data" );
		    p->valid = 0;
		    return;
		} else {
#if 0
		    DEBUG( DEBUG_XML, "UTF-8 two-bytes decode: "
			   "0x%02hx 0x%02hx -> 0x%02hx!\n",
			   data[n] & 0xFF, data[n+1] & 0xFF, dest[m] & 0xFF );
#endif
		    /* Skip the second byte */
		    n += 2;
		}
	    }
	}
	sbuffer_altered( p->buffer );
    } else {
	sbuffer_append( p->buffer, data, len );
    }

#else /* !HIP_XML_DECODE_UTF8 */

    sbuffer_append( p->buffer, data, len );

#endif

}

/* Called with the end of an element */
static void end_element( void *userdata, const hip_xml_char *name ) {
    struct hip_xml_parser *p = userdata;
    struct hip_xml_state *s = p->current;
    if( !p->valid ) {
	/* We've stopped parsing */
	DEBUG( DEBUG_XML, "Parse died. Ignoring end of element: %s\n", name );
	return;
    }
    if( p->collect > 0 ) {
	if( --p->collect ) {
	    sbuffer_concat( p->buffer, "</", name, ">", NULL );
	    return;
	}
    }
	
    /* process it */
    if( s->handler->endelm_cb ) {
	DEBUG( DEBUG_XMLPARSE, "Calling endelm callback for %s.\n", s->elm->name );
	if( (*s->handler->endelm_cb)( s->handler->userdata, s,
				      p->want_cdata?sbuffer_data(p->buffer):
				      NULL ) ) {
	    DEBUG( DEBUG_XML, "Endelm callback failed.\n" );
	    p->valid = 0;
	}
    }
    p->current = s->parent;
    /* Move the current pointer up the branch */
    DEBUG( DEBUG_XML, "Back in element: %s\n", friendly_name(p->current->elm) );
    if( p->want_cdata ) {
	sbuffer_clear( p->buffer );
    } 
    destroy_state( s );
}

/* Parses the attributes, and handles XML namespaces. 
 * With a little bit of luck.
 * Returns:
 *   the element name on success
 *   or NULL on error.
 */
static int parse_element( struct hip_xml_parser *p, struct hip_xml_state *state,
			  const hip_xml_char *name, const hip_xml_char **atts )
{
    struct hip_xml_nspace *ns;
    const hip_xml_char *pnt;
    struct hip_xml_state *xmlt;

    DEBUG( DEBUG_XMLPARSE, "Parsing elm of name: [%s]\n", name );
    /* Parse the atts for namespace declarations... if we have any atts.
     * expat will never pass us atts == NULL, but libxml will. */
    if( atts != NULL ) {
	int attn;
	for( attn = 0; atts[attn]!=NULL; attn+=2 ) {
	    DEBUG( DEBUG_XMLPARSE, "Got attribute: [%s] = [%s]\n", atts[attn], atts[attn+1] );
	    if( strcasecmp( atts[attn], "xmlns" ) == 0 ) {
		/* New default namespace */
		state->default_ns = xstrdup( atts[attn+1] );
		DEBUG( DEBUG_XMLPARSE, "New default namespace: %s\n", 
		       state->default_ns );
	    } else if( strncasecmp( atts[attn], "xmlns:", 6 ) == 0 ) {
		/* New namespace scope */
		ns = xmalloc( sizeof( struct hip_xml_nspace ) );
		ns->next = state->nspaces;
		state->nspaces = ns;
		ns->name = xstrdup( atts[attn]+6 ); /* skip the xmlns= */
		ns->uri = xstrdup( atts[attn+1] );
		DEBUG( DEBUG_XMLPARSE, "New namespace scope: %s -> %s\n",
		       ns->name, ns->uri );
	    }
	}
    }
    /* Now check the elm name for a namespace scope */
    pnt = strchr( name, ':' );
    if( pnt == NULL ) {
	/* No namespace prefix - have we got a default? */
	state->name = xstrdup(name);
	DEBUG( DEBUG_XMLPARSE, "No prefix found, searching for default.\n" );
	for( xmlt = state; xmlt!=NULL; xmlt=xmlt->parent ) {
	    if( xmlt->default_ns != NULL ) {
		state->nspace = xmlt->default_ns;
		break;
	    }
	}
	if( state->nspace == NULL ) {
	    DEBUG( DEBUG_XMLPARSE, "No default namespace, using empty.\n" );
	    state->nspace = "";
	}
    } else {
	DEBUG( DEBUG_XMLPARSE, "Got namespace scope. Trying to resolve..." );
	/* Have a scope - resolve it */
	for( xmlt = state; state->nspace==NULL && xmlt!=NULL; xmlt=xmlt->parent ) {
	    for( ns = xmlt->nspaces; ns!=NULL; ns=ns->next ) {
		/* Just compare against the bit before the :
		 * pnt points to the colon. */
		if( strncasecmp( ns->name, name, pnt-name ) == 0 ) {
		    /* Scope matched! Hoorah */
		    state->nspace = ns->uri;
		    /* end the search */
		    break;
		}
	    }
	}
	if( state->nspace != NULL ) {
	    DEBUG( DEBUG_XMLPARSE, "Resolved prefix to [%s]\n", state->nspace );
	    /* The name is everything after the ':' */
	    if( pnt[1] == '\0' ) {
		snprintf( p->error, BUFSIZ, 
			  "Element name missing in '%s' at line %d.",
			  name, hip_xml_currentline(p) );
		DEBUG( DEBUG_XMLPARSE, "No element name after ':'. Failed.\n" );
		return -1;
	    }
	    state->name = xstrdup(pnt+1);
	} else {
	    DEBUG( DEBUG_XMLPARSE, "Undeclared namespace.\n" );
	    snprintf( p->error, BUFSIZ, 
		      "Undeclared namespace in '%s' at line %d.",
		      name, hip_xml_currentline(p) );
	    return -1;
	}
    }
    return 0;
}

void hip_xml_init( struct hip_xml_parser *p, struct hip_xml_handler *handlers ) 
{
    /* Initialize the expat stuff */
    memset( p, 0, sizeof( struct hip_xml_parser ) );
    /* Initialize other stuff */
    p->valid = 1;
    /* Placeholder for the root element */
    p->current = p->root = xmalloc( sizeof(struct hip_xml_state) );
    memset( p->root, 0, sizeof(struct hip_xml_state) );
    p->root->elm = &root_element;
    p->handlers = handlers;
    /* Initialize the cdata buffer */
    p->buffer = sbuffer_create();
#ifdef HAVE_EXPAT
    p->parser = XML_ParserCreate( NULL );
    if( p->parser == NULL ) {
	abort();
    }
    XML_SetElementHandler( p->parser, start_element, end_element );
    XML_SetCharacterDataHandler( p->parser, char_data );
    XML_SetUserData( p->parser, (void *) p );
#else
    p->parser = xmlCreatePushParserCtxt( &sax_handler, (void *)p, NULL, 0, NULL );
    if( p->parser == NULL ) {
	abort();
    }
#endif
}   

void hip_xml_parse_v( void *userdata, const char *block, size_t len ) 
{
    struct hip_xml_parser *p = userdata;
    /* FIXME: The two XML parsers break all our nice abstraction by
     * choosing different char *'s. The swine. This may kill us some
     * day. */
    hip_xml_parse( p, (const hip_xml_char *) block, len );
}

/* Parse the given block of input of length len */
void hip_xml_parse( struct hip_xml_parser *p, const hip_xml_char *block, size_t len ) 
{
    int ret, flag;
    /* duck out if it's broken */
    if( !p->valid ) {
	DEBUG( DEBUG_XML, "Not parsing %d bytes.\n", len );
	return;
    }
    if( len == 0 ) {
	flag = -1;
	block = "";
	DEBUG( DEBUG_XML, "Got 0-length buffer, end of document.\n" );
    } else {	
	DEBUG( DEBUG_XML, "Parsing %d length buffer.\n", len );
	flag = 0;
    }
    /* Note, don't write a parser error if !p->valid, since an error
     * will already have been written in that case. */
#ifdef HAVE_EXPAT
    ret = XML_Parse( p->parser, block, len, flag );
    DEBUG( DEBUG_XMLPARSE, "XML_Parse returned %d\n", ret );
    if( ret == 0 && p->valid ) {
	snprintf( p->error, BUFSIZ,
		  "XML parse error at line %d: %s", 
		  XML_GetCurrentLineNumber(p->parser),
		  XML_ErrorString(XML_GetErrorCode(p->parser)) );
	p->valid = 0;
    }
#else
    ret = xmlParseChunk( p->parser, block, len, flag );
    DEBUG( DEBUG_XMLPARSE, "xmlParseChunk returned %d\n", ret );
    if( p->parser->errNo && p->valid ) {
	/* FIXME: error handling */
	snprintf( p->error, BUFSIZ, "XML parse error at line %d.", 
		  hip_xml_currentline(p) );
	p->valid = 0;
    }
#endif

}

int hip_xml_finish( struct hip_xml_parser *p ) 
{
    struct hip_xml_state *s, *parent;
    sbuffer_destroy( p->buffer );
    /* Clean up any states which may remain.
     * If p.valid, then this should be only the root element. */
    for( s = p->current; s!=NULL; s=parent ) {
	parent = s->parent;
	destroy_state( s );
    }
#ifdef HAVE_EXPAT
    XML_ParserFree( p->parser );
#else
    xmlFreeParserCtxt( p->parser );
#endif
    return !p->valid;
}

