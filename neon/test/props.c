/* 
   Tests for property handling
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

#include "ne_props.h"

#include "tests.h"
#include "child.h"
#include "utils.h"

static const ne_propname p_alpha = {"DAV:", "alpha"},
    p_beta = {"http://webdav.org/random/namespace", "beta"},
    p_delta = {NULL, "delta"};

/* Tests little except that ne_proppatch() doesn't segfault. */
static int patch_simple(void)
{
    ne_session *sess;
    ne_proppatch_operation ops[] = {
	{ &p_alpha, ne_propset, "fish" },
	{ &p_beta, ne_propremove, NULL },
	{ NULL, ne_propset, NULL }
    };
    
    CALL(make_session(&sess, single_serve_string, 
		      "HTTP/1.1 200 Goferit\r\n"
		      "Connection: close\r\n\r\n"));
    ONREQ(ne_proppatch(sess, "/fish", ops));
    ne_session_destroy(sess);
    return await_server();
}

#define RESP207 "HTTP/1.0 207 Stuff\r\n" "Server: foo\r\n\r\n"

static void dummy_results(void *ud, const char *href,
			  const ne_prop_result_set *rset)
{
    NE_DEBUG(NE_DBG_HTTP, "dummy_results.\n");
}

/* Regression tests for propfind bodies which caused segfaults. */
static int regress(void)
{
    static const char *bodies[] = { 
	RESP207 "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
	"<multistatus xmlns=\"DAV:\">"
	"<response><propstat><prop><href>"
	"</href></prop></propstat></response>"
	"</multistatus>",
	
	/* segfaults with neon <= 0.23.5 */
	RESP207 "<?xml version=\"1.0\"?><D:multistatus xmlns:D=\"DAV:\">"
	"<D:response><D:href>/foo/</D:href>"
	"<D:propstat/>"
	"<D:status>HTTP/1.1 404 Not Found</D:status>"
	"</D:multistatus>",

	/* format string handling with neon <= 0.24.4 */
	RESP207 "<?xml version=\"1.0\"?><D:multistatus xmlns:D=\"DAV:\">"
	"<D:response><D:href>/foo/</D:href>"
	"<D:propstat/>"
	"<D:status>%s%s%s%s</D:status>"
	"</D:response></D:multistatus>",

	NULL,
    };
    ne_session *sess;
    int n;

    for (n = 0; bodies[n] != NULL; n++) {
	CALL(make_session(&sess, single_serve_string, (void *)bodies[n]));
	ne_simple_propfind(sess, "/", 0, NULL, dummy_results, NULL);
	ne_session_destroy(sess);
	CALL(await_server());
    }

    return OK;
}

static int patch_regress(void)
{
    static const char *bodies[] = { 
	/* format string handling bugs with neon <= 0.24.4 */
	RESP207 "<?xml version=\"1.0\"?><D:multistatus xmlns:D=\"DAV:\">"
	"<D:response><D:href>/foo/</D:href>"
	"<D:status>HTTP/1.1 500 Bad Voodoo</D:status>"
	"<D:responsedescription>%s%s%s%s</D:responsedescription>"
        "</D:response></D:multistatus>",

	RESP207 "<?xml version=\"1.0\"?><D:multistatus xmlns:D=\"DAV:\">"
	"<D:response><D:href>/foo/</D:href>"
	"<D:status>HTTP/1.1 %s%s%s%s</D:status>",

        NULL
    };
    ne_session *sess;
    int n;
    static const ne_propname pn = { "DAV:", "foobar" };
    ne_proppatch_operation pops[] = { 
        { &pn, ne_propset, "fish" },
        { NULL, ne_propset, NULL }
    };

    for (n = 0; bodies[n] != NULL; n++) {
	CALL(make_session(&sess, single_serve_string, (void *)bodies[n]));
	ne_proppatch(sess, "/", pops);
	ne_session_destroy(sess);
	CALL(await_server());
    }

    return OK;
}

static int pstat_count;

/* tos_*: set of 207 callbacks which serialize the data back into a
 * text stream, which can be easily checked for correctness. */
static void *tos_startresp(void *buf, const char *href)
{
    ne_buffer_concat(buf, "start-resp[", href, "];", NULL);
    pstat_count = 0;
    return ne_strdup(href);
}

