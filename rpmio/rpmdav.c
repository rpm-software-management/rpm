/*@-modfilesys@*/
/** \ingroup rpmio
 * \file rpmio/rpmdav.c
 */

#include "system.h"

#if defined(HAVE_PTHREAD_H) && !defined(__LCLINT__)
#include <pthread.h>
#endif

#include <neon/ne_alloc.h>
#include <neon/ne_auth.h>
#include <neon/ne_basic.h>
#include <neon/ne_dates.h>
#include <neon/ne_locks.h>
#include <neon/ne_props.h>
#include <neon/ne_request.h>
#include <neon/ne_socket.h>
#include <neon/ne_string.h>
#include <neon/ne_utils.h>

#include <rpmio_internal.h>

#define _RPMDAV_INTERNAL
#include <rpmdav.h>
                                                                                
#include "argv.h"
#include "ugid.h"
#include "debug.h"

/*@access DIR @*/
/*@access FD_t @*/
/*@access urlinfo @*/

#if 0	/* HACK: reasonable value needed. */
#define TIMEOUT_SECS 60
#else
#define TIMEOUT_SECS 5
#endif
/*@unchecked@*/
static int httpTimeoutSecs = TIMEOUT_SECS;

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @retval		NULL always
 */
/*@unused@*/ static inline /*@null@*/ void *
_free(/*@only@*/ /*@null@*/ /*@out@*/ const void * p)
	/*@modifies p@*/
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

/* =============================================================== */
static int davFree(urlinfo u)
	/*@globals internalState @*/
	/*@modifies u, internalState @*/
{
    if (u != NULL && u->sess != NULL) {
	u->capabilities = _free(u->capabilities);
	if (u->lockstore != NULL)
	    ne_lockstore_destroy(u->lockstore);
	u->lockstore = NULL;
	ne_session_destroy(u->sess);
	u->sess = NULL;
    }
    return 0;
}

static void davProgress(void * userdata, off_t current, off_t total)
	/*@*/
{
    urlinfo u = userdata;
    ne_session * sess;

assert(u != NULL);
    sess = u->sess;
assert(sess != NULL);
assert(u == ne_get_session_private(sess, "urlinfo"));

    u->current = current;
    u->total = total;

if (_dav_debug < 0)
fprintf(stderr, "*** davProgress(%p,0x%x:0x%x) sess %p u %p\n", userdata, (unsigned int)current, (unsigned int)total, sess, u);
}

static void davNotify(void * userdata,
		ne_conn_status connstatus, const char * info)
	/*@*/
{
    urlinfo u = userdata;
    ne_session * sess;
    /*@observer@*/
    static const char * connstates[] = {
	"namelookup",
	"connecting",
	"connected",
	"secure",
	"unknown"
    };

assert(u != NULL);
    sess = u->sess;
assert(sess != NULL);
assert(u == ne_get_session_private(sess, "urlinfo"));

#ifdef	REFERENCE
typedef enum {
    ne_conn_namelookup, /* lookup up hostname (info = hostname) */
    ne_conn_connecting, /* connecting to host (info = hostname) */
    ne_conn_connected, /* connected to host (info = hostname) */
    ne_conn_secure /* connection now secure (info = crypto level) */
} ne_conn_status;
#endif

    u->connstatus = connstatus;

/*@-boundsread@*/
if (_dav_debug < 0)
fprintf(stderr, "*** davNotify(%p,%d,%p) sess %p u %p %s\n", userdata, connstatus, info, sess, u, connstates[ (connstatus < 4 ? connstatus : 4)]);
/*@=boundsread@*/

}

static void davCreateRequest(ne_request * req, void * userdata,
		const char * method, const char * uri)
	/*@*/
{
    urlinfo u = userdata;
    ne_session * sess;
    void * private = NULL;
    const char * id = "urlinfo";

assert(u != NULL);
assert(u->sess != NULL);
assert(req != NULL);
    sess = ne_get_session(req);
assert(sess == u->sess);
assert(u == ne_get_session_private(sess, "urlinfo"));

assert(sess != NULL);
    private = ne_get_session_private(sess, id);
assert(u == private);

if (_dav_debug < 0)
fprintf(stderr, "*** davCreateRequest(%p,%p,%s,%s) %s:%p\n", req, userdata, method, uri, id, private);
}

static void davPreSend(ne_request * req, void * userdata, ne_buffer * buf)
{
    urlinfo u = userdata;
    ne_session * sess;
    const char * id = "fd";
    FD_t fd = NULL;

assert(u != NULL);
assert(u->sess != NULL);
assert(req != NULL);
    sess = ne_get_session(req);
assert(sess == u->sess);
assert(u == ne_get_session_private(sess, "urlinfo"));

    fd = ne_get_request_private(req, id);

if (_dav_debug < 0)
fprintf(stderr, "*** davPreSend(%p,%p,%p) sess %p %s %p\n", req, userdata, buf, sess, id, fd);
if (_dav_debug)
fprintf(stderr, "-> %s\n", buf->data);

}

static int davPostSend(ne_request * req, void * userdata, const ne_status * status)
	/*@*/
{
    urlinfo u = userdata;
    ne_session * sess;
    const char * id = "fd";
    FD_t fd = NULL;

assert(u != NULL);
assert(u->sess != NULL);
assert(req != NULL);
    sess = ne_get_session(req);
assert(sess == u->sess);
assert(u == ne_get_session_private(sess, "urlinfo"));

    fd = ne_get_request_private(req, id);

/*@-evalorder@*/
if (_dav_debug < 0)
fprintf(stderr, "*** davPostSend(%p,%p,%p) sess %p %s %p %s\n", req, userdata, status, sess, id, fd, ne_get_error(sess));
/*@=evalorder@*/
    return NE_OK;
}

static void davDestroyRequest(ne_request * req, void * userdata)
	/*@*/
{
    urlinfo u = userdata;
    ne_session * sess;
    const char * id = "fd";
    FD_t fd = NULL;

assert(u != NULL);
assert(u->sess != NULL);
assert(req != NULL);
    sess = ne_get_session(req);
assert(sess == u->sess);
assert(u == ne_get_session_private(sess, "urlinfo"));

    fd = ne_get_request_private(req, id);

if (_dav_debug < 0)
fprintf(stderr, "*** davDestroyRequest(%p,%p) sess %p %s %p\n", req, userdata, sess, id, fd);
}

