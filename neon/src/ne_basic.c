/* 
   Basic HTTP and WebDAV methods
   Copyright (C) 1999-2003, Joe Orton <joe@manyfish.co.uk>

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

#include <sys/types.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <errno.h>

#include "ne_request.h"
#include "ne_alloc.h"
#include "ne_utils.h"
#include "ne_basic.h"
#include "ne_207.h"

#ifndef NEON_NODAV
#include "ne_uri.h"
#endif

#ifdef USE_DAV_LOCKS
#include "ne_locks.h"
#endif
#include "ne_dates.h"
#include "ne_i18n.h"

/* Header parser to retrieve Last-Modified date */
static void get_lastmodified(void *userdata, const char *value) {
    time_t *modtime = userdata;
    *modtime = ne_httpdate_parse(value);
}

int ne_getmodtime(ne_session *sess, const char *uri, time_t *modtime) 
{
    ne_request *req = ne_request_create(sess, "HEAD", uri);
    int ret;

    ne_add_response_header_handler(req, "Last-Modified", get_lastmodified,
				   modtime);

    *modtime = -1;

    ret = ne_request_dispatch(req);

    if (ret == NE_OK && ne_get_status(req)->klass != 2) {
	*modtime = -1;
	ret = NE_ERROR;
    }

    ne_request_destroy(req);

    return ret;
}

/* PUT's from fd to URI */
int ne_put(ne_session *sess, const char *uri, int fd) 
{
    ne_request *req = ne_request_create(sess, "PUT", uri);
    int ret;
    
#ifdef USE_DAV_LOCKS
    ne_lock_using_resource(req, uri, 0);
    ne_lock_using_parent(req, uri);
#endif

    ne_set_request_body_fd(req, fd);
	
    ret = ne_request_dispatch(req);
    
    if (ret == NE_OK && ne_get_status(req)->klass != 2)
	ret = NE_ERROR;

    ne_request_destroy(req);

    return ret;
}

/* Conditional HTTP put. 
 * PUTs from fd to uri, returning NE_FAILED if resource as URI has
 * been modified more recently than 'since'.
 */
int 
ne_put_if_unmodified(ne_session *sess, const char *uri, int fd, 
		     time_t since)
{
    ne_request *req;
    char *date;
    int ret;
    
    if (ne_version_pre_http11(sess)) {
	time_t modtime;
	/* Server is not minimally HTTP/1.1 compliant.  Do a HEAD to
	 * check the remote mod time. Of course, this makes the
	 * operation very non-atomic, but better than nothing. */
	ret = ne_getmodtime(sess, uri, &modtime);
	if (ret != NE_OK) return ret;
	if (modtime != since)
	    return NE_FAILED;
    }

    req = ne_request_create(sess, "PUT", uri);

    date = ne_rfc1123_date(since);
    /* Add in the conditionals */
    ne_add_request_header(req, "If-Unmodified-Since", date);
    ne_free(date);
    
#ifdef USE_DAV_LOCKS
    ne_lock_using_resource(req, uri, 0);
    /* FIXME: this will give 412 if the resource doesn't exist, since
     * PUT may modify the parent... does that matter?  */
#endif

    ne_set_request_body_fd(req, fd);

    ret = ne_request_dispatch(req);
    
    if (ret == NE_OK) {
	if (ne_get_status(req)->code == 412) {
	    ret = NE_FAILED;
	} else if (ne_get_status(req)->klass != 2) {
	    ret = NE_ERROR;
	}
    }

    ne_request_destroy(req);

    return ret;
}

struct get_context {
    int error;
    ne_session *session;
    off_t total;
    int fd; /* used in get_to_fd */
    ne_content_range *range;
};

static void get_to_fd(void *userdata, const char *block, size_t length)
{
    struct get_context *ctx = userdata;
    ssize_t ret;
    
    if (!ctx->error) {
	while (length > 0) {
	    ret = write(ctx->fd, block, length);
	    if (ret < 0) {
		char err[200];
		ctx->error = 1;
		ne_strerror(errno, err, sizeof err);
		ne_set_error(ctx->session, _("Could not write to file: %s"),
			     err);
		break;
	    } else {
		length -= ret;
		block += ret;
	    }
	}
    }
}

static int accept_206(void *ud, ne_request *req, const ne_status *st)
{
    return (st->code == 206);
}

static void clength_hdr_handler(void *ud, const char *value)
{
    struct get_context *ctx = ud;
    off_t len = strtol(value, NULL, 10);
    
    if (ctx->range->end == -1) {
	ctx->range->end = ctx->range->start + len - 1;
	ctx->range->total = len;
    }
    else if (len != ctx->total) {
	NE_DEBUG(NE_DBG_HTTP, 
		 "Expecting %" NE_FMT_OFF_T " bytes, "
		 "got entity of length %" NE_FMT_OFF_T "\n", 
		 ctx->total, len);
	ne_set_error(ctx->session, _("Response not of expected length"));
	ctx->error = 1;
    }
}