static void tos_status_descr(ne_buffer *buf, const ne_status *status,
                             const char *description)
{
    if (status) {
        char s[50];
        ne_snprintf(s, sizeof s, "-status={%d %s}", status->code,
                    status->reason_phrase);
        ne_buffer_zappend(buf, s);
    }
    if (description)
        ne_buffer_concat(buf, "-descr={", description, "}", NULL);
}

static void tos_endresp(void *buf, void *response,
                        const ne_status *status, const char *description)
{
    char *href = response;
    ne_buffer_concat(buf, "end-resp[", href, "]", NULL);
    ne_free(href);
    tos_status_descr(buf, status, description);
    ne_buffer_zappend(buf, ";");
}

static void *tos_startpstat(void *buf, void *resphref)
{
    char num[20], *href;
    sprintf(num, "-%d", ++pstat_count);
    href = ne_concat(resphref, num, NULL);
    ne_buffer_concat(buf, "start-pstat[", href, "];", NULL);
    return href;
}

static void tos_endpstat(void *buf, void *href,
                         const ne_status *status, const char *description)
{
    ne_buffer_concat(buf, "end-pstat[", href, "]", NULL);
    tos_status_descr(buf, status, description);
    ne_buffer_zappend(buf, ";");
    ne_free(href);
}

struct propctx {
    ne_207_parser *p207;
    ne_buffer *buf;
};

#define STATE_myprop (NE_PROPS_STATE_TOP)

static int tos_startprop(void *userdata, int parent,
                         const char *nspace, const char *name,
                         const char **atts)
{
    if (parent == NE_207_STATE_PROP && 
        strcmp(nspace, "DAV:") == 0 && 
        (strcmp(name, "propone") == 0 || strcmp(name, "proptwo") == 0)) {
        /* Handle this! */
        struct propctx *ctx = userdata;
        char *resphref = ne_207_get_current_response(ctx->p207);
        char *pstathref = ne_207_get_current_propstat(ctx->p207);
        
        ne_buffer_concat(ctx->buf, "start-prop[", resphref, ",", pstathref,
                         ",", name, "];", NULL);

        return STATE_myprop;
    } else {
        return NE_XML_DECLINE;
    }
}

static int tos_cdata(void *userdata, int state,
                     const char *cdata, size_t len)
{
    struct propctx *ctx = userdata;

    ne_buffer_zappend(ctx->buf, "cdata-prop[");
    ne_buffer_append(ctx->buf, cdata, len);
    ne_buffer_zappend(ctx->buf, "];");
    return 0;
}

static int tos_endprop(void *userdata, int state,
                       const char *nspace, const char *name)
{
    struct propctx *ctx = userdata;

    ne_buffer_concat(ctx->buf, "end-prop[", name, "];", NULL);
    return 0;
}


static int run_207_response(char *resp, const char *expected)
{
    ne_buffer *buf = ne_buffer_create();
    ne_session *sess = ne_session_create("http", "localhost", 7777);
    ne_xml_parser *p = ne_xml_create();
    ne_207_parser *p207 = ne_207_create(p, buf);
    ne_request *req = ne_request_create(sess, "PROPFIND", "/foo");
    struct propctx ctx;

    ne_add_response_body_reader(req, ne_accept_207, ne_xml_parse_v, p);

    ne_207_set_response_handlers(p207, tos_startresp, tos_endresp);
    ne_207_set_propstat_handlers(p207, tos_startpstat, tos_endpstat);

    ctx.buf = buf;
    ctx.p207 = p207;
    ne_xml_push_handler(p, tos_startprop, tos_cdata, tos_endprop, &ctx);

    CALL(spawn_server(7777, single_serve_string, resp));

    ONREQ(ne_request_dispatch(req));

    CALL(await_server());

    ONV(ne_xml_failed(p),
        ("parse error in response body: %s", ne_xml_get_error(p)));

    ONV(strcmp(buf->data, expected),
        ("comparison failed.\n"
         "expected string: `%s'\n"
         "got string:      `%s'", expected, buf->data));

    ne_buffer_destroy(buf);
    ne_207_destroy(p207);
    ne_xml_destroy(p);
    ne_request_destroy(req);
    ne_session_destroy(sess);
    return OK;
}

/* Macros for easily writing a 207 response body; all expand to
 * a string literal. */
