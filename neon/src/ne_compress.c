/* 
   Handling of compressed HTTP responses
   Copyright (C) 2001, Joe Orton <joe@manyfish.co.uk>

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

#include <assert.h>

#include "ne_request.h"
#include "ne_compress.h"
#include "ne_utils.h"

#include <zlib.h>

/* Adds support for the 'gzip' Content-Encoding in HTTP.  gzip is a
 * file format which wraps the DEFLATE compression algorithm.  zlib
 * implements DEFLATE: we have to unwrap the gzip format as it comes
 * off the wire, and hand off chunks of data to be inflated. */

struct ne_decompress_s {
    ne_session *session; /* associated session. */
    /* temporary buffer for holding inflated data. */
    char outbuf[BUFSIZ];
    z_stream zstr;
    char *enchdr; /* value of Content-Enconding response header. */

    /* pass blocks back to this. */
    ne_block_reader reader;
    void *userdata;

    /* buffer for gzip header bytes. */
    union {
	unsigned char buf[10];
	struct header {
	    unsigned char id1;
	    unsigned char id2;
	    unsigned char cmeth; /* compression method. */
	    unsigned char flags;
	    unsigned int mtime;
	    unsigned char xflags;
	    unsigned char os;
	} hdr;
    } in;
    size_t incount;    /* bytes in in.buf */

    /* current state. */
    enum state {
	BEFORE_DATA, /* not received any response blocks yet. */
	PASSTHROUGH, /* response not compressed: passing through. */
	IN_HEADER, /* received a few bytes of response data, but not
		    * got past the gzip header yet. */
	POST_HEADER, /* waiting for the end of the NUL-terminated bits. */
	INFLATING, /* inflating response bytes. */
	FINISHED, /* inflate has returned Z_STREAM_END: not expecting
		   * any more response data. */
	ERROR /* inflate bombed. */
    } state;
};

#define ID1 0x1f
#define ID2 0x8b

#define HDR_DONE 0
#define HDR_EXTENDED 1
#define HDR_ERROR 2

/* parse_header parses the gzip header, sets the next state and returns
 *   HDR_DONE: all done, bytes following are raw DEFLATE data.
 *   HDR_EXTENDED: all done, expect a NUL-termianted string
 *                 before the DEFLATE data
 *   HDR_ERROR: invalid header, give up.
 */
static int parse_header(ne_decompress *ctx)
{
    struct header *h = &ctx->in.hdr;

    NE_DEBUG(NE_DBG_HTTP, "ID1: %d  ID2: %d, cmeth %d, flags %d\n", 
	    h->id1, h->id2, h->cmeth, h->flags);
    
    if (h->id1 != ID1 || h->id2 != ID2 || h->cmeth != 8) {
	ctx->state = ERROR;
	ne_set_error(ctx->session, "Compressed stream invalid");
	return HDR_ERROR;
    }

    NE_DEBUG(NE_DBG_HTTP, "mtime: %d, xflags: %d, os: %d\n",
	     h->mtime, h->xflags, h->os);
    
    /* TODO: we can only handle one NUL-terminated extensions field
     * currently.  Really, we should count the number of bits set, and
     * skip as many fields as bits set (bailing if any reserved bits
     * are set. */
    if (h->flags == 8) {
	ctx->state = POST_HEADER;
	return HDR_EXTENDED;
    } else if (h->flags != 0) {
	ctx->state = ERROR;
	ne_set_error(ctx->session, "Compressed stream not supported");
	return HDR_ERROR;
    }

    NE_DEBUG(NE_DBG_HTTP, "compress: Good stream.\n");
    
    ctx->state = INFLATING;
    return HDR_DONE;
}

static int find_token(const char *value, const char *wanted)
{
    char *buf = ne_strdup(value), *pnt = buf, *tok;
    int ret = 0;

    while ((tok = ne_token(&pnt, ',', NULL)) != NULL)
    {
	if (strcasecmp(tok, wanted) == 0) {
	    ret = 1;
	    break;
	}
    }

    free(buf);
    
    return ret;
}

/* inflate it baby. */
static void do_inflate(ne_decompress *ctx, const char *buf, size_t len)
{
    int ret;

    ctx->zstr.avail_in = len;
    ctx->zstr.next_in = (char *)buf;
    ctx->zstr.total_in = 0;
    
    do {
	ctx->zstr.avail_out = BUFSIZ;
	ctx->zstr.next_out = ctx->outbuf;
	ctx->zstr.total_out = 0;
	
	ret = inflate(&ctx->zstr, Z_NO_FLUSH);
	
	NE_DEBUG(NE_DBG_HTTP, 
		 "compress: inflate %d, %ld bytes out, %d remaining\n",
		 ret, ctx->zstr.total_out, ctx->zstr.avail_in);
#if 0
	NE_DEBUG(NE_DBG_HTTPBODY,
		 "Inflated body block (%ld):\n[%.*s]\n", 
		 ctx->zstr.total_out, (int)ctx->zstr.total_out, 
		 ctx->outbuf);
#endif
	
	/* pass on the inflated data */
	ctx->reader(ctx->userdata, ctx->outbuf, ctx->zstr.total_out);
	
    } while (ret == Z_OK && ctx->zstr.avail_out == 0);
    
    if (ret == Z_STREAM_END) {
	NE_DEBUG(NE_DBG_HTTP, "compress: end of stream.\n");
	ctx->state = FINISHED;
    } else if (ret != Z_OK) {
	ctx->state = ERROR;
	ne_set_error(ctx->session, "Error reading compressed data.");
	NE_DEBUG(NE_DBG_HTTP, "compress: inflate failed (%d): %s\n", 
		 ret, ctx->zstr.msg);
    }
}

