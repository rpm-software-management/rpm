/* 
   HTTP session handling
   Copyright (C) 1999-2004, Joe Orton <joe@manyfish.co.uk>
   Portions are:
   Copyright (C) 1999-2000 Tommi Komulainen <Tommi.Komulainen@iki.fi>

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

#include "config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef NE_HAVE_IDNA
#include <idna.h>
#endif

#include "ne_session.h"
#include "ne_alloc.h"
#include "ne_utils.h"
#include "ne_i18n.h"
#include "ne_string.h"

#include "ne_private.h"

/* Destroy a a list of hooks. */
static void destroy_hooks(struct hook *hooks)
{
    struct hook *nexthk;

    while (hooks) {
	nexthk = hooks->next;
	ne_free(hooks);
	hooks = nexthk;
    }
}

void ne_session_destroy(ne_session *sess) 
{
    struct hook *hk;

    NE_DEBUG(NE_DBG_HTTP, "ne_session_destroy called.\n");

    /* Run the destroy hooks. */
    for (hk = sess->destroy_sess_hooks; hk != NULL; hk = hk->next) {
	ne_destroy_sess_fn fn = (ne_destroy_sess_fn)hk->fn;
	fn(hk->userdata);
    }
    
    destroy_hooks(sess->create_req_hooks);
    destroy_hooks(sess->pre_send_hooks);
    destroy_hooks(sess->post_send_hooks);
    destroy_hooks(sess->destroy_req_hooks);
    destroy_hooks(sess->destroy_sess_hooks);
    destroy_hooks(sess->private);

    ne_free(sess->scheme);
    ne_free(sess->server.hostname);
    ne_free(sess->server.hostport);
    if (sess->server.address) ne_addr_destroy(sess->server.address);
    if (sess->proxy.address) ne_addr_destroy(sess->proxy.address);
    if (sess->proxy.hostname) ne_free(sess->proxy.hostname);
    if (sess->user_agent) ne_free(sess->user_agent);

    if (sess->connected) {
	ne_close_connection(sess);
    }

#ifdef NE_HAVE_SSL
    if (sess->ssl_context)
        ne_ssl_context_destroy(sess->ssl_context);

    if (sess->server_cert)
        ne_ssl_cert_free(sess->server_cert);
    
    if (sess->client_cert)
        ne_ssl_clicert_free(sess->client_cert);
#endif

    ne_free(sess);
}

int ne_version_pre_http11(ne_session *s)
{
    return !s->is_http11;
}

/* Stores the "hostname[:port]" segment */
static void set_hostport(struct host_info *host, unsigned int defaultport)
{
    size_t len = strlen(host->hostname);
    host->hostport = ne_malloc(len + 10);
    strcpy(host->hostport, host->hostname);
    if (host->port != defaultport)
	ne_snprintf(host->hostport + len, 9, ":%u", host->port);
}

/* Stores the hostname/port in *info, setting up the "hostport"
 * segment correctly. */
static void
set_hostinfo(struct host_info *info, const char *hostname, unsigned int port)
{
#ifdef NE_HAVE_IDNA
    char *ihost;
        
#define FLAGS IDNA_USE_STD3_ASCII_RULES
    if (idna_to_ascii_8z(hostname, &ihost, FLAGS) == IDNA_SUCCESS)
        info->hostname = ihost;
    else /* fall back to use provided hostname string */
#endif
    info->hostname = ne_strdup(hostname);
    info->port = port;
}

ne_session *ne_session_create(const char *scheme,
			      const char *hostname, unsigned int port)
{
    ne_session *sess = ne_calloc(sizeof *sess);

    NE_DEBUG(NE_DBG_HTTP, "HTTP session to %s://%s:%d begins.\n",
	     scheme, hostname, port);

    strcpy(sess->error, "Unknown error.");

    /* use SSL if scheme is https */
    sess->use_ssl = !strcmp(scheme, "https");
    
    /* set the hostname/port */
    set_hostinfo(&sess->server, hostname, port);
    set_hostport(&sess->server, sess->use_ssl?443:80);

#ifdef NE_HAVE_SSL
    if (sess->use_ssl) {
        sess->ssl_context = ne_ssl_context_create(0);
    }
#endif

    sess->scheme = ne_strdup(scheme);

    return sess;
}

