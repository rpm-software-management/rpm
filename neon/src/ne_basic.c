/* 
   Basic HTTP and WebDAV methods
   Copyright (C) 1999-2001, Joe Orton <joe@manyfish.co.uk>

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
    free(date);
    
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
    size_t total, progress;
    int fd; /* used in get_to_fd */
    ne_content_range *range;
};

int ne_read_file(ne_session *sess, const char *uri, 
		   ne_block_reader reader, void *userdata) {
    struct get_context ctx;
    ne_request *req = ne_request_create(sess, "GET", uri);
    int ret;

    /* Read the value of the Content-Length header into ctx.total */
    ne_add_response_header_handler(req, "Content-Length",
				   ne_handle_numeric_header, &ctx.total);
    
    ne_add_response_body_reader(req, ne_accept_2xx, reader, userdata);

    ret = ne_request_dispatch(req);

    if (ret == NE_OK && ne_get_status(req)->klass != 2)
	ret = NE_ERROR;

    ne_request_destroy(req);

    return ret;
}

static void get_to_fd(void *userdata, const char *block, size_t length)
{
    struct get_context *ctx = userdata;
    ssize_t ret;
    
    if (!ctx->error) {
	while (length > 0) {
	    ret = write(ctx->fd, block, length);
	    if (ret < 0) {
		ctx->error = errno;
		break;
	    } else {
		length -= ret;
	    }
	}
    }
}

static int accept_206(void *ud, ne_request *req, ne_status *st)
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
    else if (len != ctx->range->total) {
	NE_DEBUG(NE_DBG_HTTP, 
		 "Expecting %ld bytes, got entity of length %ld\n", 
		 (long int) ctx->range->total, (long int) len);
	ctx->error = 1;
    }
}

static void content_range_hdr_handler(void *ud, const char *value)
{
    struct get_context *ctx = ud;

    if (strncmp(value, "bytes ", 6) != 0) {
	ctx->error = 1;
    }

    /* TODO: verify against requested range. */
}

