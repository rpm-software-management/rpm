/* 
   Handling of compressed HTTP responses
   Copyright (C) 2001-2004, Joe Orton <joe@manyfish.co.uk>

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

#include "ne_request.h"
#include "ne_compress.h"
#include "ne_utils.h"
#include "ne_i18n.h"

#include "ne_private.h"

#ifdef NEON_ZLIB

#include <zlib.h>

/* Adds support for the 'gzip' Content-Encoding in HTTP.  gzip is a
 * file format which wraps the DEFLATE compression algorithm.  zlib
 * implements DEFLATE: we have to unwrap the gzip format (specified in
 * RFC1952) as it comes off the wire, and hand off chunks of data to
 * be inflated. */

struct ne_decompress_s {
    ne_session *session; /* associated session. */
    /* temporary buffer for holding inflated data. */
    char outbuf[BUFSIZ];
    z_stream zstr;
    int zstrinit; /* non-zero if zstr has been initialized */
    char *enchdr; /* value of Content-Enconding response header. */

    /* pass blocks back to this. */
    ne_block_reader reader;
    ne_accept_response acceptor;
    void *userdata;

    /* buffer for gzip header bytes. */
    union {
	unsigned char buf[10];
	struct header {
	    unsigned char id1;
	    unsigned char id2;
	    unsigned char cmeth; /* compression method. */
	    unsigned char flags;
	    unsigned int mtime; /* breaks when sizeof int != 4 */
	    unsigned char xflags;
	    unsigned char os;
	} hdr;
    } in;
    size_t incount;    /* bytes in in.buf */
    
    unsigned char footer[8];
    size_t footcount; /* bytes in footer. */

    /* CRC32 checksum: odd that zlib uses uLong for this since it is a
     * 64-bit integer on LP64 platforms. */
    uLong checksum;

    /* current state. */
    enum state {
	NE_Z_BEFORE_DATA, /* not received any response blocks yet. */
	NE_Z_PASSTHROUGH, /* response not compressed: passing through. */
	NE_Z_IN_HEADER, /* received a few bytes of response data, but not
			 * got past the gzip header yet. */
	NE_Z_POST_HEADER, /* waiting for the end of the NUL-terminated bits. */
	NE_Z_INFLATING, /* inflating response bytes. */
	NE_Z_AFTER_DATA, /* after data; reading CRC32 & ISIZE */
	NE_Z_FINISHED, /* stream is finished. */
	NE_Z_ERROR /* inflate bombed. */
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
	ctx->state = NE_Z_ERROR;
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
	ctx->state = NE_Z_POST_HEADER;
	return HDR_EXTENDED;
    } else if (h->flags != 0) {
	ctx->state = NE_Z_ERROR;
	ne_set_error(ctx->session, "Compressed stream not supported");
	return HDR_ERROR;
    }

    NE_DEBUG(NE_DBG_HTTP, "compress: Good stream.\n");
    
    ctx->state = NE_Z_INFLATING;
    return HDR_DONE;
}

/* Convert 'buf' to unsigned int; 'buf' must be 'unsigned char *' */
#define BUF2UINT(buf) ((buf[3]<<24) + (buf[2]<<16) + (buf[1]<<8) + buf[0])

/* Process extra 'len' bytes of 'buf' which were received after the
 * DEFLATE data. */
static void process_footer(ne_decompress *ctx, 
			   const unsigned char *buf, size_t len)
{
    if (len + ctx->footcount > 8) {
        ne_set_error(ctx->session, 
                     "Too many bytes (%" NE_FMT_SIZE_T ") in gzip footer",
                     len);
	ctx->state = NE_Z_ERROR;
    } else {
	memcpy(ctx->footer + ctx->footcount, buf, len);
	ctx->footcount += len;
	if (ctx->footcount == 8) {
	    uLong crc = BUF2UINT(ctx->footer) & 0xFFFFFFFF;
	    if (crc == ctx->checksum) {
		ctx->state = NE_Z_FINISHED;
                /* reader requires a size=0 call at end-of-response */
                ctx->reader(ctx->userdata, NULL, 0);
		NE_DEBUG(NE_DBG_HTTP, "compress: Checksum match.\n");
	    } else {
		NE_DEBUG(NE_DBG_HTTP, "compress: Checksum mismatch: "
			 "given %lu vs computed %lu\n", crc, ctx->checksum);
		ne_set_error(ctx->session, 
			     "Checksum invalid for compressed stream");
		ctx->state = NE_Z_ERROR;
	    }
	}
    }
}