static void davDestroySession(void * userdata)
	/*@*/
{
    urlinfo u = userdata;
    ne_session * sess;
    void * private = NULL;
    const char * id = "urlinfo";

assert(u != NULL);
assert(u->sess != NULL);
    sess = u->sess;
assert(u == ne_get_session_private(sess, "urlinfo"));

assert(sess != NULL);
    private = ne_get_session_private(sess, id);
assert(u == private);

if (_dav_debug < 0)
fprintf(stderr, "*** davDestroySession(%p) sess %p %s %p\n", userdata, sess, id, private);
}

static int
davVerifyCert(void *userdata, int failures, const ne_ssl_certificate *cert)
	/*@*/
{
    const char *hostname = userdata;

if (_dav_debug < 0)
fprintf(stderr, "*** davVerifyCert(%p,%d,%p) %s\n", userdata, failures, cert, hostname);

    return 0;	/* HACK: trust all server certificates. */
}

static int davConnect(urlinfo u)
	/*@globals internalState @*/
	/*@modifies u, internalState @*/
{
    const char * path = NULL;
    int rc;

    /* HACK: where should server capabilities be read? */
    (void) urlPath(u->url, &path);
    /* HACK: perhaps capture Allow: tag, look for PUT permitted. */
    rc = ne_options(u->sess, path, u->capabilities);
    switch (rc) {
    case NE_OK:
	break;
    case NE_ERROR:
	/* HACK: "301 Moved Permanently" on empty subdir. */
	if (!strncmp("301 ", ne_get_error(u->sess), sizeof("301 ")-1))
	    break;
	/*@fallthrough@*/
    case NE_CONNECT:
    case NE_LOOKUP:
    default:
if (_dav_debug)
fprintf(stderr, "*** Connect to %s:%d failed(%d):\n\t%s\n",
		   u->host, u->port, rc, ne_get_error(u->sess));
	break;
    }

    /* HACK: sensitive to error returns? */
    u->httpVersion = (ne_version_pre_http11(u->sess) ? 0 : 1);

    /* HACK: stupid error impedence matching. */
    if (rc)
	rc = FTPERR_FAILED_CONNECT;

    return rc;
}

static int davInit(const char * url, urlinfo * uret)
	/*@globals internalState @*/
	/*@modifies *uret, internalState @*/
{
    urlinfo u = NULL;
    int rc = 0;

/*@-globs@*/	/* FIX: h_errno annoyance. */
    if (urlSplit(url, &u))
	return -1;	/* XXX error returns needed. */
/*@=globs@*/

    if ((u->urltype == URL_IS_HTTP || u->urltype == URL_IS_HTTPS)
     && u->url != NULL && u->sess == NULL)
    {
	ne_server_capabilities * capabilities;

	/* HACK: oneshots should be done Somewhere Else Instead. */
/*@-noeffect@*/
	rc = ((_dav_debug < 0) ? NE_DBG_HTTP : 0);
	ne_debug_init(stderr, rc);		/* XXX oneshot? */
/*@=noeffect@*/
	rc = ne_sock_init();			/* XXX oneshot? */

	u->capabilities = capabilities = xcalloc(1, sizeof(*capabilities));
	u->sess = ne_session_create(u->scheme, u->host, u->port);

	u->lockstore = ne_lockstore_create();	/* XXX oneshot? */
	ne_lockstore_register(u->lockstore, u->sess);

	if (u->proxyh != NULL)
	    ne_session_proxy(u->sess, u->proxyh, u->proxyp);

#if 0
	{   const ne_inet_addr ** addrs;
	    unsigned int n;
	    ne_set_addrlist(u->sess, addrs, n);
	}
#endif

	ne_set_progress(u->sess, davProgress, u);
	ne_set_status(u->sess, davNotify, u);

	ne_set_persist(u->sess, 1);
	ne_set_read_timeout(u->sess, httpTimeoutSecs);
	ne_set_useragent(u->sess, PACKAGE "/" PACKAGE_VERSION);

	/* XXX check that neon is ssl enabled. */
	if (!strcasecmp(u->scheme, "https"))
	    ne_ssl_set_verify(u->sess, davVerifyCert, (char *)u->host);
                                                                                
	ne_set_session_private(u->sess, "urlinfo", u);

	ne_hook_destroy_session(u->sess, davDestroySession, u);

	ne_hook_create_request(u->sess, davCreateRequest, u);
	ne_hook_pre_send(u->sess, davPreSend, u);
	ne_hook_post_send(u->sess, davPostSend, u);
	ne_hook_destroy_request(u->sess, davDestroyRequest, u);

	/* HACK: where should server capabilities be read? */
	rc = davConnect(u);
	if (rc)
	    goto exit;

    }

exit:
/*@-boundswrite@*/
    if (rc == 0 && uret != NULL)
	*uret = urlLink(u, __FUNCTION__);
/*@=boundswrite@*/
    u = urlFree(u, "urlSplit (davInit)");

    return rc;
}

/* =============================================================== */
enum fetch_rtype_e {
    resr_normal = 0,
    resr_collection,
    resr_reference,
    resr_error
};
                                                                                
struct fetch_resource_s {
    struct fetch_resource_s *next;
    char *uri;
/*@unused@*/
    char *displayname;
    enum fetch_rtype_e type;
    size_t size;
    time_t modtime;
    int is_executable;
    int is_vcr;    /* Is version resource. 0: no vcr, 1 checkin 2 checkout */
    char *error_reason; /* error string returned for this resource */
    int error_status; /* error status returned for this resource */
};

/*@null@*/
static void *fetch_destroy_item(/*@only@*/ struct fetch_resource_s *res)
	/*@modifies res @*/
{
    NE_FREE(res->uri);
    NE_FREE(res->error_reason);
    res = _free(res);
    return NULL;
}

#ifdef	UNUSED
/*@null@*/
static void *fetch_destroy_list(/*@only@*/ struct fetch_resource_s *res)
	/*@modifies res @*/
{
    struct fetch_resource_s *next;
/*@-branchstate@*/
    for (; res != NULL; res = next) {
	next = res->next;
	res = fetch_destroy_item(res);
    }
/*@=branchstate@*/
    return NULL;
}
#endif

static void *fetch_create_item(/*@unused@*/ void *userdata, /*@unused@*/ const char *uri)
        /*@*/
{
    struct fetch_resource_s * res = ne_calloc(sizeof(*res));
    return res;
}

/* =============================================================== */
struct fetch_context_s {
/*@relnull@*/
    struct fetch_resource_s **resrock;
    const char *uri;
    unsigned int include_target; /* Include resource at href */
/*@refcounted@*/
    urlinfo u;
    int ac;
    int nalloced;
    ARGV_t av;
    mode_t * modes;
    size_t * sizes;
    time_t * mtimes;
};