static void gz_reader(void *ud, const char *buf, size_t len)
{
    ne_decompress *ctx = ud;
    const char *zbuf;
    size_t count;

    switch (ctx->state) {
    case PASSTHROUGH:
	/* move along there. */
	ctx->reader(ctx->userdata, buf, len);
	return;

    case ERROR:
	/* beyond hope. */
	return;

    case FINISHED:
	NE_DEBUG(NE_DBG_HTTP, 
		 "compress: %d bytes in after end of stream.\n", len);
	return;

    case BEFORE_DATA:
	/* work out whether this is a compressed response or not. */
	if (ctx->enchdr != NULL && find_token(ctx->enchdr, "gzip")) {
	    NE_DEBUG(NE_DBG_HTTP, "compress: got gzipped stream.\n");

	    /* This is the magic bit: using plain inflateInit()
	     * doesn't work, and this does, but I have no idea why..
	     * Google showed me the way. */
	    if (inflateInit2(&ctx->zstr, -MAX_WBITS) != Z_OK) {
		/* bugger. can't get to the session. */
		ne_set_error(ctx->session, ctx->zstr.msg);
		ctx->state = ERROR;
		return;
	    }

	} else {
	    /* No Content-Encoding header: pass it on.  TODO: we could
	     * hack it and register the real callback now. But that
	     * would require add_resp_body_rdr to have defined
	     * ordering semantics etc etc */
	    ctx->state = PASSTHROUGH;
	    ctx->reader(ctx->userdata, buf, len);
	    return;
	}

	ctx->state = IN_HEADER;
	/* FALLTHROUGH */

    case IN_HEADER:
	/* copy as many bytes as possible into the buffer. */
	if (len + ctx->incount > 10) {
	    count = 10 - ctx->incount;
	} else {
	    count = len;
	}
	memcpy(ctx->in.buf + ctx->incount, buf, count);
	ctx->incount += count;
	/* have we got the full header yet? */
	if (ctx->incount != 10) {
	    return;
	}

	buf += count;
	len -= count;

	switch (parse_header(ctx)) {
	case HDR_ERROR:
	    return;
	case HDR_EXTENDED:
	    if (len == 0)
		return;
	    break;
	case HDR_DONE:
	    if (len > 0) {
		do_inflate(ctx, buf, len);
	    }
	    return;
	}

	/* FALLTHROUGH */

    case POST_HEADER:
	/* eating the filename string. */
	zbuf = memchr(buf, '\0', len);
	if (zbuf == NULL) {
	    /* not found it yet. */
	    return;
	}

	NE_DEBUG(NE_DBG_HTTP, "compresss: skipped %d header bytes.\n", 
		 zbuf - buf);
	/* found end of string. */
	len -= (1 + zbuf - buf);
	buf = zbuf + 1;
	ctx->state = INFLATING;
	if (len == 0) {
	    /* end of string was at end of buffer. */
	    return;
	}

	/* FALLTHROUGH */

    case INFLATING:
	do_inflate(ctx, buf, len);
	break;
    }    
    
}

int ne_decompress_destroy(ne_decompress *ctx)
{
    if (ctx->state != PASSTHROUGH && ctx->state != BEFORE_DATA) {
	/* stream must have been initialized */
	inflateEnd(&ctx->zstr);
    }
    if (ctx->enchdr)
	free(ctx->enchdr);
    free(ctx);
    return (ctx->state == ERROR);
}

ne_decompress *ne_decompress_reader(ne_request *req, ne_accept_response acpt,
				    ne_block_reader rdr, void *userdata)
{
    ne_decompress *ctx = ne_calloc(sizeof *ctx);

    ne_add_request_header(req, "Accept-Encoding", "gzip");

    ne_add_response_header_handler(req, "Content-Encoding", 
				   ne_duplicate_header, &ctx->enchdr);

    ne_add_response_body_reader(req, acpt, gz_reader, ctx);

    ctx->state = BEFORE_DATA;
    ctx->reader = rdr;
    ctx->userdata = userdata;
    ctx->session = ne_get_session(req);

    return ctx;    
}
