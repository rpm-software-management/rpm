/* 
   HTTP server tests
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

/* These don't really test neon, they test an HTTP server.  */

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "nsocket.h"
#include "ne_utils.h"

#include "tests.h"

struct in_addr addr;
nsocket *sock;

char buffer[BUFSIZ];
int clength;

#define HOSTNAME "localhost"

static int hostname_lookup(void) {
    if (sock_name_lookup(HOSTNAME, &addr))
	return FAILHARD;
    return OK;
}

static int connect_to_server(void) {
    sock = sock_connect(addr, 80);
    if (sock == NULL)
	return FAILHARD;
    return OK;
}

static int close_conn(void) {
    ON(sock_close(sock));
    return OK;
}

#define EOL "\r\n"

#define TRACE_REQ "TRACE / HTTP/1.0\r\n" "Host: " HOSTNAME "\r\n\r\n"

#define ROOT_URL "/test/"

#define GET_REQ(url) do { ON(sock_send_string(sock, "GET " url " HTTP/1.0" EOL "Host: " HOSTNAME EOL EOL)); } while (0)

static int trace(void) {
    CALL(connect_to_server());

    ON(sock_send_string(sock, TRACE_REQ));
    ON(sock_readline(sock, buffer, 1024) < 0);
    /* this is true for any Apache server. */
    ON(strcasecmp(buffer, "HTTP/1.1 200 OK\r\n"));
    /* read hdrs */
    do {
	ON(sock_readline(sock, buffer, 1024) < 0);
    } while (strcmp(buffer, "\r\n") != 0);
    /* read body */
    ON(sock_read(sock, buffer, 1024) < 0);
    /* this will fail if you have a transparent proxy. */
    ONN("pristine TRACE loopback", 
	strncmp(buffer, TRACE_REQ, strlen(TRACE_REQ)) != 0);

    CALL(close_conn());

    return OK;
}

static int basic_syntax(void) {
    char *value;
    
    CALL(connect_to_server());
    GET_REQ(ROOT_URL);
    ON(sock_readline(sock, buffer, 1024) < 0);
    ONN("CRLF on status-line", strstr(buffer, EOL) == NULL);
    
    ONN("HTTP-Version is 1.0 or 1.1",
	strncasecmp(buffer, "HTTP/1.1", 8) != 0 &&
	strncasecmp(buffer, "HTTP/1.0", 8));

    ONN("Status-Line syntax", buffer[8] != ' ' || buffer[12] != ' ');	

    /* should be a warning if this fails. */
    ONN("Status-Code is 200", 
	buffer[9] != '2' || buffer[10] != '0' || buffer[11] != '0');

    ONN("Reason-Phrase is present", strlen(buffer) < strlen("HTTP/x.y nnn X" EOL));

    do {
	ON(sock_readline(sock, buffer, 1024) < 0);
	ONN("CRLF on request-header", strstr(buffer, EOL) == NULL);
	if (strcmp(buffer, EOL) != 0) {
	    value = strchr(buffer, ':');
	    ONN("colon in request-header", value == NULL);
	    ONN("field-name in request-header", value == buffer);
	}
    } while (strcmp(buffer, EOL) != 0);

    CALL(close_conn());

    return OK;
}

static int skip_header(void)
{
    do {
	ON(sock_readline(sock, buffer, 1024) < 0);
	if (strncasecmp(buffer, "content-length:", 15) == 0) {
	    clength = atoi(buffer + 16);
	}
    } while (strcmp(buffer, EOL) != 0);
    return OK;
}

static int simple_get(void)
{
    CALL(connect_to_server());
    GET_REQ(ROOT_URL "plain");
    clength = -1;
    CALL(skip_header());
    ONN("Content-Length header present", clength == -1);
    ONN("Content-Length of 'plain'", clength != 11);
    ONN("read response body", sock_read(sock, buffer, BUFSIZ) != 11);
    ONN("content of 'plain'", strncmp(buffer, "Test file.\n", 11) != 0);

    /* FIXME: I'm not sure if this is right, actually. */
    ONN("connection close after GET",
	sock_peek(sock, buffer, BUFSIZ) != SOCK_CLOSED);

    CALL(close_conn());
    return OK;
}

static int simple_head(void)
{
    CALL(connect_to_server());
    ON(sock_send_string(sock, "HEAD " ROOT_URL "plain HTTP/1.0" EOL
			"Host: " HOSTNAME EOL EOL));
    clength = -1;
    CALL(skip_header());
    ONN("Content-Length header present", clength == -1);
    ONN("Content-Length of 'plain'", clength != 11);

    ONN("connection close after HEAD",
	sock_peek(sock, buffer, BUFSIZ) != SOCK_CLOSED);
    
    CALL(close_conn());
    return OK;    
}

static int null_resource(void)
{
    ne_status s = {0};
    CALL(connect_to_server());
    GET_REQ(ROOT_URL "nothing-here");
    ON(sock_readline(sock, buffer, BUFSIZ) < 0);
    ON(ne_parse_statusline(buffer, &s));
    ONN("null resource gives 404", s.code != 404);
    CALL(close_conn());
    return OK;
}

test_func tests[] = {
    hostname_lookup,
    basic_syntax,
    trace,
    simple_get,
    simple_head,
    null_resource,
    NULL
};
