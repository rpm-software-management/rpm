/* 
   HTTP session handling
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

#include "config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "ne_session.h"
#include "ne_alloc.h"
#include "ne_utils.h"

#include "ne_private.h"

#define NEON_USERAGENT "neon/" NEON_VERSION;

/* Initializes an HTTP session */
ne_session *ne_session_create(void) 
{
    ne_session *sess = ne_calloc(sizeof *sess);

    NE_DEBUG(NE_DBG_HTTP, "HTTP session begins.\n");
    strcpy(sess->error, "Unknown error.");
    sess->version_major = -1;
    sess->version_minor = -1;
    /* Default expect-100 to OFF. */
    sess->expect100_works = -1;
    return sess;
}

int ne_session_destroy(ne_session *sess) 
{
    struct hook *hk;

    NE_DEBUG(NE_DBG_HTTP, "ne_session_destroy called.\n");

    /* Clear the hooks. */
    hk = sess->hooks;
    while (hk) {
	struct hook *nexthk = hk->next;
	if (hk->free) {
	    hk->free(hk->private);
	}
	free(hk);
	hk = nexthk;
    }

    NE_FREE(sess->server.hostname);
    NE_FREE(sess->server.hostport);
    NE_FREE(sess->proxy.hostport);
    NE_FREE(sess->user_agent);

    if (sess->connected) {
	ne_close_connection(sess);
    }

    free(sess);
    return NE_OK;
}

int ne_version_pre_http11(ne_session *s)
{
    return (s->version_major<1 || (s->version_major==1 && s->version_minor<1));
}

static char *get_hostport(struct host_info *host) 
{
    size_t len = strlen(host->hostname);
    char *ret = ne_malloc(len + 10);
    strcpy(ret, host->hostname);
    if (host->port != 80) {
	snprintf(ret + len, 9, ":%d", host->port);
    }
    return ret;
}

static void
set_hostinfo(struct host_info *info, const char *hostname, int port)
{
    NE_FREE(info->hostport);
    NE_FREE(info->hostname);
    info->hostname= ne_strdup(hostname);
    info->port = port;
    info->hostport = get_hostport(info);
}

static int lookup_host(ne_session *sess, struct host_info *info)
{
    if (sess->notify_cb) {
	sess->notify_cb(sess->notify_ud, ne_conn_namelookup, info->hostname);
    }
    if (sock_name_lookup(info->hostname, &info->addr)) {
	return NE_LOOKUP;
    } else {
	return NE_OK;
    }
}

int ne_session_server(ne_session *sess, const char *hostname, int port)
{
    if (sess->connected && !sess->have_proxy) {
	/* Force reconnect */
	ne_close_connection(sess);
    }
    set_hostinfo(&sess->server, hostname, port);
    /* We do a name lookup on the origin server if either:
     *  1) we do not have a proxy server
     *  2) we *might not* have a proxy server (since we have a 'proxy decider' function).
     */
    if (!sess->have_proxy || sess->proxy_decider) {
	return lookup_host(sess, &sess->server);
    } else {
	return NE_OK;
    }
}

void ne_set_secure_context(ne_session *sess, nssl_context *ctx)
{
    sess->ssl_context = ctx;
}

int ne_set_request_secure_upgrade(ne_session *sess, int req_upgrade)
{
#ifdef ENABLE_SSL
    sess->request_secure_upgrade = req_upgrade;
    return 0;
#else
    return -1;
#endif
}

int ne_set_accept_secure_upgrade(ne_session *sess, int acc_upgrade)
{
#ifdef ENABLE_SSL
    sess->accept_secure_upgrade = acc_upgrade;
    return 0;
#else
    return -1;
#endif
}

int ne_set_secure(ne_session *sess, int use_secure)
{
#ifdef ENABLE_SSL
    sess->use_secure = use_secure;
    return 0;
#else
    return -1;
#endif
}

void ne_session_decide_proxy(ne_session *sess, ne_use_proxy use_proxy,
			       void *userdata)
{
    sess->proxy_decider = use_proxy;
    sess->proxy_decider_udata = userdata;
}

int ne_session_proxy(ne_session *sess, const char *hostname, int port)
{
    if (sess->connected) {
	/* Force reconnect */
	ne_close_connection(sess);
    }
    sess->have_proxy = 1;
    set_hostinfo(&sess->proxy, hostname, port);
    return lookup_host(sess, &sess->proxy);
}

void ne_set_error(ne_session *sess, const char *errstring)
{
    strncpy(sess->error, errstring, BUFSIZ);
    sess->error[BUFSIZ-1] = '\0';
    STRIP_EOL(sess->error);
}


void ne_set_progress(ne_session *sess, 
		       sock_progress progress, void *userdata)
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

void ne_set_expect100(ne_session *sess, int use_expect100)
{
    if (use_expect100) {
	sess->expect100_works = 1;
    } else {
	sess->expect100_works = -1;
    }
}

void ne_set_persist(ne_session *sess, int persist)
{
    sess->no_persist = !persist;
}

void ne_set_useragent(ne_session *sess, const char *token)
{
    static const char *fixed = " " NEON_USERAGENT;
    NE_FREE(sess->user_agent);
    CONCAT2(sess->user_agent, token, fixed);
}

const char *ne_get_server_hostport(ne_session *sess) {
    return sess->server.hostport;
}

const char *ne_get_scheme(ne_session *sess)
{
    if (sess->use_secure) {
	return "https";
    } else {
	return "http";
    }
}


const char *ne_get_error(ne_session *sess) {
    return sess->error;
}

int ne_close_connection(ne_session *sess)
{
    NE_DEBUG(NE_DBG_SOCKET, "Closing connection.\n");
    if (sess->connected > 0) {
	sock_close(sess->socket);
	sess->socket = NULL;
    }
    sess->connected = 0;
    NE_DEBUG(NE_DBG_SOCKET, "Connection closed.\n");
    return 0;
}

