/* 
   neon test suite
   Copyright (C) 2002-2004, Joe Orton <joe@manyfish.co.uk>

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

#define ABORT (-42) /* magic code for abort handlers */

/* A set of SAX handlers which serialize SAX events back into a 
 * pseudo-XML-like string. */
static int startelm(void *userdata, int state,
                    const char *nspace, const char *name,
		    const char **atts)
{
    ne_buffer *buf = userdata;
    int n;

    if (strcmp(name, "decline") == 0)
        return NE_XML_DECLINE;

    ne_buffer_concat(buf, "<", "{", nspace, "}", name, NULL);
    for (n = 0; atts && atts[n] != NULL; n+=2) {
	ne_buffer_concat(buf, " ", atts[n], "='", atts[n+1], "'", NULL);
    }
    ne_buffer_zappend(buf, ">");

    return state + 1;
}

static int chardata(void *userdata, int state, const char *cdata, size_t len)
{
    ne_buffer *buf = userdata;
    ne_buffer_append(buf, cdata, len);
    return strncmp(cdata, "!ABORT!", len) == 0 ? ABORT : NE_XML_DECLINE;
}

static int endelm(void *userdata, int state,
                  const char *nspace, const char *name)
{
    ne_buffer *buf = userdata;
    ne_buffer_concat(buf, "</{", nspace, "}", name, ">", NULL);
    return 0;
}

/* A set of SAX handlers which do as above, but change some element
 * names; used to check nested SAX handling is working properly.  */
static int startelm_xform(void *userdata, int state,
                          const char *nspace, const char *name,
                          const char **atts)
{
    if (strcmp(nspace, "two") == 0)
        return startelm(userdata, state, nspace, "xform", atts);
    else
        return NE_XML_DECLINE;
}

static int endelm_xform(void *userdata, int state,
                        const char *nspace, const char *name)
{
    if (strcmp(nspace, "two") == 0)
        return endelm(userdata, state, nspace, "xform");
    else
        return NE_XML_DECLINE;
}

/* A set of SAX handlers which verify that state handling is working
 * correctly. */
static int startelm_state(void *userdata, int parent,
                          const char *nspace, const char *name,
                          const char **atts)
{
    int n;

    if (strcmp(nspace, "state") != 0)
        return NE_XML_DECLINE;

    for (n = 0; atts[n]; n += 2) {
        if (strcmp(atts[n], "parent") == 0) {
            int expected = atoi(atts[n+1]);
            
            if (expected != parent) {
                char err[50];
                sprintf(err, "parent state of %s was %d not %d", name, parent, 
                    expected);
                ne_buffer_zappend(userdata, err);
            }
        }
    }

    return atoi(name+1);    
}

static int endelm_state(void *userdata, int state,
                        const char *nspace, const char *name)
{
    int expected = atoi(name + 1);
    ne_buffer *buf = userdata;
    
    if (state != expected)
        ne_buffer_concat(buf, "wrong state in endelm of ", name, NULL);
    
    return 0;
}

/* A set of SAX handlers which verify that abort handling is working
 * correctly. */
static int startelm_abort(void *buf, int parent,
                          const char *nspace, const char *name,
                          const char **atts)
{
    if (strcmp(name, "abort-start") == 0) {
        ne_buffer_zappend(buf, "ABORT");
        return ABORT;
    } else
        return startelm(buf, parent, nspace, name, atts);
}

static int endelm_abort(void *buf, int state,
                        const char *nspace, const char *name)
{
    if (strcmp(name, "abort-end") == 0) {
        ne_buffer_zappend(buf, "ABORT");
        return ABORT;
    } else
        return 0;
}

/* Test mode for parse_match: */
enum match_type {
    match_valid = 0, /* test that the parse succeeds */
    match_invalid, /* test that the parse fails */
    match_nohands, /* test with no handlers registered */
    match_encoding, /* test whether the encoding is equal to the result string */
    match_chunked /* parse the document one byte at a time */
};