#define MULTI_207(x) "HTTP/1.0 207 Foo\r\nConnection: close\r\n\r\n" \
"<?xml version='1.0'?>\r\n" \
"<D:multistatus xmlns:D='DAV:'>" x "</D:multistatus>"
#define RESP_207(href, x) "<D:response><D:href>" href "</D:href>" x \
"</D:response>"
#define PSTAT_207(x) "<D:propstat>" x "</D:propstat>"
#define STAT_207(s) "<D:status>HTTP/1.1 " s "</D:status>"
#define DESCR_207(d) "<D:responsedescription>" d "</D:responsedescription>"
#define DESCR_REM "The end of the world, as we know it"

#define PROPS_207(x) "<D:prop>" x "</D:prop>"
#define APROP_207(n, c) "<D:" n ">" c "</D:" n ">"

/* Tests for the 207 interface: send a 207 response body, compare the
 * re-serialized string returned with that expected. */
static int two_oh_seven(void)
{
    static char *ts[][2] = {
        { MULTI_207(RESP_207("/foo", "")),
          "start-resp[/foo];end-resp[/foo];" },

        /* test for response status handling */
        { MULTI_207(RESP_207("/bar", STAT_207("200 OK"))), 
          "start-resp[/bar];end-resp[/bar]-status={200 OK};" },

        /* test that empty description == NULL description argument */
        { MULTI_207(RESP_207("/bar", STAT_207("200 OK") DESCR_207(""))), 
          "start-resp[/bar];end-resp[/bar]-status={200 OK};" },

        /* test multiple responses */
        { MULTI_207(RESP_207("/hello/world", STAT_207("200 OK"))
                    RESP_207("/foo/bar", STAT_207("999 French Fries"))), 
          "start-resp[/hello/world];end-resp[/hello/world]-status={200 OK};"
          "start-resp[/foo/bar];end-resp[/foo/bar]"
          "-status={999 French Fries};" 
        },

        /* test multiple propstats in mulitple responses */
        { MULTI_207(RESP_207("/al/pha", 
                             PSTAT_207(STAT_207("321 Une"))
                             PSTAT_207(STAT_207("432 Deux"))
                             PSTAT_207(STAT_207("543 Trois")))
                    RESP_207("/be/ta",
                             PSTAT_207(STAT_207("787 Quatre"))
                             PSTAT_207(STAT_207("878 Cinq")))),
          "start-resp[/al/pha];"
          "start-pstat[/al/pha-1];end-pstat[/al/pha-1]-status={321 Une};"
          "start-pstat[/al/pha-2];end-pstat[/al/pha-2]-status={432 Deux};"
          "start-pstat[/al/pha-3];end-pstat[/al/pha-3]-status={543 Trois};"
          "end-resp[/al/pha];"
          "start-resp[/be/ta];"
          "start-pstat[/be/ta-1];end-pstat[/be/ta-1]-status={787 Quatre};"
          "start-pstat[/be/ta-2];end-pstat[/be/ta-2]-status={878 Cinq};"
          "end-resp[/be/ta];"
        },

        /* test that incomplete responses are completely ignored. */
        { MULTI_207("<D:response/>"
                    RESP_207("/", STAT_207("123 Hoorah"))
                    "<D:response/>"
                    "<D:response><D:propstat>hello</D:propstat></D:response>"
                    "<D:response><D:href/></D:response>"
                    RESP_207("/bar", STAT_207("200 OK"))),
          "start-resp[/];end-resp[/]-status={123 Hoorah};"
          "start-resp[/bar];end-resp[/bar]-status={200 OK};" },

        /* tests for propstat status */
        { MULTI_207(RESP_207("/pstat",
                            PSTAT_207("<D:prop/>" STAT_207("666 Doomed")))),
          "start-resp[/pstat];start-pstat[/pstat-1];"
          "end-pstat[/pstat-1]-status={666 Doomed};end-resp[/pstat];" },

        { MULTI_207(RESP_207("/pstat", PSTAT_207("<D:status/>"))),
          "start-resp[/pstat];start-pstat[/pstat-1];"
          "end-pstat[/pstat-1];end-resp[/pstat];" },

        /* tests for responsedescription handling */
        { MULTI_207(RESP_207("/bar", STAT_207("200 OK") DESCR_207(DESCR_REM))), 
          "start-resp[/bar];end-resp[/bar]-status={200 OK}"
          "-descr={" DESCR_REM "};" },

        { MULTI_207(RESP_207("/bar",
                             PSTAT_207(STAT_207("456 Too Hungry")
                                       DESCR_207("Not enough food available"))
                             STAT_207("200 OK") DESCR_207("Not " DESCR_REM))), 
          "start-resp[/bar];"
          "start-pstat[/bar-1];end-pstat[/bar-1]-status={456 Too Hungry}"
          "-descr={Not enough food available};"
          "end-resp[/bar]-status={200 OK}-descr={Not " DESCR_REM "};" },

        /* intermingle some random elements and cdata to make sure
         * they are ignored. */
        { MULTI_207("<D:fish-food/>blargl" 
                    RESP_207("/b<ping-pong/>ar", "<D:sausages/>"
                             PSTAT_207("<D:hello-mum/>blergl") 
                             STAT_207("200 <pong-ping/> OK") "foop"
                             DESCR_207(DESCR_REM) "carroon") 
                    "carapi"), 
          "start-resp[/bar];start-pstat[/bar-1];end-pstat[/bar-1];"
          "end-resp[/bar]-status={200 OK}-descr={" DESCR_REM "};" },

        /* test for properties within a 207. */
        { MULTI_207(RESP_207("/alpha",
                             PSTAT_207(PROPS_207(
                                           APROP_207("propone", "hello")
                                           APROP_207("proptwo", "foobar"))
                                       STAT_207("200 OK")))),
          "start-resp[/alpha];start-pstat[/alpha-1];"
          "start-prop[/alpha,/alpha-1,propone];cdata-prop[hello];"
          "end-prop[propone];"
          "start-prop[/alpha,/alpha-1,proptwo];cdata-prop[foobar];"
          "end-prop[proptwo];"
          "end-pstat[/alpha-1]-status={200 OK};end-resp[/alpha];" }

    };
    size_t n;

    for (n = 0; n < sizeof(ts)/sizeof(ts[0]); n++)
        CALL(run_207_response(ts[n][0], ts[n][1]));        

    return OK;
}