/*@null@*/
static void *fetch_destroy_context(/*@only@*/ /*@null@*/ struct fetch_context_s *ctx)
	/*@globals internalState @*/
	/*@modifies ctx, internalState @*/
{
    if (ctx == NULL)
	return NULL;
    if (ctx->av != NULL)
	ctx->av = argvFree(ctx->av);
    ctx->modes = _free(ctx->modes);
    ctx->sizes = _free(ctx->sizes);
    ctx->mtimes = _free(ctx->mtimes);
    ctx->u = urlFree(ctx->u, __FUNCTION__);
    ctx->uri = _free(ctx->uri);
/*@-boundswrite@*/
    memset(ctx, 0, sizeof(*ctx));
/*@=boundswrite@*/
    ctx = _free(ctx);
    return NULL;
}

/*@null@*/
static void *fetch_create_context(const char *uri)
	/*@globals internalState @*/
	/*@modifies internalState @*/
{
    struct fetch_context_s * ctx;
    urlinfo u;

/*@-globs@*/	/* FIX: h_errno annoyance. */
    if (urlSplit(uri, &u))
	return NULL;
/*@=globs@*/

    ctx = ne_calloc(sizeof(*ctx));
    ctx->uri = xstrdup(uri);
    ctx->u = urlLink(u, __FUNCTION__);
    return ctx;
}

/*@unchecked@*/ /*@observer@*/
static const ne_propname fetch_props[] = {
    { "DAV:", "getcontentlength" },
    { "DAV:", "getlastmodified" },
    { "http://apache.org/dav/props/", "executable" },
    { "DAV:", "resourcetype" },
    { "DAV:", "checked-in" },
    { "DAV:", "checked-out" },
    { NULL, NULL }
};

#define ELM_resourcetype (NE_PROPS_STATE_TOP + 1)
#define ELM_collection (NE_PROPS_STATE_TOP + 2)

/*@unchecked@*/ /*@observer@*/
static const struct ne_xml_idmap fetch_idmap[] = {
    { "DAV:", "resourcetype", ELM_resourcetype },
    { "DAV:", "collection", ELM_collection }
};

static int fetch_startelm(void *userdata, int parent, 
		const char *nspace, const char *name,
		/*@unused@*/ const char **atts)
	/*@*/
{
    ne_propfind_handler *pfh = userdata;
    struct fetch_resource_s *r = ne_propfind_current_private(pfh);
    int state = ne_xml_mapid(fetch_idmap, NE_XML_MAPLEN(fetch_idmap),
                             nspace, name);

    if (r == NULL || 
        !((parent == NE_207_STATE_PROP && state == ELM_resourcetype) ||
          (parent == ELM_resourcetype && state == ELM_collection)))
        return NE_XML_DECLINE;

    if (state == ELM_collection) {
	r->type = resr_collection;
    }

    return state;
}

static int fetch_compare(const struct fetch_resource_s *r1, 
			    const struct fetch_resource_s *r2)
	/*@*/
{
    /* Sort errors first, then collections, then alphabetically */
    if (r1->type == resr_error) {
	return -1;
    } else if (r2->type == resr_error) {
	return 1;
    } else if (r1->type == resr_collection) {
	if (r2->type != resr_collection) {
	    return -1;
	} else {
	    return strcmp(r1->uri, r2->uri);
	}
    } else {
	if (r2->type != resr_collection) {
	    return strcmp(r1->uri, r2->uri);
	} else {
	    return 1;
	}
    }
}

static void fetch_results(void *userdata, const char *uri,
		    const ne_prop_result_set *set)
	/*@*/
{
    struct fetch_context_s *ctx = userdata;
    struct fetch_resource_s *current, *previous, *newres;
    const char *clength, *modtime, *isexec;
    const char *checkin, *checkout;
    const ne_status *status = NULL;
    const char * path = NULL;

    (void) urlPath(uri, &path);
    if (path == NULL)
	return;

    newres = ne_propset_private(set);

if (_dav_debug < 0)
fprintf(stderr, "==> %s in uri %s\n", path, ctx->uri);
    
    if (ne_path_compare(ctx->uri, path) == 0 && !ctx->include_target) {
	/* This is the target URI */
if (_dav_debug < 0)
fprintf(stderr, "==> %s skipping target resource.\n", path);
	/* Free the private structure. */
	free(newres);
	return;
    }

    newres->uri = ne_strdup(path);

/*@-boundsread@*/
    clength = ne_propset_value(set, &fetch_props[0]);    
    modtime = ne_propset_value(set, &fetch_props[1]);
    isexec = ne_propset_value(set, &fetch_props[2]);
    checkin = ne_propset_value(set, &fetch_props[4]);
    checkout = ne_propset_value(set, &fetch_props[5]);
/*@=boundsread@*/
    
    if (clength == NULL)
	status = ne_propset_status(set, &fetch_props[0]);
    if (modtime == NULL)
	status = ne_propset_status(set, &fetch_props[1]);

    if (newres->type == resr_normal && status != NULL) {
	/* It's an error! */
	newres->error_status = status->code;

	/* Special hack for Apache 1.3/mod_dav */
	if (strcmp(status->reason_phrase, "status text goes here") == 0) {
	    const char *desc;
	    if (status->code == 401) {
		desc = _("Authorization Required");
	    } else if (status->klass == 3) {
		desc = _("Redirect");
	    } else if (status->klass == 5) {
		desc = _("Server Error");
	    } else {
		desc = _("Unknown Error");
	    }
	    newres->error_reason = ne_strdup(desc);
	} else {
	    newres->error_reason = ne_strdup(status->reason_phrase);
	}
	newres->type = resr_error;
    }

    if (isexec && strcasecmp(isexec, "T") == 0) {
	newres->is_executable = 1;
    } else {
	newres->is_executable = 0;
    }

    if (modtime)
	newres->modtime = ne_httpdate_parse(modtime);

    if (clength)
	newres->size = atoi(clength);

    /* is vcr */
    if (checkin) {
	newres->is_vcr = 1;
    } else if (checkout) {
	newres->is_vcr = 2;
    } else {
	newres->is_vcr = 0;
    }

    for (current = *ctx->resrock, previous = NULL; current != NULL; 
	previous = current, current = current->next)
    {
	if (fetch_compare(current, newres) >= 0) {
	    break;
	}
    }
    if (previous) {
	previous->next = newres;
    } else {
/*@-boundswrite@*/
	*ctx->resrock = newres;
/*@=boundswrite@*/
    }
    newres->next = current;
}