static int parse_match(const char *doc, const char *result, enum match_type t)
{
    ne_xml_parser *p = ne_xml_create();
    ne_buffer *buf = ne_buffer_create();
    int ret;

    if (t == match_invalid)
        ne_xml_push_handler(p, startelm_abort, chardata, endelm_abort, buf);
    if (t != match_encoding && t != match_nohands) {
        ne_xml_push_handler(p, startelm_state, NULL, endelm_state, buf);
        ne_xml_push_handler(p, startelm, chardata, endelm, buf);
        ne_xml_push_handler(p, startelm_xform, chardata, endelm_xform, buf);
    }

    if (t == match_chunked) {
        do {
            ret = ne_xml_parse(p, doc++, 1);
        } while (ret == 0 && *doc);
    } else {
        ret = ne_xml_parse(p, doc, strlen(doc));
    }

    if (ret == 0) {
        ne_xml_parse(p, "", 0);
    }

    ONV(ret != ne_xml_failed(p), 
        ("ne_xml_failed gave %d not %d", ne_xml_failed(p), ret));

    if (t == match_invalid)
        ONV(ret != ABORT, 
            ("parse got %d not abort failure: %s", ret, buf->data));
    else
        ONV(ret, ("parse failed: %s", ne_xml_get_error(p)));
    
    if (t == match_encoding) {
        const char *enc = ne_xml_doc_encoding(p);
        ONV(strcmp(enc, result), ("encoding was `%s' not `%s'", enc, result));
    } else if (t == match_valid)
        ONV(strcmp(result, buf->data),
            ("result mismatch: %s not %s", buf->data, result));
        
    ne_xml_destroy(p);
    ne_buffer_destroy(buf);

    return OK;
}

static int matches(void)
{
#define PFX "<?xml version='1.0' encoding='utf-8'?>\r\n"
#define E(ns, n) "<{" ns "}" n "></{" ns "}" n ">"
    static const struct {
	const char *in, *out;
        enum match_type invalid;
    } ms[] = {
        
        /*** Simplest tests ***/
	{ PFX "<hello/>", "<{}hello></{}hello>"},
	{ PFX "<hello foo='bar'/>",
	  "<{}hello foo='bar'></{}hello>"},

	/*** Tests for character data handling. ***/
	{ PFX "<hello> world</hello>", "<{}hello> world</{}hello>"},
	/* test for cdata between elements. */
	{ PFX "<hello>\r\n<wide>  world</wide></hello>",
	  "<{}hello>\n<{}wide>  world</{}wide></{}hello>"},

        /* UTF-8 XML Byte Order Mark */
        { "\xEF\xBB\xBF" PFX "<hello/>", "<{}hello></{}hello>" },
        /* UTF-8 XML Byte Order Mark */
        { "\xEF\xBB\xBF" PFX "<hello/>", "<{}hello></{}hello>", match_chunked },

	/*** Tests for namespace handling. ***/
#define NSA "xmlns:foo='bar'"
	{ PFX "<foo:widget " NSA "/>", 
	  "<{bar}widget " NSA ">"
	  "</{bar}widget>" },
	/* inherited namespace expansion. */
	{ PFX "<widget " NSA "><foo:norman/></widget>",
	  "<{}widget " NSA ">" E("bar", "norman") "</{}widget>"},
	{ PFX "<widget " NSA " xmlns:abc='def' xmlns:g='z'>"
          "<foo:norman/></widget>",
	  "<{}widget " NSA " xmlns:abc='def' xmlns:g='z'>" 
          E("bar", "norman") "</{}widget>"},
	/* empty namespace default takes precedence. */
	{ PFX "<widget xmlns='foo'><smidgen xmlns=''><norman/>"
	  "</smidgen></widget>",
	  "<{foo}widget xmlns='foo'><{}smidgen xmlns=''>" 
	  E("", "norman") 
	  "</{}smidgen></{foo}widget>" },
        /* inherited empty namespace default */
        { PFX "<bar xmlns='foo'><grok xmlns=''><fish/></grok></bar>",
          "<{foo}bar xmlns='foo'><{}grok xmlns=''>" 
          E("", "fish") "</{}grok></{foo}bar>" },

	/* regression test for neon <= 0.23.5 with libxml2, where the
	 * "dereference entities" flag was not set by default. */
	{ PFX "<widget foo=\"no&amp;body\"/>",
	  "<{}widget foo='no&body'></{}widget>" },
	{ PFX "<widget foo=\"no&#x20;body\"/>",
	  "<{}widget foo='no body'></{}widget>" },

        /* tests for declined branches */
        { PFX "<hello><decline>fish</decline>"
          "<world><decline/>yes</world>goodbye<decline/></hello>",
          "<{}hello><{}world>yes</{}world>goodbye</{}hello>" },
        { PFX 
          "<hello><decline><nested>fish</nested>bar</decline><fish/></hello>",
          "<{}hello>" E("", "fish") "</{}hello>" },
        /* tests for nested SAX handlers */
        { PFX "<hello xmlns='two'><decline/></hello>",
          "<{two}hello xmlns='two'>" E("two", "xform") "</{two}hello>"},

        /* tests for state handling */
        { PFX "<a55 xmlns='state'/>", "" },
        { PFX "<a777 xmlns='state' parent='0'/>", "" },
        { PFX "<a99 xmlns='state'><f77 parent='99'/>blah</a99>", "" },

        /* tests for abort handling */
        { PFX "<hello><merry><abort-start/></merry></hello>",
          "<{}hello><{}merry>ABORT", match_invalid },
        { PFX "<hello><merry><abort-end/>fish</merry></hello>",
          "<{}hello><{}merry><{}abort-end>ABORT", match_invalid },
        { PFX "<hello>!ABORT!</hello>", "<{}hello>!ABORT!", match_invalid },
        { PFX "<hello>!ABORT!<foo/></hello>", "<{}hello>!ABORT!", match_invalid },
        { PFX "<hello>!ABORT!</fish>", "<{}hello>!ABORT!", match_invalid },

        /* tests for encodings */
        { "<?xml version='1.0' encoding='ISO-8859-1'?><hello/>",
          "ISO-8859-1", match_encoding },

        { "<?xml version='1.0' encoding='UTF-8'?><hello/>",
          "UTF-8", match_encoding },

        /* test that parse is valid even with no handlers registered. */
        { PFX "<hello><old>world</old></hello>", "", match_nohands },

        /* regression test for prefix matching bug fixed in 0.18.0 */
#define THENS "xmlns:d='foo' xmlns:dd='bar'"
        { PFX "<d:hello " THENS "/>",
          "<{foo}hello " THENS "></{foo}hello>" },

        /**** end of list ****/ { NULL, NULL }
    };
    int n;

    for (n = 0; ms[n].in != NULL; n++) {
	CALL(parse_match(ms[n].in, ms[n].out, ms[n].invalid));
    }

    return OK;
}