/* Serialize propfind result callbacks into a string */
static int simple_iterator(void *buf, const ne_propname *name,
                           const char *value, const ne_status *st)
{
    char code[20];
    ne_buffer_concat(buf, "prop:[{", name->nspace, ",", 
                     name->name, "}=", NULL);
    if (value)
        ne_buffer_concat(buf, "'", value, "'", NULL);
    else 
        ne_buffer_zappend(buf, "#novalue#");
    sprintf(code, ":{%d ", st->code);
    if (st->reason_phrase)
        ne_buffer_concat(buf, code, st->reason_phrase, "}];", NULL);
    else
        ne_buffer_concat(buf, code, "#noreason#}];", NULL);
    return 0;
}

static void simple_results(void *buf, const char *href,
                           const ne_prop_result_set *rset)
{
    ne_buffer_concat(buf, "results(", href, ",", NULL);
    ne_propset_iterate(rset, simple_iterator, buf);
    ne_buffer_zappend(buf, ")//");
}

static int diffcmp(const char *expected, const char *actual)
{
    size_t n;
    
    if (!strcmp(expected, actual)) return OK;

    for (n = 0; expected[n] && actual[n]; n++) {
        if (expected[n] != actual[n]) {
            t_context("difference at byte %" NE_FMT_SIZE_T ": "
                      "`%.6s...' not `%.6s...'",
                      n, actual+n, expected+n);
            break;
        }
    }

    return FAIL;
}
       

static int run_simple_propfind(const ne_propname *props, char *resp, 
                               int depth, const char *expected)
{
    ne_session *sess = ne_session_create("http", "localhost", 7777);
    ne_buffer *buf = ne_buffer_create();

    CALL(spawn_server(7777, single_serve_string, resp));

    ONREQ(ne_simple_propfind(sess, "/propfind", depth, props,
                             simple_results, buf));

    CALL(await_server());

    CALL(diffcmp(expected, buf->data));

    ne_buffer_destroy(buf);
    ne_session_destroy(sess);
    return OK;
}

/* a PROPFIND response body for the {DAV:}fishbone property, using
 * given property value and status. */