void ne_session_proxy(ne_session *sess, const char *hostname,
		      unsigned int port)
{
    sess->use_proxy = 1;
    if (sess->proxy.hostname) ne_free(sess->proxy.hostname);
    set_hostinfo(&sess->proxy, hostname, port);
}

void ne_set_addrlist(ne_session *sess, const ne_inet_addr **addrs, size_t n)
{
    sess->addrlist = addrs;
    sess->numaddrs = n;
}

void ne_set_error(ne_session *sess, const char *format, ...)
{
    va_list params;

    va_start(params, format);
    ne_vsnprintf(sess->error, sizeof sess->error, format, params);
    va_end(params);
}


void ne_set_progress(ne_session *sess, 
		     ne_progress progress, void *userdata)
{
    sess->progress_cb = progress;
    sess->progress_ud = userdata;
}

void ne_set_status(ne_session *sess,
		     ne_notify_status status, void *userdata)
{
    sess->notify_cb = status;
    sess->notify_ud = userdata;
}

void ne_set_persist(ne_session *sess, int persist)
{
    sess->no_persist = !persist;
}

void ne_set_read_timeout(ne_session *sess, int timeout)
{
    sess->rdtimeout = timeout;
}

#define UAHDR "User-Agent: "
#define AGENT " neon/" NEON_VERSION "\r\n"

void ne_set_useragent(ne_session *sess, const char *token)
{
    if (sess->user_agent) ne_free(sess->user_agent);
    sess->user_agent = ne_malloc(strlen(UAHDR) + strlen(AGENT) + 
                                 strlen(token) + 1);
#ifdef HAVE_STPCPY
    strcpy(stpcpy(stpcpy(sess->user_agent, UAHDR), token), AGENT);
#else
    strcat(strcat(strcpy(sess->user_agent, UAHDR), token), AGENT);
#endif
}

const char *ne_get_server_hostport(ne_session *sess)
{
    return sess->server.hostport;
}

const char *ne_get_scheme(ne_session *sess)
{
    return sess->scheme;
}

void ne_fill_server_uri(ne_session *sess, ne_uri *uri)
{
    uri->host = ne_strdup(sess->server.hostname);
    uri->port = sess->server.port;
    uri->scheme = ne_strdup(sess->scheme);
}

const char *ne_get_error(ne_session *sess)
{
    return ne_strclean(sess->error);
}

void ne_close_connection(ne_session *sess)
{
    if (sess->connected) {
	NE_DEBUG(NE_DBG_SOCKET, "Closing connection.\n");
	ne_sock_close(sess->socket);
	sess->socket = NULL;
	NE_DEBUG(NE_DBG_SOCKET, "Connection closed.\n");
    } else {
	NE_DEBUG(NE_DBG_SOCKET, "(Not closing closed connection!).\n");
    }
    sess->connected = 0;
}

void ne_ssl_set_verify(ne_session *sess, ne_ssl_verify_fn fn, void *userdata)
{
    sess->ssl_verify_fn = fn;
    sess->ssl_verify_ud = userdata;
}

void ne_ssl_provide_clicert(ne_session *sess, 
			  ne_ssl_provide_fn fn, void *userdata)
{
    sess->ssl_provide_fn = fn;
    sess->ssl_provide_ud = userdata;
}

void ne_ssl_trust_cert(ne_session *sess, const ne_ssl_certificate *cert)
{
#ifdef NE_HAVE_SSL
    ne_ssl_context_trustcert(sess->ssl_context, cert);
#endif
}