static int mapping(void)
{
    static const struct ne_xml_idmap map[] = {
        { "fee", "bar", 1 },
        { "foo", "bar", 2 },
        { "bar", "foo", 3 },
        { "", "bob", 4 },
        { "balloon", "buffoon", 5},
        { NULL, NULL, 0}
    };
    int n;

    for (n = 0; map[n].id; n++) {
        int id = ne_xml_mapid(map, NE_XML_MAPLEN(map) - 1,
                              map[n].nspace, map[n].name);
        ONV(id != map[n].id, ("mapped to id %d not %d", id, map[n].id));
    }

    n = ne_xml_mapid(map, NE_XML_MAPLEN(map) - 1, "no-such", "element");
    ONV(n != 0, ("unknown element got id %d not zero", n));

    return OK;
}

/* Test for some parse failures */
static int fail_parse(void)
{
    static const char *docs[] = {
        "foo",
        PFX "<bar:foo/>",
        /* malformed namespace declarations */
        PFX "<foo xmlns:D=''/>",
        PFX "<foo xmlns:='fish'/>",
        PFX "<foo xmlns:.bar='fish'/>",
        PFX "<foo xmlns:-bar='fish'/>",
        PFX "<foo xmlns:0bar='fish'/>",
        PFX "<fee xmlns:8baz='bar'/>",

        /* element names which are not valid QNames. */
        PFX "<foo: xmlns:foo='bar'/>",
        PFX "<:fee/>",
        PFX "<0fish/>",
        PFX "<foo:0fish xmlns:foo='bar'/>",
        PFX "<foo:9fish xmlns:foo='bar'/>",
        PFX "<foo:-fish xmlns:foo='bar'/>",
        PFX "<foo:.fish xmlns:foo='bar'/>",

#if 0 /* currently disabled to allow SVN to work */
        PFX "<foo:bar:baz xmlns:foo='bar'/>",
        PFX "<fee xmlns:baz:bar='bar'/>",
        PFX "<fee xmlns::bar='bar'/>",
        PFX "<foo::fish xmlns:foo='bar'/>",
#endif

#if 0
        /* 2-byte encoding of '.': */
        PFX "<foo>" "\x2F\xC0\xAE\x2E\x2F" "</foo>",
        /* 3-byte encoding of '.': */
        PFX "<foo>" "\x2F\xE0\x80\xAE\x2E\x2F" "</foo>",
        /* 4-byte encoding of '.': */
        PFX "<foo>" "\x2F\xF0\x80\x80\xAE\x2E\x2F" "</foo>",
        /* 5-byte encoding of '.': */
        PFX "<foo>" "\x2F\xF8\x80\x80\x80\xAE\x2E\x2F" "</foo>",
        /* 6-byte encoding of '.': */
        PFX "<foo>" "\x2F\xFC\x80\x80\x80\x80\xAE\x2E\x2F" "</foo>",
#endif
        /* two-byte encoding of '<' must not be parsed as a '<': */
        PFX "\xC0\xBC" "foo></foo>",

        /* Invalid UTF-8 XML Byte Order Marks */
        "\xEF\xBB" PFX "<hello/>",
        "\xEF" PFX "<hello/>",

        NULL
    };
    int n;

    for (n = 0; docs[n]; n++) {
        ne_xml_parser *p = ne_xml_create();
        const char *err;

        ne_xml_parse(p, docs[n], strlen(docs[n]));
        ne_xml_parse(p, "", 0);
        ONV(ne_xml_failed(p) <= 0, 
            ("`%s' did not get positive parse error", docs[n]));

        err = ne_xml_get_error(p);
        NE_DEBUG(NE_DBG_HTTP, "Parse error for '%s': %s\n", docs[n], err);
        ONV(strstr(err, "parse error") == NULL
            && strstr(err, "Invalid Byte Order Mark") == NULL,
            ("bad error %s", err));
        
        ne_xml_destroy(p);
    }

    return OK;
}