static void content_range_hdr_handler(void *ud, const char *value)
{
    struct get_context *ctx = ud;

    if (strncmp(value, "bytes ", 6) != 0) {
	ne_set_error(ctx->session, ("Response range using unrecognized unit"));
	ctx->error = 1;
    }

    /* TODO: verify against requested range. */
}

int ne_get_range(ne_session *sess, const char *uri, 
		 ne_content_range *range, int fd)
{
    ne_request *req = ne_request_create(sess, "GET", uri);
    struct get_context ctx;
    const ne_status *status;
    int ret;

    if (range->end == -1) {
	ctx.total = -1;
    } 
    else {
	ctx.total = (range->end - range->start) + 1;
    }

    NE_DEBUG(NE_DBG_HTTP, "Range total: %" NE_FMT_OFF_T "\n", ctx.total);

    ctx.fd = fd;
    ctx.error = 0;
    ctx.range = range;
    ctx.session = sess;

    ne_add_response_header_handler(req, "Content-Length",
				     clength_hdr_handler, &ctx);
    ne_add_response_header_handler(req, "Content-Range",
				     content_range_hdr_handler,
				     &ctx);

    ne_add_response_body_reader(req, accept_206, get_to_fd, &ctx);

    if (range->end == -1) {
	ne_print_request_header(req, "Range", "bytes=%" NE_FMT_OFF_T "-", 
				range->start);
    }
    else {
	ne_print_request_header(req, "Range", 
				"bytes=%" NE_FMT_OFF_T "-%" NE_FMT_OFF_T,
				range->start, range->end);
    }
    ne_add_request_header(req, "Accept-Ranges", "bytes");

    ret = ne_request_dispatch(req);

    status = ne_get_status(req);

    if (ctx.error) {
	ret = NE_ERROR;
    } else if (status && status->code == 416) {
	/* connection is terminated too early with Apache/1.3, so we check
	 * this even if ret == NE_ERROR... */
	ne_set_error(sess, _("Range is not satisfiable"));
	ret = NE_ERROR;
    }
    else if (ret == NE_OK) {
	if (status->klass == 2 && status->code != 206) {
	    ne_set_error(sess, _("Resource does not support ranged GETs."));
	    ret = NE_ERROR;
	}
	else if (status->klass != 2) {
	    ret = NE_ERROR;
	}
    } 
    
    ne_request_destroy(req);

    return ret;
}


/* Get to given fd */
int ne_get(ne_session *sess, const char *uri, int fd)
{
    ne_request *req = ne_request_create(sess, "GET", uri);
    struct get_context ctx;
    int ret;

    ctx.total = -1;
    ctx.fd = fd;
    ctx.error = 0;
    ctx.session = sess;

    /* Read the value of the Content-Length header into ctx.total */
    ne_add_response_header_handler(req, "Content-Length",
				     ne_handle_numeric_header,
				     &ctx.total);
    
    ne_add_response_body_reader(req, ne_accept_2xx, get_to_fd, &ctx);

    ret = ne_request_dispatch(req);
    
    if (ctx.error) {
	ret = NE_ERROR;
    } else if (ret == NE_OK && ne_get_status(req)->klass != 2) {
	ret = NE_ERROR;
    }

    ne_request_destroy(req);

    return ret;
}


/* Get to given fd */
int ne_post(ne_session *sess, const char *uri, int fd, const char *buffer)
{
    ne_request *req = ne_request_create(sess, "POST", uri);
    struct get_context ctx;
    int ret;

    ctx.total = -1;
    ctx.fd = fd;
    ctx.error = 0;
    ctx.session = sess;

    /* Read the value of the Content-Length header into ctx.total */
    ne_add_response_header_handler(req, "Content-Length",
				     ne_handle_numeric_header, &ctx.total);

    ne_add_response_body_reader(req, ne_accept_2xx, get_to_fd, &ctx);

    ne_set_request_body_buffer(req, buffer, strlen(buffer));

    ret = ne_request_dispatch(req);
    
    if (ctx.error) {
	ret = NE_ERROR;
    }
    else if (ret == NE_OK && ne_get_status(req)->klass != 2) {
	ret = NE_ERROR;
    }

    ne_request_destroy(req);

    return ret;
}

void ne_content_type_handler(void *userdata, const char *value)
{
    ne_content_type *ct = userdata;
    char *sep, *stype;

    ct->value = ne_strdup(value);
    
    stype = strchr(ct->value, '/');
    if (!stype) {
	NE_FREE(ct->value);
	return;
    }

    *stype++ = '\0';
    ct->type = ct->value;
    
    sep = strchr(stype, ';');

    if (sep) {
	char *tok;
	/* look for the charset parameter. TODO; probably better to
	 * hand-carve a parser than use ne_token/strstr/shave here. */
	*sep++ = '\0';
	do {
	    tok = ne_qtoken(&sep, ';', "\"\'");
	    if (tok) {
		tok = strstr(tok, "charset=");
		if (tok)
		    ct->charset = ne_shave(tok+8, "\"\'");
	    } else {
		break;
	    }
	} while (sep != NULL);
    }

    /* set subtype, losing any trailing whitespace */
    ct->subtype = ne_shave(stype, " \t");
    
    /* 2616#3.7.1: subtypes of text/ default to charset ISO-8859-1. */
    if (ct->charset == NULL && strcasecmp(ct->type, "text") == 0)
	ct->charset = "ISO-8859-1";
}

