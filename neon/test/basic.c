/* 
   neon test suite
   Copyright (C) 2002, Joe Orton <joe@manyfish.co.uk>

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

#include "ne_basic.h"

#include "tests.h"
#include "child.h"
#include "utils.h"

static int content_type(void)
{
    int n;
    struct {
	const char *value, *type, *subtype, *charset;
    } ctypes[] = {
	{ "text/plain", "text", "plain", NULL },
	{ "text/plain  ", "text", "plain", NULL },
	{ "application/xml", "application", "xml", NULL },
#undef TXU
#define TXU "text", "xml", "utf-8"
	/* 2616 doesn't *say* that charset can be quoted, but bets are
	 * that some servers do it anyway. */
	{ "text/xml; charset=utf-8", TXU },
	{ "text/xml; charset=utf-8; foo=bar", TXU },
	{ "text/xml;charset=utf-8", TXU },
	{ "text/xml ;charset=utf-8", TXU },
	{ "text/xml;charset=utf-8;foo=bar", TXU },
	{ "text/xml; foo=bar; charset=utf-8", TXU },
	{ "text/xml; foo=bar; charset=utf-8; bar=foo", TXU },
	{ "text/xml; charset=\"utf-8\"", TXU },
	{ "text/xml; charset='utf-8'", TXU },
	{ "text/xml; foo=bar; charset=\"utf-8\"; bar=foo", TXU },
#undef TXU
	/* badly quoted charset should come out as NULL */
	{ "text/xml; charset=\"utf-8", "text", "xml", NULL },
	{ NULL }
    };

    for (n = 0; ctypes[n].value != NULL; n++) {
	ne_content_type ct = {0};

	ne_content_type_handler(&ct, ctypes[n].value);
	
	ONV(strcmp(ct.type, ctypes[n].type),
	    ("for `%s': type was `%s'", ctypes[n].value, ct.type));

	ONV(strcmp(ct.subtype, ctypes[n].subtype),
	    ("for `%s': subtype was `%s'", ctypes[n].value, ct.subtype));

	ONV(ctypes[n].charset && ct.charset == NULL,
	    ("for `%s': charset unset", ctypes[n].value));
	
	ONV(ctypes[n].charset == NULL && ct.charset != NULL,
	    ("for `%s': unexpected charset `%s'", ctypes[n].value, 
	     ct.charset));

	ONV(ctypes[n].charset && ct.charset &&
	    strcmp(ctypes[n].charset, ct.charset),
	    ("for `%s': charset was `%s'", ctypes[n].value, ct.charset));

	NE_FREE(ct.value);
    }

    return OK;
}

ne_test tests[] = {
    T(content_type), /* test functions here */

    /* end of test functions. */
    T(NULL) 
};

