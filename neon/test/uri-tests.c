/* 
   URI tests
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

#include <uri.h>

#include "tests.h"

struct test_uris {
    const char *uri;
    const char *scheme, *hostname;
    int port;
    const char *path;
} uritests[] = {
    { "http://webdav.org/norman", "http:", "webdav.org", 80, "/norman" },
    { "https://webdav.org/foo", "https:", "webdav.org", 443, "/foo" },
    { "http://a/b", "a:", "a", 80, "/b" },
    { NULL }
};

static int simple(void)
{
    struct uri p = {0};
    ON(uri_parse("http://www.webdav.org/foo", &p, NULL));
    ON(strcmp(p.host, "www.webdav.org"));
    ON(strcmp(p.path, "/foo"));
    ON(strcmp(p.scheme, "http"));
    ON(p.port != -1);
    ON(p.authinfo != NULL);
    return 0;
}

static int simple_ssl(void)
{
    struct uri p = {0};
    ON(uri_parse("https://webdav.org/", &p, NULL));
    ON(strcmp(p.scheme, "https"));
    ON(p.port != -1);
    return OK;
}

static int no_path(void)
{
    struct uri p = {0};
    ON(uri_parse("https://webdav.org", &p, NULL));
    ON(strcmp(p.path, "/"));
    return OK;
}

static int authinfo(void)
{
    struct uri p = {0};
    ON(uri_parse("ftp://jim:bob@jim.com", &p, NULL));
    ON(strcmp(p.host, "jim.com"));
    ON(strcmp(p.authinfo, "jim:bob"));
    return OK;
}

#define STR "/a¹²³¼½/"
static int escapes(void)
{
    char *un, *esc;
    esc = uri_abspath_escape(STR);
    ON(esc == NULL);
    un = uri_unescape(esc);
    ON(un == NULL);
    ON(strcmp(un, STR));
    free(un);
    free(esc);
    return OK;
}    

static int parents(void)
{
    char *p = uri_parent("/a/b/c");
    ON(p == NULL);
    ON(strcmp(p, "/a/b/"));
    free(p);
    p = uri_parent("/a/b/c/");
    ON(p == NULL);
    ON(strcmp(p, "/a/b/"));
    free(p);
    return OK;
}

static int abspath(void)
{
    const char *p = uri_abspath("http://norman:1234/a/b");
    ON(p == NULL);
    ON(strcmp(p, "/a/b"));
    return OK;
}

static int compares(void)
{
    ON(uri_compare("/a", "/a/") != 0);
    ON(uri_compare("/a/", "/a") != 0);
    ON(uri_compare("/ab", "/a/") == 0);
    ON(uri_compare("/a/", "/ab") == 0);
    ON(uri_compare("/a/", "/a/") != 0);
    ON(uri_compare("/a/b/c/d", "/a/b/c/") == 0);
    return OK;
}       

static int children(void)
{
    ON(uri_childof("/a", "/a/b") == 0);
    ON(uri_childof("/a/", "/a/b") == 0);
    ON(uri_childof("/aa/b/c", "/a/b/c/d/e") != 0);
    ON(uri_childof("////", "/a") != 0);
    return OK;
}

static int slash(void)
{
    ON(uri_has_trailing_slash("/a/") == 0);
    ON(uri_has_trailing_slash("/a") != 0);
    {
	/* check the uri == "" case. */
	char *foo = "/";
	ON(uri_has_trailing_slash(&foo[1]));
    }
    return OK;
}

static int just_hostname(void)
{
    struct uri p = {0};
    ON(uri_parse("host.name.com", &p, NULL));
    ON(strcmp(p.host, "host.name.com"));
    return 0;
}

static int just_path(void)
{
    struct uri p = {0};
    ON(uri_parse("/argh", &p, NULL));
    ON(strcmp(p.path, "/argh"));
    return 0;
}

static int null_uri(void) {
    struct uri p = {0};
    ON(uri_parse("", &p, NULL) == 0);
    return 0;
}

test_func tests[] = {
    simple,
    simple_ssl,
    authinfo,
    no_path,
    escapes,
    parents,
    abspath,
    compares,
    children,
    slash,
    just_hostname,
    just_path,
    null_uri,
    NULL
};