static void dav_hdr_handler(void *userdata, const char *value)
{
    char *tokens = ne_strdup(value), *pnt = tokens;
    ne_server_capabilities *caps = userdata;
    
    do {
	char *tok = ne_qtoken(&pnt, ',',  "\"'");
	if (!tok) break;
	
	tok = ne_shave(tok, " \r\t\n");

	if (strcmp(tok, "1") == 0) {
	    caps->dav_class1 = 1;
	} else if (strcmp(tok, "2") == 0) {
	    caps->dav_class2 = 1;
	} else if (strcmp(tok, "<http://apache.org/dav/propset/fs/1>") == 0) {
	    caps->dav_executable = 1;
	}
    } while (pnt != NULL);
    
    ne_free(tokens);

}

int ne_options(ne_session *sess, const char *uri,
		  ne_server_capabilities *caps)
{
    ne_request *req = ne_request_create(sess, "OPTIONS", uri);
    
    int ret;

    ne_add_response_header_handler(req, "DAV", dav_hdr_handler, caps);

    ret = ne_request_dispatch(req);
 
    if (ret == NE_OK && ne_get_status(req)->klass != 2) {
	ret = NE_ERROR;
    }
    
    ne_request_destroy(req);

    return ret;
}

#ifndef NEON_NODAV

void ne_add_depth_header(ne_request *req, int depth)
{
    const char *value;
    switch(depth) {
    case NE_DEPTH_ZERO:
	value = "0";
	break;
    case NE_DEPTH_ONE:
	value = "1";
	break;
    default:
	value = "infinity";
	break;
    }
    ne_add_request_header(req, "Depth", value);
}

static int copy_or_move(ne_session *sess, int is_move, int overwrite,
			int depth, const char *src, const char *dest) 
{
    ne_request *req = ne_request_create( sess, is_move?"MOVE":"COPY", src );

    /* 2518 S8.9.2 says only use Depth: infinity with MOVE. */
    if (!is_move) {
	ne_add_depth_header(req, depth);
    }

#ifdef USE_DAV_LOCKS
    if (is_move) {
	ne_lock_using_resource(req, src, NE_DEPTH_INFINITE);
    }
    ne_lock_using_resource(req, dest, NE_DEPTH_INFINITE);
    /* And we need to be able to add members to the destination's parent */
    ne_lock_using_parent(req, dest);
#endif

    ne_print_request_header(req, "Destination", "%s://%s%s", 
			      ne_get_scheme(sess), 
			      ne_get_server_hostport(sess), dest);
    
    ne_add_request_header(req, "Overwrite", overwrite?"T":"F");

    return ne_simple_request(sess, req);
}

int ne_copy(ne_session *sess, int overwrite, int depth,
	     const char *src, const char *dest) 
{
    return copy_or_move(sess, 0, overwrite, depth, src, dest);
}

int ne_move(ne_session *sess, int overwrite,
	     const char *src, const char *dest) 
{
    return copy_or_move(sess, 1, overwrite, 0, src, dest);
}

/* Deletes the specified resource. (and in only two lines of code!) */
int ne_delete(ne_session *sess, const char *uri) 
{
    ne_request *req = ne_request_create(sess, "DELETE", uri);

#ifdef USE_DAV_LOCKS
    ne_lock_using_resource(req, uri, NE_DEPTH_INFINITE);
    ne_lock_using_parent(req, uri);
#endif
    
    /* joe: I asked on the DAV WG list about whether we might get a
     * 207 error back from a DELETE... conclusion, you shouldn't if
     * you don't send the Depth header, since we might be an HTTP/1.1
     * client and a 2xx response indicates success to them.  But
     * it's all a bit unclear. In any case, DAV servers today do
     * return 207 to DELETE even if we don't send the Depth header.
     * So we handle 207 errors appropriately. */

    return ne_simple_request(sess, req);
}

int ne_mkcol(ne_session *sess, const char *uri) 
{
    ne_request *req;
    char *real_uri;
    int ret;

    if (ne_path_has_trailing_slash(uri)) {
	real_uri = ne_strdup(uri);
    } else {
	real_uri = ne_concat(uri, "/", NULL);
    }

    req = ne_request_create(sess, "MKCOL", real_uri);

#ifdef USE_DAV_LOCKS
    ne_lock_using_resource(req, real_uri, 0);
    ne_lock_using_parent(req, real_uri);
#endif
    
    ret = ne_simple_request(sess, req);

    ne_free(real_uri);

    return ret;
}

#endif /* NEON_NODAV */