#define FISHBONE_RESP(value, status) MULTI_207(RESP_207("/foop", \
	PSTAT_207(PROPS_207(APROP_207("fishbone", value)) \
	STAT_207(status))))

static int pfind_simple(void)
{
    static const struct {
        char *resp;
        const char *expected;
        int depth, pset;
    } ts[] = {
        /* simple single property. */
        { FISHBONE_RESP("hello, world", "212 Well OK"),
          "results(/foop,prop:[{DAV:,fishbone}='hello, world':{212 Well OK}];)//",
          0, 0 },
        /* property with some nested elements. */
        { FISHBONE_RESP("this is <foo/> a property <bar><lemon>fish</lemon></bar> value", 
                        "299 Just About OK"),
          "results(/foop,prop:[{DAV:,fishbone}="
          "'this is <foo></foo> a property "
          "<bar><lemon>fish</lemon></bar> value':"
          "{299 Just About OK}];)//",
          0, 0 },

        /* failed to fetch a property. */
        { FISHBONE_RESP("property value is ignored", 
                        "404 Il n'ya pas de property"),
          "results(/foop,prop:[{DAV:,fishbone}=#novalue#:"
          "{404 Il n'ya pas de property}];)//",
          0, 0 },

#if 0
        /* propstat missing status should be ignored; if a response contains no
         * valid propstats, it should also be ignored. */
        { MULTI_207(RESP_207("/alpha", PSTAT_207(APROP_207("fishbone", "unseen")))
                    RESP_207("/beta", PSTAT_207(APROP_207("fishbone", "hello, world")
                                                STAT_207("200 OK")))),
          "results(/beta,prop:[{DAV:,fishbone}='hello, world':{200 OK}];)//", 0, 0},
#endif

        /* props on several resources */
        { MULTI_207(RESP_207("/alpha",
                             PSTAT_207(PROPS_207(APROP_207("fishbone", "strike one"))
                                       STAT_207("234 First is OK")))
                    RESP_207("/beta",
                             PSTAT_207(PROPS_207(APROP_207("fishbone", "strike two"))
                                       STAT_207("256 Second is OK")))),
          "results(/alpha,prop:[{DAV:,fishbone}='strike one':{234 First is OK}];)//"
          "results(/beta,prop:[{DAV:,fishbone}='strike two':{256 Second is OK}];)//",
          0, 0},

        /* whitespace handling. */
        { MULTI_207(RESP_207("\r\nhttp://localhost:7777/alpha ",
                             PSTAT_207(PROPS_207(APROP_207("alpha", "beta"))
                                       "<D:status>\r\nHTTP/1.1 200 OK </D:status>"))),
          "results(http://localhost:7777/alpha,prop:[{DAV:,alpha}='beta':{200 OK}];)//",
          0, 0}
    };
    const ne_propname pset1[] = {
        { "DAV:", "fishbone", },
        { NULL, NULL }
    };
    size_t n;

    for (n = 0; n < sizeof(ts)/sizeof(ts[0]); n++) {
        const ne_propname *pset = pset1;

        CALL(run_simple_propfind(pset, ts[n].resp, ts[n].depth,
                                 ts[n].expected));
    }


    return OK;
}

static int unbounded_response(const char *header, const char *repeats)
{
    ne_session *sess;
    struct infinite i = { header, repeats};
    int dbg;

    CALL(make_session(&sess, serve_infinite, &i));

    dbg = ne_debug_mask;

    ONN("unbounded PROPFIND response did not fail",
        ne_simple_propfind(sess, "/", 0, NULL, 
                           dummy_results, NULL) != NE_ERROR);

    CALL(reap_server());    
    ne_session_destroy(sess);
    return OK;
}

static int unbounded_propstats(void)
{
    return unbounded_response(
	RESP207 "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
	"<multistatus xmlns=\"DAV:\">"
	"<response><href>/</href>",
        "<propstat></propstat>");
}

static int unbounded_props(void)
{
    return unbounded_response(
	RESP207 "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
	"<multistatus xmlns=\"DAV:\">"
	"<response><href>/</href><propstat>",
        "<prop><jim>hello, world</jim></prop>");
}

ne_test tests[] = {
    T(two_oh_seven),
    T(patch_simple),
    T(pfind_simple),
    T(regress),
    T(patch_regress),
    T(unbounded_props),
    T(unbounded_propstats),
    T(NULL) 
};

