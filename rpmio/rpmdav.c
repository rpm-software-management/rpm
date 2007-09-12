/** \ingroup rpmio
 * \file rpmio/rpmdav.c
 */

#include "system.h"

#if defined(HAVE_PTHREAD_H)
#include <pthread.h>
#endif

#ifdef WITH_NEON

#include "neon/ne_alloc.h"
#include "neon/ne_auth.h"
#include "neon/ne_basic.h"
#include "neon/ne_dates.h"
#include "neon/ne_locks.h"

#include "neon/ne_props.h"
#include "neon/ne_request.h"
#include "neon/ne_socket.h"
#include "neon/ne_string.h"
#include "neon/ne_utils.h"

/* XXX don't assume access to neon internal headers, ugh.. */
#define NEONBLOWSCHUNKS

#endif /* WITH_NEON */

#include <rpmio_internal.h>

#define _RPMDAV_INTERNAL
#include <rpmdav.h>
                                                                                
#include "argv.h"
#include "ugid.h"
#include "debug.h"


#if 0	/* HACK: reasonable value needed. */
#define TIMEOUT_SECS 60
#else
#define TIMEOUT_SECS 5
#endif

#ifdef WITH_NEON
static int httpTimeoutSecs = TIMEOUT_SECS;
#endif

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @retval		NULL always
 */
static inline void *
_free(const void * p)
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

#ifdef WITH_NEON

/* =============================================================== */

static void davProgress(void * userdata, off_t current, off_t total)
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
{
    urlinfo u = userdata;
    ne_session * sess;
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

if (_dav_debug < 0)
fprintf(stderr, "*** davNotify(%p,%d,%p) sess %p u %p %s\n", userdata, connstatus, info, sess, u, connstates[ (connstatus < 4 ? connstatus : 4)]);

}

static void davCreateRequest(ne_request * req, void * userdata,
		const char * method, const char * uri)
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
fprintf(stderr, "*** davPostSend(%p,%p,%p) sess %p %s %p %s\n", req, userdata, status, sess, id, fd, ne_get_error(sess));
    return NE_OK;
}

static void davDestroyRequest(ne_request * req, void * userdata)
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
{
    const char *hostname = userdata;

if (_dav_debug < 0)
fprintf(stderr, "*** davVerifyCert(%p,%d,%p) %s\n", userdata, failures, cert, hostname);

    return 0;	/* HACK: trust all server certificates. */
}

static int davConnect(urlinfo u)
{
    const char * path = NULL;
    int rc;

    /* HACK: hkp:// has no steenkin' options */
    if (!(u->urltype == URL_IS_HTTP || u->urltype == URL_IS_HTTPS))
	return 0;

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
    case NE_CONNECT:
    case NE_LOOKUP:
    default:
if (_dav_debug)
fprintf(stderr, "*** Connect to %s:%d failed(%d):\n\t%s\n",
		   u->host, u->port, rc, ne_get_error(u->sess));
	u = urlLink(u, __FUNCTION__);   /* XXX error exit refcount adjustment */
	break;
    }

    /* HACK: sensitive to error returns? */
    u->httpVersion = (ne_version_pre_http11(u->sess) ? 0 : 1);

    return rc;
}

static int davInit(const char * url, urlinfo * uret)
{
    urlinfo u = NULL;
    int rc = 0;

    if (urlSplit(url, &u))
	return -1;	/* XXX error returns needed. */

    if (u->url != NULL && u->sess == NULL)
    switch (u->urltype) {
    default:
	assert(u->urltype != u->urltype);
	break;
    case URL_IS_HTTPS:
    case URL_IS_HTTP:
    case URL_IS_HKP:
      {	ne_server_capabilities * capabilities;

	/* HACK: oneshots should be done Somewhere Else Instead. */
	rc = ((_dav_debug < 0) ? NE_DBG_HTTP : 0);
	ne_debug_init(stderr, rc);		/* XXX oneshot? */
	rc = ne_sock_init();			/* XXX oneshot? */

	u->lockstore = ne_lockstore_create();	/* XXX oneshot? */

	u->capabilities = capabilities = xcalloc(1, sizeof(*capabilities));
	u->sess = ne_session_create(u->scheme, u->host, u->port);

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
      }	break;
    }

exit:
    if (rc == 0 && uret != NULL)
	*uret = urlLink(u, __FUNCTION__);
    u = urlFree(u, "urlSplit (davInit)");

    return rc;
}