/* A zlib function failed with 'code'; set the session error string
 * appropriately. */
static void set_zlib_error(ne_decompress *ctx, const char *msg, int code)
{
    if (ctx->zstr.msg)
        ne_set_error(ctx->session, _("%s: %s"), msg, ctx->zstr.msg);
    else {
        const char *err;
        switch (code) {
        case Z_STREAM_ERROR: err = "stream error"; break;
        case Z_DATA_ERROR: err = "data corrupt"; break;
        case Z_MEM_ERROR: err = "out of memory"; break;
        case Z_BUF_ERROR: err = "buffer error"; break;
        case Z_VERSION_ERROR: err = "library version mismatch"; break;
        default: err = "unknown error"; break;
        }
        ne_set_error(ctx->session, _("%s: %s (code %d)"), msg, err, code);
    }
}

/* Inflate response buffer 'buf' of length 'len'. */
static void do_inflate(ne_decompress *ctx, const char *buf, size_t len)
{
    int ret;

    ctx->zstr.avail_in = len;
    ctx->zstr.next_in = (unsigned char *)buf;
    ctx->zstr.total_in = 0;
    
    do {
	ctx->zstr.avail_out = sizeof ctx->outbuf;
	ctx->zstr.next_out = (unsigned char *)ctx->outbuf;
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
	/* update checksum. */
	ctx->checksum = crc32(ctx->checksum, (unsigned char *)ctx->outbuf, 
			      ctx->zstr.total_out);

	/* pass on the inflated data, if any */
        if (ctx->zstr.total_out > 0) {
            ctx->reader(ctx->userdata, ctx->outbuf, ctx->zstr.total_out);
        }	
    } while (ret == Z_OK && ctx->zstr.avail_in > 0);
    
    if (ret == Z_STREAM_END) {
	NE_DEBUG(NE_DBG_HTTP, "compress: end of data stream, remaining %d.\n",
		 ctx->zstr.avail_in);
	/* process the footer. */
	ctx->state = NE_Z_AFTER_DATA;
	process_footer(ctx, ctx->zstr.next_in, ctx->zstr.avail_in);
    } else if (ret != Z_OK) {
	ctx->state = NE_Z_ERROR;
        set_zlib_error(ctx, _("Could not inflate data"), ret);
    }
}

/* Callback which is passed blocks of the response body. */
static void gz_reader(void *ud, const char *buf, size_t len)
{
    ne_decompress *ctx = ud;
    const char *zbuf;
    size_t count;

    switch (ctx->state) {
    case NE_Z_PASSTHROUGH:
	/* move along there. */
	ctx->reader(ctx->userdata, buf, len);
	return;

    case NE_Z_ERROR:
	/* beyond hope. */
	break;

    case NE_Z_FINISHED:
	/* Could argue for tolerance, and ignoring trailing content;
	 * but it could mean something more serious. */
	if (len > 0) {
	    ctx->state = NE_Z_ERROR;
	    ne_set_error(ctx->session,
			 "Unexpected content received after compressed stream");
	}
	break;

    case NE_Z_BEFORE_DATA:
	/* work out whether this is a compressed response or not. */
	if (ctx->enchdr && strcasecmp(ctx->enchdr, "gzip") == 0) {
            int ret;
	    NE_DEBUG(NE_DBG_HTTP, "compress: got gzipped stream.\n");

            /* inflateInit2() works here where inflateInit() doesn't. */
            ret = inflateInit2(&ctx->zstr, -MAX_WBITS);
            if (ret != Z_OK) {
                set_zlib_error(ctx, _("Could not initialize zlib"), ret);
                return;
            }
	    ctx->zstrinit = 1;

	} else {
	    /* No Content-Encoding header: pass it on.  TODO: we could
	     * hack it and register the real callback now. But that
	     * would require add_resp_body_rdr to have defined
	     * ordering semantics etc etc */
	    ctx->state = NE_Z_PASSTHROUGH;
	    ctx->reader(ctx->userdata, buf, len);
	    return;
	}

	ctx->state = NE_Z_IN_HEADER;
	/* FALLTHROUGH */

    case NE_Z_IN_HEADER:
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
	case HDR_EXTENDED:
	    if (len == 0)
		return;
	    break;
	case HDR_DONE:
	    if (len > 0) {
		do_inflate(ctx, buf, len);
	    }
        default:
	    return;
	}

	/* FALLTHROUGH */

    case NE_Z_POST_HEADER:
	/* eating the filename string. */
	zbuf = memchr(buf, '\0', len);
	if (zbuf == NULL) {
	    /* not found it yet. */
	    return;
	}

	NE_DEBUG(NE_DBG_HTTP,
		 "compresss: skipped %" NE_FMT_SIZE_T " header bytes.\n", 
		 zbuf - buf);
	/* found end of string. */
	len -= (1 + zbuf - buf);
	buf = zbuf + 1;
	ctx->state = NE_Z_INFLATING;
	if (len == 0) {
	    /* end of string was at end of buffer. */
	    return;
	}

	/* FALLTHROUGH */

    case NE_Z_INFLATING:
	do_inflate(ctx, buf, len);
	break;

    case NE_Z_AFTER_DATA:
	process_footer(ctx, (unsigned char *)buf, len);
	break;
    }

}