static int davFetch(const urlinfo u, struct fetch_context_s * ctx)
	/*@globals internalState @*/
	/*@modifies ctx, internalState @*/
{
    const char * path = NULL;
    int depth = 1;					/* XXX passed arg? */
    unsigned int include_target = 0;			/* XXX passed arg? */
    struct fetch_resource_s * resitem = NULL;
    struct fetch_resource_s ** resrock = &resitem;	/* XXX passed arg? */
    ne_propfind_handler *pfh;
    struct fetch_resource_s *current, *next;
    mode_t st_mode;
    int rc = 0;
    int xx;

    (void) urlPath(u->url, &path);
    pfh = ne_propfind_create(u->sess, ctx->uri, depth);

    /* HACK: need to set u->httpHasRange here. */

    ctx->resrock = resrock;
    ctx->include_target = include_target;

    ne_xml_push_handler(ne_propfind_get_parser(pfh), 
                        fetch_startelm, NULL, NULL, pfh);

    ne_propfind_set_private(pfh, fetch_create_item, NULL);

    rc = ne_propfind_named(pfh, fetch_props, fetch_results, ctx);

    ne_propfind_destroy(pfh);

    for (current = resitem; current != NULL; current = next) {
	const char *s, *se;
	char * val;

	next = current->next;

	/* Collections have trailing '/' that needs trim. */
	/* The top level collection is returned as well. */
	se = current->uri + strlen(current->uri);
	if (se[-1] == '/') {
	    if (strlen(current->uri) <= strlen(path)) {
		current = fetch_destroy_item(current);
		continue;
	    }
	    se--;
	}
	s = se;
	while (s > current->uri && s[-1] != '/')
	    s--;

	val = ne_strndup(s, (se - s));

/*@-nullpass@*/
	val = ne_path_unescape(val);
/*@=nullpass@*/

	xx = argvAdd(&ctx->av, val);
if (_dav_debug < 0)
fprintf(stderr, "*** argvAdd(%p,\"%s\")\n", &ctx->av, val);
	NE_FREE(val);

	while (ctx->ac >= ctx->nalloced) {
	    if (ctx->nalloced <= 0)
		ctx->nalloced = 1;
	    ctx->nalloced *= 2;
	    ctx->modes = xrealloc(ctx->modes,
				(sizeof(*ctx->modes) * ctx->nalloced));
	    ctx->sizes = xrealloc(ctx->sizes,
				(sizeof(*ctx->sizes) * ctx->nalloced));
	    ctx->mtimes = xrealloc(ctx->mtimes,
				(sizeof(*ctx->mtimes) * ctx->nalloced));
	}

	switch (current->type) {
	case resr_normal:
	    st_mode = S_IFREG;
	    /*@switchbreak@*/ break;
	case resr_collection:
	    st_mode = S_IFDIR;
	    /*@switchbreak@*/ break;
	case resr_reference:
	case resr_error:
	default:
	    st_mode = 0;
	    /*@switchbreak@*/ break;
	}
/*@-boundswrite@*/
	ctx->modes[ctx->ac] = st_mode;
	ctx->sizes[ctx->ac] = current->size;
	ctx->mtimes[ctx->ac] = current->modtime;
/*@=boundswrite@*/
	ctx->ac++;

	current = fetch_destroy_item(current);
    }
    ctx->resrock = NULL;	/* HACK: avoid leaving stack reference. */

    return rc;
}

static int davNLST(struct fetch_context_s * ctx)
	/*@globals internalState @*/
	/*@modifies ctx, internalState @*/
{
    urlinfo u = NULL;
    int rc;
    int xx;

    rc = davInit(ctx->uri, &u);
    if (rc || u == NULL)
	goto exit;

    rc = davFetch(u, ctx);
    switch (rc) {
    case NE_OK:
        break;
    case NE_ERROR:
	/* HACK: "301 Moved Permanently" on empty subdir. */
	if (!strncmp("301 ", ne_get_error(u->sess), sizeof("301 ")-1))
	    break;
	/*@fallthrough@*/
    default:
if (_dav_debug)
fprintf(stderr, "*** Fetch from %s:%d failed:\n\t%s\n",
		   u->host, u->port, ne_get_error(u->sess));
        break;
    }

exit:
    if (rc)
	xx = davFree(u);
    return rc;
}

/* =============================================================== */
/*@observer@*/
static const char * my_retstrerror(int ret)
	/*@*/
{
    const char * str = "NE_UNKNOWN";
    switch (ret) {
    case NE_OK:		str = "NE_OK: Succeeded.";			break;
    case NE_ERROR:	str = "NE_ERROR: Generic error.";		break;
    case NE_LOOKUP:	str = "NE_LOOKUP: Hostname lookup failed.";	break;
    case NE_AUTH:	str = "NE_AUTH: Server authentication failed.";	break;
    case NE_PROXYAUTH:	str = "NE_PROXYAUTH: Proxy authentication failed.";break;
    case NE_CONNECT:	str = "NE_CONNECT: Could not connect to server.";break;
    case NE_TIMEOUT:	str = "NE_TIMEOUT: Connection timed out.";	break;
    case NE_FAILED:	str = "NE_FAILED: The precondition failed.";	break;
    case NE_RETRY:	str = "NE_RETRY: Retry request.";		break;
    case NE_REDIRECT:	str = "NE_REDIRECT: Redirect received.";	break;
    default:		str = "NE_UNKNOWN";				break;
    }
    return str;
}

static int my_result(const char * msg, int ret, /*@null@*/ FILE * fp)
	/*@modifies *fp @*/
{
    /* HACK: don't print unless debugging. */
    if (_dav_debug >= 0)
	return ret;
    if (fp == NULL)
	fp = stderr;
    if (msg != NULL)
	fprintf(fp, "*** %s: ", msg);
#ifdef	HACK
    fprintf(fp, "%s: %s\n", my_retstrerror(ret), ne_get_error(sess));
#else
    fprintf(fp, "%s\n", my_retstrerror(ret));
#endif
    return ret;
}

#ifdef	DYING
static void hexdump(const unsigned char * buf, ssize_t len)
	/*@*/
{
    int i;
    if (len <= 0)
	return;
    for (i = 0; i < len; i++) {
	if (i != 0 && (i%16) == 0)
	    fprintf(stderr, "\n");
	fprintf(stderr, " %02X", buf[i]);
    }
    fprintf(stderr, "\n");
}
#endif