static int check_attrib(ne_xml_parser *p, const char **atts, 
                        const char *nspace, const char *name,
                        const char *value)
{
    const char *act = ne_xml_get_attr(p, atts, nspace, name);
    char err[50];
    int ret = 0;

    if (value == NULL) {
        if (act != NULL) {
            sprintf(err, "attribute %s was set to %s", name, act);
            ret = NE_XML_ABORT;
        }
    } else {
        if (act == NULL) {
            sprintf(err, "attribute %s not found", name);
            ret = NE_XML_ABORT;
        } else if (strcmp(act, value) != 0) {
            sprintf(err, "attribute %s was %s not %s", name, act, value);
            ret = NE_XML_ABORT;
        }
    }
    if (ret == NE_XML_ABORT) ne_xml_set_error(p, err);
    return ret;
}

static int startelm_attrib(void *userdata, int state,
                           const char *nspace, const char *name,
                           const char **atts)
{
    ne_xml_parser *p = userdata;
    
    if (strcmp(name, "hello") == 0) {
        CALL(check_attrib(p, atts, NULL, "first", "second"));
        CALL(check_attrib(p, atts, NULL, "third", ""));
        CALL(check_attrib(p, atts, "garth", "bar", "asda"));
        CALL(check_attrib(p, atts, "giraffe", "bar", NULL));
        CALL(check_attrib(p, atts, "hot", "dog", NULL));
        CALL(check_attrib(p, atts, NULL, "nonesuch", NULL));
    } else if (strcmp(name, "goodbye") == 0) {
        if (atts[0] != NULL) {
            ne_xml_set_error(p, "non-empty attrib array");
            return 1;
        }
    }

    return 1;
}

static int attributes(void)
{
    ne_xml_parser *p = ne_xml_create();
    static const char doc[] = PFX
        "<hello xmlns:foo='garth' first='second' third='' "
        "foo:bar='asda' goof:bar='jar'><goodbye/></hello>";

    ne_xml_push_handler(p, startelm_attrib, NULL, NULL, p);

    ne_xml_parse_v(p, doc, strlen(doc));

    ONV(ne_xml_failed(p), ("parse error: %s", ne_xml_get_error(p)));

    ne_xml_destroy(p);
    return OK;
}

/* Test for the get/set error interface */
static int errors(void)
{
    ne_xml_parser *p = ne_xml_create();
    const char *err;
    
    ONV(strcmp(ne_xml_get_error(p), "Unknown error") != 0,
        ("initial error string unspecified"));

    ne_xml_set_error(p, "Fish food");
    err = ne_xml_get_error(p);
    
    ONV(strcmp(err, "Fish food"), ("wrong error %s!", err));

    ne_xml_destroy(p);
    return 0;        
}

ne_test tests[] = {
    T(matches),
    T(mapping),
    T(fail_parse),
    T(attributes),
    T(errors),
    T(NULL)
};