/* =============================================================== */
static int my_result(const char * msg, int ret, FILE * fp)
{
    /* HACK: don't print unless debugging. */
    if (_dav_debug >= 0)
	return ret;
    if (fp == NULL)
	fp = stderr;
    if (msg != NULL)
	fprintf(fp, "*** %s: ", msg);

    /* HACK FTPERR_NE_FOO == -NE_FOO error impedance match */
#ifdef	HACK
    fprintf(fp, "%s: %s\n", ftpStrerror(-ret), ne_get_error(sess));
#else
    fprintf(fp, "%s\n", ftpStrerror(-ret));
#endif
    return ret;
}

#ifdef	DYING
static void hexdump(const unsigned char * buf, ssize_t len)
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

    if (!(u && value)) return;
if (_dav_debug < 0)
fprintf(stderr, "*** u %p Accept-Ranges: %s\n", u, value);
    if (!strcmp(value, "bytes"))
	u->httpHasRange = 1;
    if (!strcmp(value, "none"))
	u->httpHasRange = 0;
}

#if !defined(HAVE_NEON_NE_GET_RESPONSE_HEADER)
static void davAllHeaders(void * userdata, const char * value)
{
    FD_t ctrl = userdata;

    if (!(ctrl && value)) return;
if (_dav_debug)
fprintf(stderr, "<- %s\n", value);
}
#endif

static void davContentLength(void * userdata, const char * value)
{
    FD_t ctrl = userdata;

    if (!(ctrl && value)) return;
if (_dav_debug < 0)
fprintf(stderr, "*** fd %p Content-Length: %s\n", ctrl, value);
   ctrl->contentLength = strtoll(value, NULL, 10);
}

static void davConnection(void * userdata, const char * value)
{
    FD_t ctrl = userdata;

    if (!(ctrl && value)) return;
if (_dav_debug < 0)
fprintf(stderr, "*** fd %p Connection: %s\n", ctrl, value);
    if (!strcasecmp(value, "close"))
	ctrl->persist = 0;
    else if (!strcasecmp(value, "Keep-Alive"))
	ctrl->persist = 1;
}

/* HACK: stash error in *str. */
int davResp(urlinfo u, FD_t ctrl, char *const * str)
{
    int rc = 0;

    rc = ne_begin_request(ctrl->req);
    rc = my_result("ne_begin_req(ctrl->req)", rc, NULL);

if (_dav_debug < 0)
fprintf(stderr, "*** davResp(%p,%p,%p) sess %p req %p rc %d\n", u, ctrl, str, u->sess, ctrl->req, rc);

    /* HACK FTPERR_NE_FOO == -NE_FOO error impedance match */
    if (rc)
	fdSetSyserrno(ctrl, errno, ftpStrerror(-rc));

    return rc;
}

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
    ctrl->req = ne_request_create(u->sess, httpCmd, httpArg);
assert(ctrl->req != NULL);

    ne_set_request_private(ctrl->req, "fd", ctrl);

#if !defined(HAVE_NEON_NE_GET_RESPONSE_HEADER)
    ne_add_response_header_catcher(ctrl->req, davAllHeaders, ctrl);

    ne_add_response_header_handler(ctrl->req, "Content-Length",
		davContentLength, ctrl);
    ne_add_response_header_handler(ctrl->req, "Connection",
		davConnection, ctrl);