static void davAcceptRanges(void * userdata, const char * value)
{
    urlinfo u = userdata;

if (_dav_debug < 0)
fprintf(stderr, "*** u %p Accept-Ranges: %s\n", u, value);
    if (!strcmp(value, "bytes"))
	u->httpHasRange = 1;
    if (!strcmp(value, "none"))
	u->httpHasRange = 0;
}

static void davAllHeaders(void * userdata, const char * value)
{
    FD_t ctrl = userdata;

if (_dav_debug)
fprintf(stderr, "<- %s\n", value);
}

static void davContentLength(void * userdata, const char * value)
{
    FD_t ctrl = userdata;

if (_dav_debug < 0)
fprintf(stderr, "*** fd %p Content-Length: %s\n", ctrl, value);
/*@-unrecog@*/
   ctrl->contentLength = strtoll(value, NULL, 10);
/*@=unrecog@*/
}

static void davConnection(void * userdata, const char * value)
{
    FD_t ctrl = userdata;

if (_dav_debug < 0)
fprintf(stderr, "*** fd %p Connection: %s\n", ctrl, value);
    if (!strcasecmp(value, "close"))
	ctrl->persist = 0;
    else if (!strcasecmp(value, "Keep-Alive"))
	ctrl->persist = 1;
}

/*@-mustmod@*/ /* HACK: stash error in *str. */
int davResp(urlinfo u, FD_t ctrl, /*@unused@*/ char *const * str)
{
    int rc = 0;

    rc = ne_begin_request(ctrl->req);
    rc = my_result("ne_begin_req(ctrl->req)", rc, NULL);

if (_dav_debug < 0)
fprintf(stderr, "*** davResp(%p,%p,%p) sess %p req %p rc %d\n", u, ctrl, str, u->sess, ctrl->req, rc);

    /* HACK: stupid error impedence matching. */
    /* HACK: NE_TIMEOUT et al here does not unravel refcnt correctly. */
    switch (rc) {
    case NE_OK:		rc = 0;				break;
    case NE_ERROR:	rc = FTPERR_SERVER_IO_ERROR;	break;
    case NE_LOOKUP:	rc = FTPERR_BAD_HOSTNAME;	break;
    case NE_AUTH:	rc = FTPERR_FAILED_CONNECT;	break;
    case NE_PROXYAUTH:	rc = FTPERR_FAILED_CONNECT;	break;
    case NE_CONNECT:	rc = FTPERR_FAILED_CONNECT;	break;
    case NE_TIMEOUT:	rc = FTPERR_SERVER_TIMEOUT;	break;
    case NE_FAILED:	rc = FTPERR_SERVER_IO_ERROR;	break;
    case NE_RETRY:	/* HACK: davReq handles. */	break;
    case NE_REDIRECT:	rc = FTPERR_BAD_SERVER_RESPONSE;break;
    default:		rc = FTPERR_UNKNOWN;		break;
    }

/*@-observertrans@*/
    if (rc)
	fdSetSyserrno(ctrl, errno, ftpStrerror(rc));
/*@=observertrans@*/

    return rc;
}
/*@=mustmod@*/

int davReq(FD_t ctrl, const char * httpCmd, const char * httpArg)
{
    urlinfo u;
    int rc = 0;

assert(ctrl != NULL);
    u = ctrl->url;
    URLSANE(u);

if (_dav_debug < 0)
fprintf(stderr, "*** davReq(%p,%s,\"%s\") entry sess %p req %p\n", ctrl, httpCmd, (httpArg ? httpArg : ""), u->sess, ctrl->req);

    ctrl->persist = (u->httpVersion > 0 ? 1 : 0);
    ctrl = fdLink(ctrl, "open ctrl (davReq)");

assert(u->sess != NULL);
assert(ctrl->req == NULL);
/*@-nullpass@*/
    ctrl->req = ne_request_create(u->sess, httpCmd, httpArg);
/*@=nullpass@*/
assert(ctrl->req != NULL);

    ne_set_request_private(ctrl->req, "fd", ctrl);

    ne_add_response_header_catcher(ctrl->req, davAllHeaders, ctrl);

    ne_add_response_header_handler(ctrl->req, "Content-Length",
		davContentLength, ctrl);
    ne_add_response_header_handler(ctrl->req, "Connection",
		davConnection, ctrl);

    if (!strcmp(httpCmd, "PUT")) {
#ifdef	NOTYET		/* XXX HACK no wr_chunked until libneon supports */
	ctrl->wr_chunked = 1;
	ne_add_request_header(ctrl->req, "Transfer-Encoding", "chunked");
	ne_set_request_chunked(ctrl->req, 1);
	/* HACK: no retries if/when chunking. */
	rc = davResp(u, ctrl, NULL);
#else
	rc = FTPERR_SERVER_IO_ERROR;
#endif
    } else {
	/* HACK: possible Last-Modified: Tue, 02 Nov 2004 14:29:36 GMT */
	/* HACK: possible ETag: "inode-size-mtime" */
	ne_add_response_header_handler(ctrl->req, "Accept-Ranges",
			davAcceptRanges, u);
	/* HACK: possible Transfer-Encoding: on GET. */

	/* HACK: other errors may need retry too. */
	/* HACK: neon retries once, gud enuf. */
	do {
	    rc = davResp(u, ctrl, NULL);
	} while (rc == NE_RETRY);
    }
    if (rc)
	goto errxit;

if (_dav_debug < 0)
fprintf(stderr, "*** davReq(%p,%s,\"%s\") exit sess %p req %p rc %d\n", ctrl, httpCmd, (httpArg ? httpArg : ""), u->sess, ctrl->req, rc);

    ctrl = fdLink(ctrl, "open data (davReq)");
    return 0;

errxit:
/*@-observertrans@*/
    fdSetSyserrno(ctrl, errno, ftpStrerror(rc));
/*@=observertrans@*/

    return rc;
}