int ne_decompress_destroy(ne_decompress *ctx)
{
    int ret;

    if (ctx->zstrinit)
	/* inflateEnd only fails if it's passed NULL etc; ignore
	 * return value. */
	inflateEnd(&ctx->zstr);

    if (ctx->enchdr)
	ne_free(ctx->enchdr);

    switch (ctx->state) {
    case NE_Z_BEFORE_DATA:
    case NE_Z_PASSTHROUGH:
    case NE_Z_FINISHED:
	ret = NE_OK;
	break;
    case NE_Z_ERROR:
	/* session error already set. */
	ret = NE_ERROR;
	break;
    default:
	/* truncated response. */
	ne_set_error(ctx->session, "Compressed response was truncated");
	ret = NE_ERROR;
	break;
    }

    ne_free(ctx);
    return ret;
}

/* Prepare for a compressed response */
static void gz_pre_send(ne_request *r, void *ud, ne_buffer *req)
{
    ne_decompress *ctx = ud;

    NE_DEBUG(NE_DBG_HTTP, "compress: Initialization.\n");

    /* (Re-)Initialize the context */
    ctx->state = NE_Z_BEFORE_DATA;
    if (ctx->zstrinit) inflateEnd(&ctx->zstr);
    ctx->zstrinit = 0;
    ctx->incount = ctx->footcount = 0;
    ctx->checksum = crc32(0L, Z_NULL, 0);
    if (ctx->enchdr) {
        ne_free(ctx->enchdr);
        ctx->enchdr = NULL;
    }
}

/* Kill the pre-send hook */
static void gz_destroy(ne_request *req, void *userdata)
{
    ne_kill_pre_send(ne_get_session(req), gz_pre_send, userdata);
}

/* Wrapper for user-passed acceptor function. */
static int gz_acceptor(void *userdata, ne_request *req, const ne_status *st)
{
    ne_decompress *ctx = userdata;
    return ctx->acceptor(ctx->userdata, req, st);
}

ne_decompress *ne_decompress_reader(ne_request *req, ne_accept_response acpt,
				    ne_block_reader rdr, void *userdata)
{
    ne_decompress *ctx = ne_calloc(sizeof *ctx);

    ne_add_request_header(req, "Accept-Encoding", "gzip");

    ne_add_response_header_handler(req, "Content-Encoding", 
				   ne_duplicate_header, &ctx->enchdr);

    ne_add_response_body_reader(req, gz_acceptor, gz_reader, ctx);

    ctx->reader = rdr;
    ctx->userdata = userdata;
    ctx->session = ne_get_session(req);
    ctx->acceptor = acpt;

    ne_hook_pre_send(ctx->session, gz_pre_send, ctx);
    ne_hook_destroy_request(ctx->session, gz_destroy, ctx);

    return ctx;    
}

#else /* !NEON_ZLIB */

/* Pass-through interface present to provide ABI compatibility. */

ne_decompress *ne_decompress_reader(ne_request *req, ne_accept_response acpt,
				    ne_block_reader rdr, void *userdata)
{
    ne_add_response_body_reader(req, acpt, rdr, userdata);
    /* an arbitrary return value: don't confuse them by returning NULL. */
    return (ne_decompress *)req;
}

int ne_decompress_destroy(ne_decompress *dc)
{
    return 0;
}

#endif /* NEON_ZLIB */
