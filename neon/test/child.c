/* 
   Framework for testing with a server process
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

#include <sys/types.h>

#include <sys/wait.h>
#include <sys/socket.h>

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <signal.h>

#include "ne_socket.h"

#include "tests.h"
#include "child.h"

static pid_t child = 0;

static struct in_addr lh_addr;

int lookup_localhost(void)
{
    if (sock_name_lookup("localhost", &lh_addr))
	return FAILHARD;
    
    return OK;
}

static nsocket *server_socket(int port)
{
    int ls = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    nsocket *s;
    int val = 1;

    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (void *)&val, sizeof(int));
    
    addr.sin_addr = lh_addr;
    addr.sin_port = htons(port);

    if (bind(ls, (struct sockaddr *)&addr, sizeof(addr))) {
	printf("bind failed: %s\n", strerror(errno));
	return NULL;
    }
    if (listen(ls, 5)) {
	printf("listen failed: %s\n", strerror(errno));
	return NULL;
    }

    s = sock_accept(ls);
    close(ls);
    
    return s;
}

static void minisleep(void)
{
    sleep(1);
}

/* This runs as the child process. */
static int server_child(int port, server_fn callback, void *userdata)
{
    nsocket *s;
    int ret;

    in_child();

    s = server_socket(port);

    if (s == NULL)
	return FAIL;

    ret = callback(s, userdata);

    sock_close(s);

    return ret;
}


int spawn_server(int port, server_fn fn, void *ud)
{
    child = fork();
    
    ONN("fork server", child == -1);

    if (child == 0) {
	/* this is the child. */
	int ret = server_child(port, fn, ud);

	/* print the error out otherwise it gets lost. */
	if (ret) {
	    printf("server child failed: %s\n", name);
	}

	/* and quit the child. */
	exit(ret);
    } else {
	/* this is the parent... sleep for a bit to let the parent
	 * get itself into gear. */
	minisleep();
	return OK;
    }
}

int await_server(void)
{
    int status;

    (void) wait(&status);
    
    i_am("error from server process");

    /* so that we aren't reaped by mistake. */
    child = 0;

    return WEXITSTATUS(status);
}

int reap_server(void)
{
    int status;
    
    if (child != 0) {
	(void) kill(child, SIGTERM);
	minisleep();
	(void) wait(&status);
    }

    return OK;
}