FD_t davOpen(const char * url, /*@unused@*/ int flags,
		/*@unused@*/ mode_t mode, /*@out@*/ urlinfo * uret)
{
    const char * path = NULL;
    urltype urlType = urlPath(url, &path);
    urlinfo u = NULL;
    FD_t fd = NULL;
    int rc;

#if 0	/* XXX makeTempFile() heartburn */
    assert(!(flags & O_RDWR));
#endif

if (_dav_debug < 0)
fprintf(stderr, "*** davOpen(%s,0x%x,0%o,%p)\n", url, flags, mode, uret);
    rc = davInit(url, &u);
    if (rc || u == NULL || u->sess == NULL)
	goto exit;

    if (u->ctrl == NULL)
	u->ctrl = fdNew("persist ctrl (davOpen)");
    if (u->ctrl->nrefs > 2 && u->data == NULL)
	u->data = fdNew("persist data (davOpen)");

    if (u->ctrl->url == NULL)
	fd = fdLink(u->ctrl, "grab ctrl (davOpen persist ctrl)");
    else if (u->data->url == NULL)
	fd = fdLink(u->data, "grab ctrl (davOpen persist data)");
    else
	fd = fdNew("grab ctrl (davOpen)");

    if (fd) {
	fdSetIo(fd, ufdio);
	fd->ftpFileDoneNeeded = 0;
	fd->rd_timeoutsecs = httpTimeoutSecs;
	fd->contentLength = fd->bytesRemain = -1;
	fd->url = urlLink(u, "url (davOpen)");
	fd = fdLink(fd, "grab data (davOpen)");
assert(urlType == URL_IS_HTTPS || urlType == URL_IS_HTTP);
	fd->urlType = urlType;	/* URL_IS_HTTPS */
    }

exit:
/*@-boundswrite@*/
    if (uret)
	*uret = u;
/*@=boundswrite@*/
    /*@-refcounttrans@*/
    return fd;
    /*@=refcounttrans@*/
}

ssize_t davRead(void * cookie, /*@out@*/ char * buf, size_t count)
{
    FD_t fd = cookie;
    ssize_t rc;

#if 0
assert(count >= 128);	/* HACK: see ne_request.h comment */
#endif
    rc = ne_read_response_block(fd->req, buf, count);

if (_dav_debug < 0) {
fprintf(stderr, "*** davRead(%p,%p,0x%x) rc 0x%x\n", cookie, buf, count, (unsigned)rc);
#ifdef	DYING
hexdump(buf, rc);
#endif
    }

    return rc;
}

ssize_t davWrite(void * cookie, const char * buf, size_t count)
{
#ifdef	NOTYET		/* XXX HACK no wr_chunked until libneon supports */
    FD_t fd = cookie;
    ssize_t rc;
    int xx;

assert(fd->req != NULL);
    xx = ne_send_request_chunk(fd->req, buf, count);

    /* HACK: stupid error impedence matching. */
    rc = (xx == 0 ? count : -1);

if (_dav_debug < 0)
fprintf(stderr, "*** davWrite(%p,%p,0x%x) rc 0x%x\n", cookie, buf, count, rc);
#ifdef	DYING
if (count > 0)
hexdump(buf, count);
#endif

    return rc;
#else
    errno = EIO;	/* HACK */
    return -1;
#endif
}

int davSeek(void * cookie, /*@unused@*/ _libio_pos_t pos, int whence)
{
if (_dav_debug < 0)
fprintf(stderr, "*** davSeek(%p,pos,%d)\n", cookie, whence);
    return -1;
}

/*@-mustmod@*/	/* HACK: fd->req is modified. */
int davClose(void * cookie)
{
/*@-onlytrans@*/
    FD_t fd = cookie;
/*@=onlytrans@*/
    int rc;

assert(fd->req != NULL);
    rc = ne_end_request(fd->req);
    rc = my_result("ne_end_request(req)", rc, NULL);

    ne_request_destroy(fd->req);
    fd->req = NULL;

if (_dav_debug < 0)
fprintf(stderr, "*** davClose(%p) rc %d\n", fd, rc);
    return rc;
}
/*@=mustmod@*/

/* =============================================================== */
int davMkdir(const char * path, mode_t mode)
{
    urlinfo u = NULL;
    const char * src = NULL;
    int rc;

    rc = davInit(path, &u);
    if (rc)
	goto exit;

    (void) urlPath(path, &src);

    rc = ne_mkcol(u->sess, path);

    if (rc) rc = -1;	/* XXX HACK: errno impedance match */

    /* XXX HACK: verify getrestype(remote) == resr_collection */

exit:
if (_dav_debug)
fprintf(stderr, "*** davMkdir(%s,0%o) rc %d\n", path, mode, rc);
    return rc;
}

int davRmdir(const char * path)
{
    urlinfo u = NULL;
    const char * src = NULL;
    int rc;

    rc = davInit(path, &u);
    if (rc)
	goto exit;

    (void) urlPath(path, &src);

    /* XXX HACK: only getrestype(remote) == resr_collection */

    rc = ne_delete(u->sess, path);

    if (rc) rc = -1;	/* XXX HACK: errno impedance match */

exit:
if (_dav_debug)
fprintf(stderr, "*** davRmdir(%s) rc %d\n", path, rc);
    return rc;
}

int davRename(const char * oldpath, const char * newpath)
{
    urlinfo u = NULL;
    const char * src = NULL;
    const char * dst = NULL;
    int overwrite = 1;		/* HACK: set this correctly. */
    int rc;

    rc = davInit(oldpath, &u);
    if (rc)
	goto exit;

    (void) urlPath(oldpath, &src);
    (void) urlPath(newpath, &dst);

    /* XXX HACK: only getrestype(remote) != resr_collection */

    rc = ne_move(u->sess, overwrite, src, dst);

    if (rc) rc = -1;	/* XXX HACK: errno impedance match */

exit:
if (_dav_debug)
fprintf(stderr, "*** davRename(%s,%s) rc %d\n", oldpath, newpath, rc);
    return rc;
}

int davUnlink(const char * path)
{
    urlinfo u = NULL;
    const char * src = NULL;
    int rc;

    rc = davInit(path, &u);
    if (rc)
	goto exit;

    (void) urlPath(path, &src);

    /* XXX HACK: only getrestype(remote) != resr_collection */

    rc = ne_delete(u->sess, src);

exit:
    if (rc) rc = -1;	/* XXX HACK: errno impedance match */

if (_dav_debug)
fprintf(stderr, "*** davUnlink(%s) rc %d\n", path, rc);
    return rc;
}

#ifdef	NOTYET
static int davChdir(const char * path)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies fileSystem, internalState @*/
{
    return davCommand("CWD", path, NULL);
}
#endif	/* NOTYET */

/* =============================================================== */