int ne_get_range(ne_session *sess, const char *uri, 
		 ne_content_range *range, int fd)
{
    ne_request *req = ne_request_create(sess, "GET", uri);
    struct get_context ctx;
    int ret;

    if (range->end == -1) {
	ctx.total = -1;
    } 
    else {
	ctx.total = (range->end - range->start) + 1;
    }

    ctx.fd = fd;
    ctx.error = 0;
    ctx.range = range;

    ne_add_response_header_handler(req, "Content-Length",
				     clength_hdr_handler, &ctx);
    ne_add_response_header_handler(req, "Content-Range",
				     content_range_hdr_handler,
				     &ctx);

    ne_add_response_body_reader(req, accept_206, get_to_fd, &ctx);

    /* icky casts to long int, which should be at least as large as the
     * off_t's */
    if (range->end == -1) {
	ne_print_request_header(req, "Range", "bytes=%ld-", 
				  (long int) range->start);
    }
    else {
	ne_print_request_header(req, "Range", "bytes=%ld-%ld",
				  (long int) range->start, 
				  (long int)range->end);
    }
    ne_add_request_header(req, "Accept-Ranges", "bytes");

    ret = ne_request_dispatch(req);
    
    if (ret == NE_OK && ne_get_status(req)->klass != 2) {
	ret = NE_ERROR;
    }
    else if (ne_get_status(req)->code != 206) {
	ne_set_error(sess, _("Server does not allow partial GETs."));
	ret = NE_ERROR;
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
    ctx.progress = 0;
    ctx.fd = fd;
    ctx.error = 0;

    /* Read the value of the Content-Length header into ctx.total */
    ne_add_response_header_handler(req, "Content-Length",
				     ne_handle_numeric_header,
				     &ctx.total);
    
    ne_add_response_body_reader(req, ne_accept_2xx, get_to_fd, &ctx);

    ret = ne_request_dispatch(req);
    
    if (ctx.error) {
	char buf[BUFSIZ];
	snprintf(buf, BUFSIZ, 
		  _("Could not write to file: %s"), strerror(ctx.error));
	ne_set_error(sess, buf);
	ret = NE_ERROR;
    }

    if (ret == NE_OK && ne_get_status(req)->klass != 2) {
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

    /* Read the value of the Content-Length header into ctx.total */
    ne_add_response_header_handler(req, "Content-Length",
				     ne_handle_numeric_header, &ctx.total);

    ne_add_response_body_reader(req, ne_accept_2xx, get_to_fd, &ctx);

    ne_set_request_body_buffer(req, buffer, strlen(buffer));

    ret = ne_request_dispatch(req);
    
    if (ctx.error) {
	char buf[BUFSIZ];
	snprintf(buf, BUFSIZ, 
		 _("Could not write to file: %s"), strerror(ctx.error));
	ne_set_error(sess, buf);
	ret = NE_ERROR;
    }

    if (ret == NE_OK && ne_get_status(req)->klass != 2) {
	ret = NE_ERROR;
    }

    ne_request_destroy(req);

    return ret;
}

static void server_hdr_handler(void *userdata, const char *value)
{
    char **tokens = split_string(value, ' ', "\"'", NULL);
    ne_server_capabilities *caps = userdata;
    int n;

    for (n = 0; tokens[n] != NULL; n++) {
	if (strncasecmp(tokens[n], "Apache/", 7) == 0 && 
	    strlen(tokens[n]) > 11) { /* 12 == "Apache/1.3.0" */
	    const char *ver = tokens[n] + 7;
	    int count;
	    char **vers;
	    vers = split_string_c(ver, '.', NULL, NULL, &count);
	    /* Apache/1.3.6 and before have broken Expect: 100 support */
	    if (count > 1 && atoi(vers[0]) < 2 && 
		atoi(vers[1]) < 4 && atoi(vers[2]) < 7) {
		caps->broken_expect100 = 1;
	    }
	    split_string_free(vers);
	}
    }    
    
    split_string_free(tokens);
}

void ne_content_type_handler(void *userdata, const char *value)
{
    ne_content_type *ct = userdata;
    char *sep, *parms;

    ct->value = ne_strdup(value);
    
    sep = strchr(ct->value, '/');
    if (!sep) {
	NE_FREE(ct->value);
	return;
    }

    *++sep = '\0';
    ct->type = ct->value;
    ct->subtype = sep;
    
    parms = strchr(ct->value, ';');

    if (parms) {
	*parms = '\0';
	/* TODO: handle charset. */
    }
}

static void dav_hdr_handler(void *userdata, const char *value)
{
    char **classes, **class;
    ne_server_capabilities *caps = userdata;
    
    classes = split_string(value, ',', "\"'", " \r\t\n");
    for (class = classes; *class!=NULL; class++) {

	if (strcmp(*class, "1") == 0) {
	    caps->dav_class1 = 1;
	} else if (strcmp(*class, "2") == 0) {
	    caps->dav_class2 = 1;
	} else if (strcmp(*class, "<http://apache.org/dav/propset/fs/1>") == 0) {
	    caps->dav_executable = 1;
	}
    }
    
    split_string_free(classes);

}

int ne_options(ne_session *sess, const char *uri,
		  ne_server_capabilities *caps)
{
    ne_request *req = ne_request_create(sess, "OPTIONS", uri);
    
    int ret;

    ne_add_response_header_handler(req, "Server", server_hdr_handler, caps);
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
			const char *src, const char *dest ) 
{
    ne_request *req = ne_request_create( sess, is_move?"MOVE":"COPY", src );

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

int ne_copy(ne_session *sess, int overwrite, 
	     const char *src, const char *dest) 
{
    return copy_or_move(sess, 0, overwrite, src, dest);
}

int ne_move(ne_session *sess, int overwrite,
	     const char *src, const char *dest) 
{
    return copy_or_move(sess, 1, overwrite, src, dest);
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

    if (uri_has_trailing_slash(uri)) {
	real_uri = ne_strdup(uri);
    } else {
	CONCAT2(real_uri, uri, "/");
    }

    req = ne_request_create(sess, "MKCOL", real_uri);

#ifdef USE_DAV_LOCKS
    ne_lock_using_resource(req, real_uri, 0);
    ne_lock_using_parent(req, real_uri);
#endif
    
    ret = ne_simple_request(sess, req);

    free(real_uri);

    return ret;
}

#endif /* NEON_NODAV */