#endif

    if (!strcmp(httpCmd, "PUT")) {
#if defined(HAVE_NEON_NE_SEND_REQUEST_CHUNK)
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
#if !defined(HAVE_NEON_NE_GET_RESPONSE_HEADER)
	ne_add_response_header_handler(ctrl->req, "Accept-Ranges",
			davAcceptRanges, u);
#endif
	/* HACK: possible Transfer-Encoding: on GET. */

	/* HACK: other errors may need retry too. */
	/* HACK: neon retries once, gud enuf. */
	/* HACK: retry counter? */
	do {
	    rc = davResp(u, ctrl, NULL);
	} while (rc == NE_RETRY);
    }
    if (rc)
	goto errxit;

if (_dav_debug < 0)
fprintf(stderr, "*** davReq(%p,%s,\"%s\") exit sess %p req %p rc %d\n", ctrl, httpCmd, (httpArg ? httpArg : ""), u->sess, ctrl->req, rc);

#if defined(HAVE_NEON_NE_GET_RESPONSE_HEADER)
    davContentLength(ctrl,
		ne_get_response_header(ctrl->req, "Content-Length"));
    davConnection(ctrl,
		ne_get_response_header(ctrl->req, "Connection"));
    if (strcmp(httpCmd, "PUT"))
	davAcceptRanges(u,
		ne_get_response_header(ctrl->req, "Accept-Ranges"));
#endif

    ctrl = fdLink(ctrl, "open data (davReq)");
    return 0;

errxit:
    fdSetSyserrno(ctrl, errno, ftpStrerror(rc));

    /* HACK balance fd refs. ne_session_destroy to tear down non-keepalive? */
    ctrl = fdLink(ctrl, "error data (davReq)");

    return rc;
}

FD_t davOpen(const char * url, int flags,
		mode_t mode, urlinfo * uret)
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
assert(urlType == URL_IS_HTTPS || urlType == URL_IS_HTTP || urlType == URL_IS_HKP);
	fd->urlType = urlType;
    }

exit:
    if (uret)
	*uret = u;
    return fd;
}

ssize_t davRead(void * cookie, char * buf, size_t count)
{
    FD_t fd = cookie;
    ssize_t rc;

#if 0
assert(count >= 128);	/* HACK: see ne_request.h comment */
#endif
    rc = ne_read_response_block(fd->req, buf, count);

if (_dav_debug < 0) {
fprintf(stderr, "*** davRead(%p,%p,0x%x) rc 0x%x\n", cookie, buf, (unsigned)count, (unsigned)rc);
#ifdef	DYING
hexdump(buf, rc);
#endif
    }

    return rc;
}

ssize_t davWrite(void * cookie, const char * buf, size_t count)
{
    ssize_t rc;
    int xx;

#ifndef	NEONBLOWSCHUNKS
    ne_session * sess;

assert(fd->req != NULL);
    sess = ne_get_session(fd->req);
assert(sess != NULL);

    /* HACK: include ne_private.h to access sess->socket for now. */
    xx = ne_sock_fullwrite(sess->socket, buf, count);
#else
#if defined(HAVE_NEON_NE_SEND_REQUEST_CHUNK)
assert(fd->req != NULL);
    xx = ne_send_request_chunk(fd->req, buf, count);
#else
    errno = EIO;       /* HACK */
    return -1;
#endif
#endif

    /* HACK: stupid error impedence matching. */
    rc = (xx == 0 ? count : -1);

if (_dav_debug < 0)
fprintf(stderr, "*** davWrite(%p,%p,0x%x) rc 0x%x\n", cookie, buf, (unsigned)count, (unsigned)rc);
#ifdef	DYING
if (count > 0)
hexdump(buf, count);
#endif

    return rc;
}

int davSeek(void * cookie, _libio_pos_t pos, int whence)
{
if (_dav_debug < 0)
fprintf(stderr, "*** davSeek(%p,pos,%d)\n", cookie, whence);
    return -1;
}

int davClose(void * cookie)
{
    FD_t fd = cookie;
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


#endif /* WITH_NEON */

