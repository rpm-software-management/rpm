/* 
   Socket handling tests
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

#include "nsocket.h"

#include "tests.h"

static int namelookup(void) {
    struct in_addr a;
    ON(sock_name_lookup("www.apache.org", &a));
    ON(sock_name_lookup("asdkajhd.webdav.org", &a) == 0);
    return OK;
}

static int svclookup(void) {
    ON(sock_service_lookup("http") != 80);
    ON(sock_service_lookup("https") != 443);
    return OK;
}

static int http(void) {
    struct in_addr a;
    nsocket *s;
    char buffer[1024];

    ON(sock_name_lookup("www.apache.org", &a));
    s = sock_connect(a, 80);
    ON(s == NULL);
    ON(sock_send_string(s, 
			"GET / HTTP/1.0\r\n"
			"Host: www.apache.org\r\n\r\n"));
    ON(sock_readline(s, buffer, 1024) < 0);
    ON(strcasecmp(buffer, "HTTP/1.1 200 OK\r\n"));
    ON(sock_close(s));
    return OK;
}

test_func tests[] = {
    namelookup,
    svclookup,
    http,
    NULL
};
    
