/* 
   utils tests
   Copyright (C) 2001, Joe Orton <joe@manyfish.co.uk>

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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "http_utils.h"
#include "neon_md5.h"
#include "base64.h"
#include "tests.h"

static const struct {
    const char *status;
    int major, minor, code;
    const char *rp;
} accept_sl[] = {
    /* These are really valid. */
    { "HTTP/1.1 200 OK", 1, 1, 200, "OK" },
    { "HTTP/1.1000 200 OK", 1, 1000, 200, "OK" },
    { "HTTP/1000.1000 200 OK", 1000, 1000, 200, "OK" },
    { "HTTP/00001.1 200 OK", 1, 1, 200, "OK" },
    { "HTTP/1.00001 200 OK", 1, 1, 200, "OK" },
    { "HTTP/99.99 999 99999", 99, 99, 999, "99999" },
    /* these aren't really valid but we should be able to parse them. */
    { "HTTP/1.1   200   OK", 1, 1, 200, "OK" },
    { "   HTTP/1.1 200 OK", 1, 1, 200, "OK" },
    { "Norman is a dog HTTP/1.1 200 OK", 1, 1, 200, "OK" },
    { NULL }
};

static const char *bad_sl[] = {
    "",
    "HTTP/1.1 1000 OK",
    "HTTP/1.1 1000",
    "HTTP/1.1 100",
    "HTTP/ 200 OK",
    NULL
};  

static int status_lines(void)
{
    http_status s;
    int n;
    char buf[50], *pnt;

    for (n = 0; accept_sl[n].status != NULL; n++) {
	sprintf(buf, "valid %d: ", n);
	pnt = buf + strlen(buf);
	strcpy(pnt, "parse");
	ONN(buf, http_parse_statusline(accept_sl[n].status, &s));
	strcpy(pnt, "major");
	ONN(buf, accept_sl[n].major != s.major_version);
	strcpy(pnt, "minor");
	ONN(buf, accept_sl[n].minor != s.minor_version);
	strcpy(pnt, "code");
	ONN(buf, accept_sl[n].code != s.code);
	strcpy(pnt, "reason-phrase");
	ONN(buf, strcmp(accept_sl[n].rp, s.reason_phrase));
    }
    
    for (n = 0; bad_sl[n] != NULL; n++) {
	sprintf(buf, "invalid %d", n);
	ONN(buf, http_parse_statusline(bad_sl[n], &s) == 0);
    }

    return OK;
}

static int md5(void)
{
    unsigned char buf[17] = {0}, buf2[17] = {0};
    char ascii[33] = {0};

    ne_md5_to_ascii(ne_md5_buffer("", 0, buf), ascii);
    ONN("MD5(null)", strcmp(ascii, "d41d8cd98f00b204e9800998ecf8427e"));
    
    ne_md5_to_ascii(ne_md5_buffer("foobar", 7, buf), ascii);
    ONN("MD5(foobar)", strcmp(ascii, "b4258860eea29e875e2ee4019763b2bb"));

    ne_ascii_to_md5(ascii, buf2);

    ON(memcmp(buf, buf2, 16));

    return 0;
}

static int base64(void)
{
#define B64(x, y) \
do { char *_b = ne_base64(x); ON(strcmp(_b, y)); free(_b); } while (0)

    /* invent these with 
     *  $ printf "string" | uuencode -m blah
     */
    B64("a", "YQ==");
    B64("bb", "YmI=");
    B64("ccc", "Y2Nj");
    B64("Hello, world", "SGVsbG8sIHdvcmxk");
    B64("I once saw a dog called norman.\n", 
	"SSBvbmNlIHNhdyBhIGRvZyBjYWxsZWQgbm9ybWFuLgo=");
#if 0
    /* duh, base64() doesn't handle binary data. which moron wrote
     * that then? add a length argument. */
    B64("\0\0\0\0\0", "AAAAAAAK");
#endif

#undef B64
    return OK;
}

test_func tests[] = {
    status_lines,
    md5,
    base64,
    NULL
};