static const char * statstr(const struct stat * st,
		/*@returned@*/ /*@out@*/ char * buf)
	/*@modifies *buf @*/
{
    sprintf(buf,
	"*** dev %x ino %x mode %0o nlink %d uid %d gid %d rdev %x size %x\n",
	(unsigned)st->st_dev,
	(unsigned)st->st_ino,
	st->st_mode,
	(unsigned)st->st_nlink,
	st->st_uid,
	st->st_gid,
	(unsigned)st->st_rdev,
	(unsigned)st->st_size);
    return buf;
}

/*@unchecked@*/
static int dav_st_ino = 0xdead0000;

/*@-boundswrite@*/
int davStat(const char * path, /*@out@*/ struct stat *st)
	/*@globals dav_st_ino, fileSystem, internalState @*/
	/*@modifies *st, dav_st_ino, fileSystem, internalState @*/
{
    struct fetch_context_s * ctx = NULL;
    char buf[1024];
    int rc = -1;

/* HACK: neon really wants collections with trailing '/' */
    ctx = fetch_create_context(path);
    if (ctx == NULL) {
/* HACK: errno = ??? */
	goto exit;
    }
    rc = davNLST(ctx);
    if (rc) {
/* HACK: errno = ??? */
	goto exit;
    }

    memset(st, 0, sizeof(*st));
    st->st_mode = ctx->modes[0];
    st->st_size = ctx->sizes[0];
    st->st_mtime = ctx->mtimes[0];
    if (S_ISDIR(st->st_mode)) {
	st->st_nlink = 2;
	st->st_mode |= 0755;
    } else
    if (S_ISREG(st->st_mode)) {
	st->st_nlink = 1;
	st->st_mode |= 0644;
    } 

    /* XXX fts(3) needs/uses st_ino, make something up for now. */
    if (st->st_ino == 0)
	st->st_ino = dav_st_ino++;
if (_dav_debug < 0)
fprintf(stderr, "*** davStat(%s) rc %d\n%s", path, rc, statstr(st, buf));
exit:
    ctx = fetch_destroy_context(ctx);
    return rc;
}
/*@=boundswrite@*/

/*@-boundswrite@*/
int davLstat(const char * path, /*@out@*/ struct stat *st)
	/*@globals dav_st_ino, fileSystem, internalState @*/
	/*@modifies *st, dav_st_ino, fileSystem, internalState @*/
{
    struct fetch_context_s * ctx = NULL;
    char buf[1024];
    int rc = -1;

/* HACK: neon really wants collections with trailing '/' */
    ctx = fetch_create_context(path);
    if (ctx == NULL) {
/* HACK: errno = ??? */
	goto exit;
    }
    rc = davNLST(ctx);
    if (rc) {
/* HACK: errno = ??? */
	goto exit;
    }

    memset(st, 0, sizeof(*st));
    st->st_mode = ctx->modes[0];
    st->st_size = ctx->sizes[0];
    st->st_mtime = ctx->mtimes[0];
    if (S_ISDIR(st->st_mode)) {
	st->st_nlink = 2;
	st->st_mode |= 0755;
    } else
    if (S_ISREG(st->st_mode)) {
	st->st_nlink = 1;
	st->st_mode |= 0644;
    } 

    /* XXX fts(3) needs/uses st_ino, make something up for now. */
    if (st->st_ino == 0)
	st->st_ino = dav_st_ino++;
if (_dav_debug < 0)
fprintf(stderr, "*** davLstat(%s) rc %d\n%s\n", path, rc, statstr(st, buf));
exit:
    ctx = fetch_destroy_context(ctx);
    return rc;
}
/*@=boundswrite@*/

#ifdef	NOTYET
static int davReadlink(const char * path, /*@out@*/ char * buf, size_t bufsiz)
	/*@globals h_errno, fileSystem, internalState @*/
	/*@modifies *buf, fileSystem, internalState @*/
{
    int rc;
    rc = davNLST(path, DO_FTP_READLINK, NULL, buf, bufsiz);
if (_dav_debug < 0)
fprintf(stderr, "*** davReadlink(%s) rc %d\n", path, rc);
    return rc;
}
#endif	/* NOTYET */

/* =============================================================== */
/*@unchecked@*/
int avmagicdir = 0x3607113;

int avClosedir(/*@only@*/ DIR * dir)
{
    AVDIR avdir = (AVDIR)dir;

if (_av_debug)
fprintf(stderr, "*** avClosedir(%p)\n", avdir);

#if defined(HAVE_PTHREAD_H)
/*@-moduncon -noeffectuncon @*/
    (void) pthread_mutex_destroy(&avdir->lock);
/*@=moduncon =noeffectuncon @*/
#endif

    avdir = _free(avdir);
    return 0;
}

struct dirent * avReaddir(DIR * dir)
{
    AVDIR avdir = (AVDIR)dir;
    struct dirent * dp;
    const char ** av;
    unsigned char * dt;
    int ac;
    int i;

    if (avdir == NULL || !ISAVMAGIC(avdir) || avdir->data == NULL) {
	/* XXX TODO: EBADF errno. */
	return NULL;
    }

    dp = (struct dirent *) avdir->data;
    av = (const char **) (dp + 1);
    ac = avdir->size;
    dt = (char *) (av + (ac + 1));
    i = avdir->offset + 1;

/*@-boundsread@*/
    if (i < 0 || i >= ac || av[i] == NULL)
	return NULL;
/*@=boundsread@*/

    avdir->offset = i;

    /* XXX glob(3) uses REAL_DIR_ENTRY(dp) test on d_ino */
/*@-type@*/
    dp->d_ino = i + 1;		/* W2DO? */
    dp->d_reclen = 0;		/* W2DO? */

#if !defined(hpux) && !defined(sun)
    dp->d_off = 0;		/* W2DO? */
/*@-boundsread@*/
    dp->d_type = dt[i];
/*@=boundsread@*/
#endif
/*@=type@*/

    strncpy(dp->d_name, av[i], sizeof(dp->d_name));
if (_av_debug)
fprintf(stderr, "*** avReaddir(%p) %p \"%s\"\n", (void *)avdir, dp, dp->d_name);
    
    return dp;
}

/*@-boundswrite@*/
DIR * avOpendir(const char * path)
{
    AVDIR avdir;
    struct dirent * dp;
    size_t nb = 0;
    const char ** av;
    unsigned char * dt;
    char * t;
    int ac;

if (_av_debug)
fprintf(stderr, "*** avOpendir(%s)\n", path);
    nb = sizeof(".") + sizeof("..");
    ac = 2;

    nb += sizeof(*avdir) + sizeof(*dp) + ((ac + 1) * sizeof(*av)) + (ac + 1);
    avdir = xcalloc(1, nb);
/*@-abstract@*/
    dp = (struct dirent *) (avdir + 1);
    av = (const char **) (dp + 1);
    dt = (char *) (av + (ac + 1));
    t = (char *) (dt + ac + 1);
/*@=abstract@*/

    avdir->fd = avmagicdir;
/*@-usereleased@*/
    avdir->data = (char *) dp;
/*@=usereleased@*/
    avdir->allocation = nb;
    avdir->size = ac;
    avdir->offset = -1;
    avdir->filepos = 0;

#if defined(HAVE_PTHREAD_H)
/*@-moduncon -noeffectuncon -nullpass @*/
    (void) pthread_mutex_init(&avdir->lock, NULL);
/*@=moduncon =noeffectuncon =nullpass @*/
#endif

    ac = 0;
    /*@-dependenttrans -unrecog@*/
    dt[ac] = DT_DIR;	av[ac++] = t;	t = stpcpy(t, ".");	t++;
    dt[ac] = DT_DIR;	av[ac++] = t;	t = stpcpy(t, "..");	t++;
    /*@=dependenttrans =unrecog@*/

    av[ac] = NULL;

/*@-kepttrans@*/
    return (DIR *) avdir;
/*@=kepttrans@*/
}
/*@=boundswrite@*/

/* =============================================================== */
/*@unchecked@*/
int davmagicdir = 0x8440291;

int davClosedir(/*@only@*/ DIR * dir)
{
    DAVDIR avdir = (DAVDIR)dir;

if (_dav_debug < 0)
fprintf(stderr, "*** davClosedir(%p)\n", avdir);

#if defined(HAVE_PTHREAD_H)
/*@-moduncon -noeffectuncon @*/
    (void) pthread_mutex_destroy(&avdir->lock);
/*@=moduncon =noeffectuncon @*/
#endif

    avdir = _free(avdir);
    return 0;
}

struct dirent * davReaddir(DIR * dir)
{
    DAVDIR avdir = (DAVDIR)dir;
    struct dirent * dp;
    const char ** av;
    unsigned char * dt;
    int ac;
    int i;

    if (avdir == NULL || !ISDAVMAGIC(avdir) || avdir->data == NULL) {
	/* XXX TODO: EBADF errno. */
	return NULL;
    }

    dp = (struct dirent *) avdir->data;
    av = (const char **) (dp + 1);
    ac = avdir->size;
    dt = (char *) (av + (ac + 1));
    i = avdir->offset + 1;

/*@-boundsread@*/
    if (i < 0 || i >= ac || av[i] == NULL)
	return NULL;
/*@=boundsread@*/

    avdir->offset = i;

    /* XXX glob(3) uses REAL_DIR_ENTRY(dp) test on d_ino */
/*@-type@*/
    dp->d_ino = i + 1;		/* W2DO? */
    dp->d_reclen = 0;		/* W2DO? */

#if !defined(hpux) && !defined(sun)
    dp->d_off = 0;		/* W2DO? */
/*@-boundsread@*/
    dp->d_type = dt[i];
/*@=boundsread@*/
#endif
/*@=type@*/

    strncpy(dp->d_name, av[i], sizeof(dp->d_name));
if (_dav_debug < 0)
fprintf(stderr, "*** davReaddir(%p) %p \"%s\"\n", (void *)avdir, dp, dp->d_name);
    
    return dp;
}

/*@-boundswrite@*/
DIR * davOpendir(const char * path)
{
    struct fetch_context_s * ctx;
    DAVDIR avdir;
    struct dirent * dp;
    size_t nb;
    const char ** av, ** nav;
    unsigned char * dt;
    char * t;
    int ac, nac;
    int rc;

    /* HACK: glob does not pass dirs with trailing '/' */
    nb = strlen(path)+1;
    if (path[nb-1] != '/') {
	char * t = alloca(nb+1);
	*t = '\0';
	(void) stpcpy( stpcpy(t, path), "/");
	path = t;
    }

if (_dav_debug < 0)
fprintf(stderr, "*** davOpendir(%s)\n", path);

    /* Load DAV collection into argv. */
    ctx = fetch_create_context(path);
    if (ctx == NULL) {
/* HACK: errno = ??? */
	return NULL;
    }
    rc = davNLST(ctx);
    if (rc) {
/* HACK: errno = ??? */
	return NULL;
    }

    nb = 0;
    ac = 0;
    av = ctx->av;
    if (av != NULL)
    while (av[ac] != NULL)
	nb += strlen(av[ac++]) + 1;
    ac += 2;	/* for "." and ".." */
    nb += sizeof(".") + sizeof("..");

    nb += sizeof(*avdir) + sizeof(*dp) + ((ac + 1) * sizeof(*av)) + (ac + 1);
    avdir = xcalloc(1, nb);
    /*@-abstract@*/
    dp = (struct dirent *) (avdir + 1);
    nav = (const char **) (dp + 1);
    dt = (char *) (nav + (ac + 1));
    t = (char *) (dt + ac + 1);
    /*@=abstract@*/

    avdir->fd = davmagicdir;
/*@-usereleased@*/
    avdir->data = (char *) dp;
/*@=usereleased@*/
    avdir->allocation = nb;
    avdir->size = ac;
    avdir->offset = -1;
    avdir->filepos = 0;

#if defined(HAVE_PTHREAD_H)
/*@-moduncon -noeffectuncon -nullpass @*/
    (void) pthread_mutex_init(&avdir->lock, NULL);
/*@=moduncon =noeffectuncon =nullpass @*/
#endif

    nac = 0;
/*@-dependenttrans -unrecog@*/
    dt[nac] = DT_DIR;	nav[nac++] = t;	t = stpcpy(t, ".");	t++;
    dt[nac] = DT_DIR;	nav[nac++] = t;	t = stpcpy(t, "..");	t++;
/*@=dependenttrans =unrecog@*/

    /* Copy DAV items into DIR elments. */
    ac = 0;
    if (av != NULL)
    while (av[ac] != NULL) {
	nav[nac] = t;
	dt[nac] = (S_ISDIR(ctx->modes[ac]) ? DT_DIR : DT_REG);
	t = stpcpy(t, av[ac]);
	ac++;
	t++;
	nac++;
    }
    nav[nac] = NULL;

    ctx = fetch_destroy_context(ctx);

/*@-kepttrans@*/
    return (DIR *) avdir;
/*@=kepttrans@*/
}
/*@=modfilesys@*/
